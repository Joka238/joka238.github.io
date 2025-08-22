#pragma once
#include <string>
#include <vector>

struct CryptoEnvelope {
    std::string alg;   // "AES-256-GCM"
    std::string salt;  // base64
    std::string iv;    // base64 (12 bytes)
    std::string ct;    // base64 (ciphertext + tag)

    std::string to_json() const;
};

CryptoEnvelope encrypt_aes_gcm_pbkdf2(const std::string& plaintext, const std::string& passphrase, int iterations);
std::string decrypt_aes_gcm_pbkdf2(const CryptoEnvelope& env, const std::string& passphrase, int iterations);
bool try_parse_envelope_json(const std::string& json, CryptoEnvelope& out);
