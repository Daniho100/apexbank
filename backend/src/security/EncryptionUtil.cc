#include "EncryptionUtil.h"
#include <openssl/evp.h>
#include <openssl/rand.h>
#include <vector>
#include <stdexcept>
#include <cstring>

namespace banking::security {

std::string EncryptionUtil::base64Encode(const std::string& input) {
    std::vector<unsigned char> buffer((input.size() + 2) / 3 * 4 + 1);
    int length = EVP_EncodeBlock(buffer.data(), reinterpret_cast<const unsigned char*>(input.data()), input.size());
    return std::string(reinterpret_cast<char*>(buffer.data()), length);
}

std::string EncryptionUtil::base64Decode(const std::string& input) {
    std::vector<unsigned char> buffer(input.size() * 3 / 4 + 1);
    int length = EVP_DecodeBlock(buffer.data(), reinterpret_cast<const unsigned char*>(input.data()), input.size());
    if (length < 0) {
        throw std::runtime_error("Base64 decoding failed");
    }
    int padCount = 0;
    if (!input.empty() && input[input.size() - 1] == '=') padCount++;
    if (input.size() > 1 && input[input.size() - 2] == '=') padCount++;
    return std::string(reinterpret_cast<char*>(buffer.data()), length - padCount);
}

std::string EncryptionUtil::encrypt(const std::string& plaintext, const std::string& key) {
    // Check key size - must be 32 bytes (256 bits) for AES-256
    std::string aesKey = key;
    if (aesKey.size() < 32) {
        aesKey.append(32 - aesKey.size(), '0'); // pad if short
    } else if (aesKey.size() > 32) {
        aesKey = aesKey.substr(0, 32); // truncate if long
    }
    
    // 1. Generate secure random 12-byte IV
    std::vector<unsigned char> iv(12);
    if (RAND_bytes(iv.data(), iv.size()) != 1) {
        throw std::runtime_error("Failed to generate secure random IV");
    }
    
    // 2. Initialize encryption context
    EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
    if (!ctx) {
        throw std::runtime_error("Failed to create OpenSSL cipher context");
    }
    
    try {
        if (EVP_EncryptInit_ex(ctx, EVP_aes_256_gcm(), nullptr, nullptr, nullptr) != 1) {
            throw std::runtime_error("Failed to initialize AES-256-GCM context");
        }
        
        if (EVP_EncryptInit_ex(ctx, nullptr, nullptr, reinterpret_cast<const unsigned char*>(aesKey.data()), iv.data()) != 1) {
            throw std::runtime_error("Failed to set key and IV on AES-256-GCM context");
        }
        
        // 3. Encrypt plaintext
        std::vector<unsigned char> ciphertext(plaintext.size() + EVP_CIPHER_block_size(EVP_aes_256_gcm()));
        int outLen1 = 0;
        if (EVP_EncryptUpdate(ctx, ciphertext.data(), &outLen1, reinterpret_cast<const unsigned char*>(plaintext.data()), plaintext.size()) != 1) {
            throw std::runtime_error("AES-256-GCM encryption update failed");
        }
        
        int outLen2 = 0;
        if (EVP_EncryptFinal_ex(ctx, ciphertext.data() + outLen1, &outLen2) != 1) {
            throw std::runtime_error("AES-256-GCM encryption final failed");
        }
        
        int ciphertextLen = outLen1 + outLen2;
        
        // 4. Retrieve authentication tag (16 bytes)
        std::vector<unsigned char> tag(16);
        if (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_GET_TAG, tag.size(), tag.data()) != 1) {
            throw std::runtime_error("Failed to retrieve AES-256-GCM tag");
        }
        
        EVP_CIPHER_CTX_free(ctx);
        
        // 5. Pack: IV (12 bytes) + TAG (16 bytes) + CIPHERTEXT
        std::string packed = std::string(reinterpret_cast<char*>(iv.data()), iv.size()) +
                             std::string(reinterpret_cast<char*>(tag.data()), tag.size()) +
                             std::string(reinterpret_cast<char*>(ciphertext.data()), ciphertextLen);
                             
        // 6. Encode in Base64
        return base64Encode(packed);
    } catch (...) {
        EVP_CIPHER_CTX_free(ctx);
        throw;
    }
}

std::string EncryptionUtil::decrypt(const std::string& packedBase64, const std::string& key) {
    // Check key size
    std::string aesKey = key;
    if (aesKey.size() < 32) {
        aesKey.append(32 - aesKey.size(), '0');
    } else if (aesKey.size() > 32) {
        aesKey = aesKey.substr(0, 32);
    }
    
    // 1. Decode Base64
    std::string packed = base64Decode(packedBase64);
    if (packed.size() < 28) {
        throw std::runtime_error("Invalid ciphertext package: too short");
    }
    
    // 2. Unpack: IV (12 bytes), TAG (16 bytes), CIPHERTEXT (remainder)
    std::vector<unsigned char> iv(12);
    std::vector<unsigned char> tag(16);
    std::memcpy(iv.data(), packed.data(), 12);
    std::memcpy(tag.data(), packed.data() + 12, 16);
    
    std::string ciphertext = packed.substr(28);
    
    // 3. Initialize decryption context
    EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
    if (!ctx) {
        throw std::runtime_error("Failed to create OpenSSL cipher context");
    }
    
    try {
        if (EVP_DecryptInit_ex(ctx, EVP_aes_256_gcm(), nullptr, nullptr, nullptr) != 1) {
            throw std::runtime_error("Failed to initialize AES-256-GCM context");
        }
        
        if (EVP_DecryptInit_ex(ctx, nullptr, nullptr, reinterpret_cast<const unsigned char*>(aesKey.data()), iv.data()) != 1) {
            throw std::runtime_error("Failed to set key and IV on AES-256-GCM context");
        }
        
        // 4. Decrypt
        std::vector<unsigned char> plaintext(ciphertext.size() + EVP_CIPHER_block_size(EVP_aes_256_gcm()));
        int outLen1 = 0;
        if (EVP_DecryptUpdate(ctx, plaintext.data(), &outLen1, reinterpret_cast<const unsigned char*>(ciphertext.data()), ciphertext.size()) != 1) {
            throw std::runtime_error("AES-256-GCM decryption update failed");
        }
        
        // 5. Set expected authentication tag
        if (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_TAG, tag.size(), tag.data()) != 1) {
            throw std::runtime_error("Failed to set AES-256-GCM tag");
        }
        
        // 6. Finalize decryption and verify tag
        int outLen2 = 0;
        int result = EVP_DecryptFinal_ex(ctx, plaintext.data() + outLen1, &outLen2);
        
        EVP_CIPHER_CTX_free(ctx);
        
        if (result <= 0) {
            throw std::runtime_error("AES-256-GCM authentication verification failed (data tampered or wrong key)");
        }
        
        int plaintextLen = outLen1 + outLen2;
        return std::string(reinterpret_cast<char*>(plaintext.data()), plaintextLen);
    } catch (...) {
        EVP_CIPHER_CTX_free(ctx);
        throw;
    }
}

} // namespace banking::security
