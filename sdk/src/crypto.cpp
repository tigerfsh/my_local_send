#include "localsend/crypto.h"

#include <openssl/bio.h>
#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/md5.h>
#include <openssl/pem.h>
#include <openssl/rand.h>
#include <openssl/rsa.h>

#include <cstring>
#include <fstream>
#include <memory>
#include <vector>

namespace localsend {

namespace {

struct EvpKeyCloser {
  void operator()(EVP_PKEY* p) const { if (p) EVP_PKEY_free(p); }
};
struct EvpCtxCloser {
  void operator()(EVP_PKEY_CTX* p) const { if (p) EVP_PKEY_CTX_free(p); }
};
struct CipherCtxCloser {
  void operator()(EVP_CIPHER_CTX* p) const { if (p) EVP_CIPHER_CTX_free(p); }
};
struct MdCtxCloser {
  void operator()(EVP_MD_CTX* p) const { if (p) EVP_MD_CTX_free(p); }
};

using EvpKeyPtr = std::unique_ptr<EVP_PKEY, EvpKeyCloser>;
using EvpCtxPtr = std::unique_ptr<EVP_PKEY_CTX, EvpCtxCloser>;
using CipherCtxPtr = std::unique_ptr<EVP_CIPHER_CTX, CipherCtxCloser>;
using MdCtxPtr = std::unique_ptr<EVP_MD_CTX, MdCtxCloser>;

bool pemToKey(const std::string& pem, bool isPublic, EVP_PKEY** out) {
  BIO* bio = BIO_new_mem_buf(pem.data(), static_cast<int>(pem.size()));
  if (!bio) return false;
  EVP_PKEY* key = isPublic ? PEM_read_bio_PUBKEY(bio, nullptr, nullptr, nullptr)
                           : PEM_read_bio_PrivateKey(bio, nullptr, nullptr, nullptr);
  BIO_free(bio);
  if (!key) return false;
  *out = key;
  return true;
}

} // namespace

bool Crypto::generateRsaKeyPair(std::string& publicPem, std::string& privatePem, int bits) {
  EvpCtxPtr ctx(EVP_PKEY_CTX_new_id(EVP_PKEY_RSA, nullptr));
  if (!ctx) return false;
  if (EVP_PKEY_keygen_init(ctx.get()) <= 0) return false;
  if (EVP_PKEY_CTX_set_rsa_keygen_bits(ctx.get(), bits) <= 0) return false;
  EVP_PKEY* keyRaw = nullptr;
  if (EVP_PKEY_keygen(ctx.get(), &keyRaw) <= 0) return false;
  EvpKeyPtr key(keyRaw);

  BIO* pubBio = BIO_new(BIO_s_mem());
  BIO* privBio = BIO_new(BIO_s_mem());
  if (!pubBio || !privBio) {
    if (pubBio) BIO_free(pubBio);
    if (privBio) BIO_free(privBio);
    return false;
  }
  bool ok = PEM_write_bio_PUBKEY(pubBio, key.get()) == 1 &&
            PEM_write_bio_PrivateKey(privBio, key.get(), nullptr, nullptr, 0, nullptr, nullptr) == 1;

  char* p = nullptr;
  long len = BIO_get_mem_data(pubBio, &p);
  if (len > 0 && p) publicPem.assign(p, static_cast<size_t>(len));
  len = BIO_get_mem_data(privBio, &p);
  if (len > 0 && p) privatePem.assign(p, static_cast<size_t>(len));
  BIO_free(pubBio);
  BIO_free(privBio);
  return ok;
}

bool Crypto::rsaEncrypt(const std::string& publicPem, const std::vector<uint8_t>& in, std::vector<uint8_t>& out) {
  EVP_PKEY* keyRaw = nullptr;
  if (!pemToKey(publicPem, true, &keyRaw)) return false;
  EvpKeyPtr key(keyRaw);

  EvpCtxPtr ctx(EVP_PKEY_CTX_new(key.get(), nullptr));
  if (!ctx) return false;
  if (EVP_PKEY_encrypt_init(ctx.get()) <= 0) return false;
  if (EVP_PKEY_CTX_set_rsa_padding(ctx.get(), RSA_PKCS1_OAEP_PADDING) <= 0) return false;
  if (EVP_PKEY_CTX_set_rsa_oaep_md(ctx.get(), EVP_sha256()) <= 0) return false;

  size_t outLen = 0;
  if (EVP_PKEY_encrypt(ctx.get(), nullptr, &outLen, in.data(), in.size()) <= 0) return false;
  out.resize(outLen);
  if (EVP_PKEY_encrypt(ctx.get(), out.data(), &outLen, in.data(), in.size()) <= 0) return false;
  out.resize(outLen);
  return true;
}

bool Crypto::rsaDecrypt(const std::string& privatePem, const std::vector<uint8_t>& in, std::vector<uint8_t>& out) {
  EVP_PKEY* keyRaw = nullptr;
  if (!pemToKey(privatePem, false, &keyRaw)) return false;
  EvpKeyPtr key(keyRaw);

  EvpCtxPtr ctx(EVP_PKEY_CTX_new(key.get(), nullptr));
  if (!ctx) return false;
  if (EVP_PKEY_decrypt_init(ctx.get()) <= 0) return false;
  if (EVP_PKEY_CTX_set_rsa_padding(ctx.get(), RSA_PKCS1_OAEP_PADDING) <= 0) return false;
  if (EVP_PKEY_CTX_set_rsa_oaep_md(ctx.get(), EVP_sha256()) <= 0) return false;

  size_t outLen = 0;
  if (EVP_PKEY_decrypt(ctx.get(), nullptr, &outLen, in.data(), in.size()) <= 0) return false;
  out.resize(outLen);
  if (EVP_PKEY_decrypt(ctx.get(), out.data(), &outLen, in.data(), in.size()) <= 0) return false;
  out.resize(outLen);
  return true;
}

bool Crypto::rsaSign(const std::string& privatePem, const std::vector<uint8_t>& data, std::vector<uint8_t>& sig) {
  EVP_PKEY* keyRaw = nullptr;
  if (!pemToKey(privatePem, false, &keyRaw)) return false;
  EvpKeyPtr key(keyRaw);

  MdCtxPtr mctx(EVP_MD_CTX_new());
  if (!mctx) return false;
  if (EVP_DigestSignInit(mctx.get(), nullptr, EVP_sha256(), nullptr, key.get()) != 1) return false;
  size_t sigLen = 0;
  if (EVP_DigestSign(mctx.get(), nullptr, &sigLen, data.data(), data.size()) != 1) return false;
  sig.resize(sigLen);
  if (EVP_DigestSign(mctx.get(), sig.data(), &sigLen, data.data(), data.size()) != 1) return false;
  sig.resize(sigLen);
  return true;
}

bool Crypto::rsaVerify(const std::string& publicPem, const std::vector<uint8_t>& data, const std::vector<uint8_t>& sig) {
  EVP_PKEY* keyRaw = nullptr;
  if (!pemToKey(publicPem, true, &keyRaw)) return false;
  EvpKeyPtr key(keyRaw);

  MdCtxPtr mctx(EVP_MD_CTX_new());
  if (!mctx) return false;
  if (EVP_DigestVerifyInit(mctx.get(), nullptr, EVP_sha256(), nullptr, key.get()) != 1) return false;
  return EVP_DigestVerify(mctx.get(), sig.data(), sig.size(), data.data(), data.size()) == 1;
}

void Crypto::generateAesKey(std::vector<uint8_t>& key, size_t len) {
  key.resize(len);
  RAND_bytes(key.data(), static_cast<int>(len));
}

bool Crypto::aesGcmEncrypt(const std::vector<uint8_t>& key, const std::vector<uint8_t>& iv,
                           const uint8_t* data, size_t len,
                           std::vector<uint8_t>& out, std::vector<uint8_t>& tag) {
  if (key.size() != 16) return false;
  CipherCtxPtr ctx(EVP_CIPHER_CTX_new());
  if (!ctx) return false;
  if (EVP_EncryptInit_ex(ctx.get(), EVP_aes_128_gcm(), nullptr, nullptr, nullptr) != 1) return false;
  if (EVP_CIPHER_CTX_ctrl(ctx.get(), EVP_CTRL_GCM_SET_IVLEN, static_cast<int>(iv.size()), nullptr) != 1) return false;
  if (EVP_EncryptInit_ex(ctx.get(), nullptr, nullptr, key.data(), iv.data()) != 1) return false;
  out.resize(len);
  int outLen = 0;
  if (EVP_EncryptUpdate(ctx.get(), out.data(), &outLen, data, static_cast<int>(len)) != 1) return false;
  int finLen = 0;
  if (EVP_EncryptFinal_ex(ctx.get(), out.data() + outLen, &finLen) != 1) return false;
  out.resize(static_cast<size_t>(outLen + finLen));
  tag.resize(16);
  if (EVP_CIPHER_CTX_ctrl(ctx.get(), EVP_CTRL_GCM_GET_TAG, 16, tag.data()) != 1) return false;
  return true;
}

bool Crypto::aesGcmDecrypt(const std::vector<uint8_t>& key, const std::vector<uint8_t>& iv,
                           const uint8_t* data, size_t len, const std::vector<uint8_t>& tag,
                           std::vector<uint8_t>& out) {
  if (key.size() != 16 || tag.size() != 16) return false;
  CipherCtxPtr ctx(EVP_CIPHER_CTX_new());
  if (!ctx) return false;
  if (EVP_DecryptInit_ex(ctx.get(), EVP_aes_128_gcm(), nullptr, nullptr, nullptr) != 1) return false;
  if (EVP_CIPHER_CTX_ctrl(ctx.get(), EVP_CTRL_GCM_SET_IVLEN, static_cast<int>(iv.size()), nullptr) != 1) return false;
  if (EVP_DecryptInit_ex(ctx.get(), nullptr, nullptr, key.data(), iv.data()) != 1) return false;
  out.resize(len);
  int outLen = 0;
  if (EVP_DecryptUpdate(ctx.get(), out.data(), &outLen, data, static_cast<int>(len)) != 1) return false;
  if (EVP_CIPHER_CTX_ctrl(ctx.get(), EVP_CTRL_GCM_SET_TAG, 16, const_cast<uint8_t*>(tag.data())) != 1) return false;
  int finLen = 0;
  if (EVP_DecryptFinal_ex(ctx.get(), out.data() + outLen, &finLen) != 1) return false;
  out.resize(static_cast<size_t>(outLen + finLen));
  return true;
}

std::string Crypto::md5(const uint8_t* data, size_t len) {
  MdCtxPtr ctx(EVP_MD_CTX_new());
  unsigned char digest[EVP_MAX_MD_SIZE];
  unsigned int digestLen = 0;
  if (!ctx) return "";
  if (EVP_DigestInit_ex(ctx.get(), EVP_md5(), nullptr) != 1) return "";
  if (EVP_DigestUpdate(ctx.get(), data, len) != 1) return "";
  if (EVP_DigestFinal_ex(ctx.get(), digest, &digestLen) != 1) return "";
  char hex[33];
  for (unsigned int i = 0; i < digestLen; ++i) {
    std::snprintf(hex + i * 2, 3, "%02x", digest[i]);
  }
  return std::string(hex, digestLen * 2);
}

std::string Crypto::md5File(const std::string& path) {
  std::ifstream in(path, std::ios::binary);
  if (!in) return "";
  MdCtxPtr ctx(EVP_MD_CTX_new());
  if (!ctx) return "";
  if (EVP_DigestInit_ex(ctx.get(), EVP_md5(), nullptr) != 1) return "";
  std::vector<char> buf(1024 * 1024);
  while (in) {
    in.read(buf.data(), static_cast<std::streamsize>(buf.size()));
    std::streamsize got = in.gcount();
    if (got > 0 && EVP_DigestUpdate(ctx.get(), buf.data(), static_cast<size_t>(got)) != 1) return "";
    if (in.eof()) break;
  }
  unsigned char digest[EVP_MAX_MD_SIZE];
  unsigned int digestLen = 0;
  if (EVP_DigestFinal_ex(ctx.get(), digest, &digestLen) != 1) return "";
  char hex[33];
  for (unsigned int i = 0; i < digestLen; ++i) {
    std::snprintf(hex + i * 2, 3, "%02x", digest[i]);
  }
  return std::string(hex, digestLen * 2);
}

std::string Crypto::toBase64(const std::vector<uint8_t>& data) {
  if (data.empty()) return "";
  int encodedLen = 4 * static_cast<int>((data.size() + 2) / 3);
  std::vector<char> buf(static_cast<size_t>(encodedLen + 1));
  int len = EVP_EncodeBlock(reinterpret_cast<unsigned char*>(buf.data()), data.data(),
                            static_cast<int>(data.size()));
  return std::string(buf.data(), static_cast<size_t>(len));
}

std::vector<uint8_t> Crypto::fromBase64(const std::string& data) {
  if (data.empty()) return {};
  std::string cleaned;
  cleaned.reserve(data.size());
  for (char c : data) {
    if (c != '\n' && c != '\r' && c != ' ' && c != '\t') cleaned.push_back(c);
  }
  std::vector<uint8_t> buf(cleaned.size() / 4 * 3 + 3);
  int len = EVP_DecodeBlock(buf.data(),
                            reinterpret_cast<const unsigned char*>(cleaned.data()),
                            static_cast<int>(cleaned.size()));
  if (len < 0) return {};
  while (!cleaned.empty() && cleaned.back() == '=') {
    cleaned.pop_back();
    --len;
  }
  buf.resize(static_cast<size_t>(len));
  return buf;
}

} // namespace localsend
