#pragma once
#include <string>

namespace banking::security {

class EncryptionUtil {
public:
    // Encrypt plaintext using AES-256-GCM and return a Base64-encoded packed string (IV + TAG + CIPHERTEXT)
    static std::string encrypt(const std::string& plaintext, const std::string& key);

    // Decrypt a Base64-encoded packed string (IV + TAG + CIPHERTEXT) using AES-256-GCM
    static std::string decrypt(const std::string& packedBase64, const std::string& key);

private:
    // Helper to encode string to standard Base64
    static std::string base64Encode(const std::string& input);

    // Helper to decode string from standard Base64
    static std::string base64Decode(const std::string& input);
};

} // namespace banking::security
