#include "Argon2Hasher.h"
#include <argon2.h>
#include <openssl/rand.h>
#include <iomanip>
#include <sstream>
#include <vector>
#include <stdexcept>
#include <iostream>

namespace banking::security {

std::string Argon2Hasher::generateSalt(size_t length) {
    std::vector<unsigned char> buffer(length);
    if (RAND_bytes(buffer.data(), length) != 1) {
        throw std::runtime_error("Failed to generate secure random salt using OpenSSL");
    }
    
    std::stringstream ss;
    for (unsigned char b : buffer) {
        ss << std::hex << std::setw(2) << std::setfill('0') << (int)b;
    }
    return ss.str();
}

std::string Argon2Hasher::hashPassword(const std::string& password, const std::string& salt, const std::string& pepper) {
    // Combine password and application pepper
    std::string input = password + pepper;
    
    // Hash parameters compliant with OWASP: 3 iterations, 64MB memory, 4 parallelism
    uint32_t t_cost = 3;
    uint32_t m_cost = 65536; // 64MB
    uint32_t parallelism = 4;
    
    // Allocate space for encoded hash string
    // Standard encoded string length is usually < 120, we use 256 to be absolutely safe
    std::vector<char> encoded(256, 0);
    
    int result = argon2id_hash_encoded(
        t_cost,
        m_cost,
        parallelism,
        input.data(),
        input.size(),
        salt.data(),
        salt.size(),
        32,
        encoded.data(),
        encoded.size()
    );
    
    if (result != ARGON2_OK) {
        throw std::runtime_error("Argon2id hashing failed: " + std::string(argon2_error_message(result)));
    }
    
    return std::string(encoded.data());
}

bool Argon2Hasher::verifyPassword(const std::string& encodedHash, const std::string& password, const std::string& pepper) {
    std::string input = password + pepper;
    
    int result = argon2id_verify(
        encodedHash.c_str(),
        input.data(),
        input.size()
    );
    
    return result == ARGON2_OK;
}

} // namespace banking::security
