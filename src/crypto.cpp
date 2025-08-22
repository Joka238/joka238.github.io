#include "crypto.hpp"
#include "github_client.hpp"
#include <openssl/evp.h>
#include <openssl/rand.h>
#include <openssl/buffer.h>
#include <openssl/bio.h>
#include <openssl/err.h>
#include <stdexcept>
#include <sstream>

static std::string b64(const unsigned char* data, size_t len){
    BIO *bio, *b64; BUF_MEM *bufferPtr;
    b64 = BIO_new(BIO_f_base64());
    bio = BIO_new(BIO_s_mem());
    b64 = BIO_push(b64, bio);
    BIO_set_flags(b64, BIO_FLAGS_BASE64_NO_NL);
    BIO_write(b64, data, (int)len);
    BIO_flush(b64);
    BIO_get_mem_ptr(b64, &bufferPtr);
    std::string out(bufferPtr->data, bufferPtr->length);
    BIO_free_all(b64);
    return out;
}

std::string CryptoEnvelope::to_json() const {
    std::ostringstream o;
    o << "{"
      << "\"v\":1,"
      << "\"alg\":\"" << alg << "\","
      << "\"salt\":\"" << salt << "\","
      << "\"iv\":\"" << iv << "\","
      << "\"ct\":\"" << ct << "\""
      << "}";
    return o.str();
}

CryptoEnvelope encrypt_aes_gcm_pbkdf2(const std::string& plaintext, const std::string& passphrase, int iterations){
    CryptoEnvelope env;
    env.alg = "AES-256-GCM";

    unsigned char salt[16];
    unsigned char iv[12];
    if (RAND_bytes(salt, sizeof(salt)) != 1) throw std::runtime_error("RAND_bytes salt failed");
    if (RAND_bytes(iv, sizeof(iv)) != 1) throw std::runtime_error("RAND_bytes iv failed");

    std::vector<unsigned char> key(32);
    if (PKCS5_PBKDF2_HMAC(passphrase.c_str(), (int)passphrase.size(),
                          salt, sizeof(salt), iterations, EVP_sha256(),
                          (int)key.size(), key.data()) != 1){
        throw std::runtime_error("PBKDF2 failed");
    }

    EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
    if (!ctx) throw std::runtime_error("EVP_CIPHER_CTX_new failed");

    int rc = EVP_EncryptInit_ex(ctx, EVP_aes_256_gcm(), nullptr, nullptr, nullptr);
    if (rc != 1) { EVP_CIPHER_CTX_free(ctx); throw std::runtime_error("EncryptInit failed"); }

    rc = EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_IVLEN, sizeof(iv), nullptr);
    if (rc != 1) { EVP_CIPHER_CTX_free(ctx); throw std::runtime_error("SET_IVLEN failed"); }

    rc = EVP_EncryptInit_ex(ctx, nullptr, nullptr, key.data(), iv);
    if (rc != 1) { EVP_CIPHER_CTX_free(ctx); throw std::runtime_error("EncryptInit key/iv failed"); }

    std::vector<unsigned char> out(plaintext.size()+16+16);
    int outlen1=0, outlen2=0;
    rc = EVP_EncryptUpdate(ctx, out.data(), &outlen1, reinterpret_cast<const unsigned char*>(plaintext.data()), (int)plaintext.size());
    if (rc != 1) { EVP_CIPHER_CTX_free(ctx); throw std::runtime_error("EncryptUpdate failed"); }

    rc = EVP_EncryptFinal_ex(ctx, out.data()+outlen1, &outlen2);
    if (rc != 1) { EVP_CIPHER_CTX_free(ctx); throw std::runtime_error("EncryptFinal failed"); }
    int ctlen = outlen1 + outlen2;

    unsigned char tag[16];
    rc = EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_GET_TAG, sizeof(tag), tag);
    if (rc != 1) { EVP_CIPHER_CTX_free(ctx); throw std::runtime_error("GET_TAG failed"); }
    EVP_CIPHER_CTX_free(ctx);

    std::vector<unsigned char> full(ctlen + sizeof(tag));
    std::copy(out.begin(), out.begin()+ctlen, full.begin());
    std::copy(tag, tag+sizeof(tag), full.begin()+ctlen);

    env.salt = b64(salt, sizeof(salt));
    env.iv   = b64(iv, sizeof(iv));
    env.ct   = b64(full.data(), full.size());
    return env;
}

