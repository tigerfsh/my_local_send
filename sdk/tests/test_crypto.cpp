#include <cstdint>
#include <string>
#include <vector>

#include "localsend/crypto.h"
#include "test_util.h"

using namespace localsend;

static void run() {
  // RSA roundtrip
  std::string pub, priv;
  CHECK(Crypto::generateRsaKeyPair(pub, priv));
  CHECK(!pub.empty());
  CHECK(!priv.empty());
  CHECK(pub.find("BEGIN PUBLIC KEY") != std::string::npos);
  CHECK(priv.find("BEGIN PRIVATE KEY") != std::string::npos);

  std::string secret = "session-aes-key-material";
  std::vector<uint8_t> plain(secret.begin(), secret.end());
  std::vector<uint8_t> cipher, dec;
  CHECK(Crypto::rsaEncrypt(pub, plain, cipher));
  CHECK(!cipher.empty());
  CHECK(Crypto::rsaDecrypt(priv, cipher, dec));
  CHECK_EQ(std::string(dec.begin(), dec.end()), secret);

  // wrong key must fail
  std::string pub2, priv2;
  Crypto::generateRsaKeyPair(pub2, priv2);
  std::vector<uint8_t> bad;
  CHECK(!Crypto::rsaDecrypt(priv2, cipher, bad));

  // AES-128-GCM roundtrip
  std::vector<uint8_t> key;
  Crypto::generateAesKey(key);
  CHECK_EQ(key.size(), static_cast<size_t>(16));
  std::vector<uint8_t> iv = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12};
  std::string payload = "hello localsend file chunk payload";
  std::vector<uint8_t> out, tag;
  CHECK(Crypto::aesGcmEncrypt(key, iv, reinterpret_cast<const uint8_t*>(payload.data()), payload.size(), out, tag));
  CHECK_EQ(tag.size(), static_cast<size_t>(16));
  std::vector<uint8_t> recovered;
  CHECK(Crypto::aesGcmDecrypt(key, iv, out.data(), out.size(), tag, recovered));
  CHECK_EQ(std::string(recovered.begin(), recovered.end()), payload);

  // tampered data must fail auth
  std::vector<uint8_t> tampered = out;
  if (!tampered.empty()) tampered[0] ^= 0xFF;
  std::vector<uint8_t> badDec;
  CHECK(!Crypto::aesGcmDecrypt(key, iv, tampered.data(), tampered.size(), tag, badDec));

  // MD5
  std::string msg = "abc";
  CHECK_EQ(Crypto::md5(reinterpret_cast<const uint8_t*>(msg.data()), msg.size()),
           std::string("900150983cd24fb0d6963f7d28e17f72"));

  // base64
  std::vector<uint8_t> b64data = {0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99};
  std::string b64 = Crypto::toBase64(b64data);
  CHECK(!b64.empty());
  auto back = Crypto::fromBase64(b64);
  CHECK(back == b64data);
}

TEST_MAIN(run)
