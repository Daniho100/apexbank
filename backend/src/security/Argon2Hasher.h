#pragma once
#include <string>

namespace banking::security {

class Argon2Hasher {
public:
    // Generate a secure random salt (hex representation)
    static std::string generateSalt(size_t length = 16);

    // Hash password with Argon2id using a salt and optional pepper
    static std::string hashPassword(const std::string& password, const std::string& salt, const std::string& pepper = "");

    // Verify password against Argon2id hash using pepper
    static bool verifyPassword(const std::string& encodedHash, const std::string& password, const std::string& pepper = "");
};

} // namespace banking::security