std::string decrypt_aes_gcm_pbkdf2(const CryptoEnvelope& env, const std::string& passphrase, int iterations){
    std::string salt = base64_decode(env.salt);
    std::string iv = base64_decode(env.iv);
    std::string ct = base64_decode(env.ct);
    if (iv.size()!=12) throw std::runtime_error("IV length invalid");
    if (ct.size() < 16) throw std::runtime_error("cipher too short");

    std::string cipher = ct.substr(0, ct.size()-16);
    std::string tag = ct.substr(ct.size()-16);

    std::vector<unsigned char> key(32);
    if (PKCS5_PBKDF2_HMAC(passphrase.c_str(), (int)passphrase.size(),
                          reinterpret_cast<const unsigned char*>(salt.data()), (int)salt.size(),
                          iterations, EVP_sha256(),
                          (int)key.size(), key.data()) != 1){
        throw std::runtime_error("PBKDF2 failed");
    }

    EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
    if (!ctx) throw std::runtime_error("EVP_CIPHER_CTX_new failed");
    int rc = EVP_DecryptInit_ex(ctx, EVP_aes_256_gcm(), nullptr, nullptr, nullptr);
    if (rc != 1) { EVP_CIPHER_CTX_free(ctx); throw std::runtime_error("DecryptInit failed"); }
    rc = EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_IVLEN, (int)iv.size(), nullptr);
    if (rc != 1) { EVP_CIPHER_CTX_free(ctx); throw std::runtime_error("SET_IVLEN failed"); }
    rc = EVP_DecryptInit_ex(ctx, nullptr, nullptr, key.data(), reinterpret_cast<const unsigned char*>(iv.data()));
    if (rc != 1) { EVP_CIPHER_CTX_free(ctx); throw std::runtime_error("DecryptInit key/iv failed"); }

    std::string out; out.resize(cipher.size());
    int outlen1=0, outlen2=0;
    rc = EVP_DecryptUpdate(ctx, reinterpret_cast<unsigned char*>(&out[0]), &outlen1,
                           reinterpret_cast<const unsigned char*>(cipher.data()), (int)cipher.size());
    if (rc != 1) { EVP_CIPHER_CTX_free(ctx); throw std::runtime_error("DecryptUpdate failed"); }

    rc = EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_TAG, (int)tag.size(), (void*)tag.data());
    if (rc != 1) { EVP_CIPHER_CTX_free(ctx); throw std::runtime_error("SET_TAG failed"); }

    rc = EVP_DecryptFinal_ex(ctx, reinterpret_cast<unsigned char*>(&out[0]) + outlen1, &outlen2);
    EVP_CIPHER_CTX_free(ctx);
    if (rc != 1) throw std::runtime_error("DecryptFinal auth failed");

    out.resize(outlen1 + outlen2);
    return out;
}

#include <json-c/json.h>
bool try_parse_envelope_json(const std::string& s, CryptoEnvelope& out){
    json_object* root = json_tokener_parse(s.c_str());
    if (!root) return false;
    json_object* jalg=nullptr; json_object* jsalt=nullptr; json_object* jiv=nullptr; json_object* jct=nullptr;
    bool ok = json_object_object_get_ex(root,"alg",&jalg) &&
              json_object_object_get_ex(root,"salt",&jsalt) &&
              json_object_object_get_ex(root,"iv",&jiv) &&
              json_object_object_get_ex(root,"ct",&jct);
    if (ok){
        out.alg = json_object_get_string(jalg);
        out.salt = json_object_get_string(jsalt);
        out.iv = json_object_get_string(jiv);
        out.ct = json_object_get_string(jct);
    }
    json_object_put(root);
    return ok;
}
