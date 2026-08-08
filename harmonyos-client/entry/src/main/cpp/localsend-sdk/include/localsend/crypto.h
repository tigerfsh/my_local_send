#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "export.h"

namespace localsend {

class LOCALSEND_API Crypto {
public:
  static bool generateRsaKeyPair(std::string& publicPem, std::string& privatePem, int bits = 2048);
  static bool rsaEncrypt(const std::string& publicPem, const std::vector<uint8_t>& in, std::vector<uint8_t>& out);
  static bool rsaDecrypt(const std::string& privatePem, const std::vector<uint8_t>& in, std::vector<uint8_t>& out);

  static void generateAesKey(std::vector<uint8_t>& key, size_t len = 16);
  static bool aesGcmEncrypt(const std::vector<uint8_t>& key, const std::vector<uint8_t>& iv,
                            const uint8_t* data, size_t len,
                            std::vector<uint8_t>& out, std::vector<uint8_t>& tag);
  static bool aesGcmDecrypt(const std::vector<uint8_t>& key, const std::vector<uint8_t>& iv,
                            const uint8_t* data, size_t len, const std::vector<uint8_t>& tag,
                            std::vector<uint8_t>& out);

  static std::string md5(const uint8_t* data, size_t len);
  static std::string md5File(const std::string& path);

  static std::string toBase64(const std::vector<uint8_t>& data);
  static std::vector<uint8_t> fromBase64(const std::string& data);
};

} // namespace localsend
