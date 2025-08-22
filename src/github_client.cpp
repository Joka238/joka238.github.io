#include "github_client.hpp"
#include <curl/curl.h>
#include <stdexcept>
#include <sstream>
#include <ctime>
#include <openssl/bio.h>
#include <openssl/evp.h>
#include <openssl/buffer.h>

static size_t wr(void* c, size_t s, size_t n, void* u) {
    ((std::string*)u)->append((char*)c, s*n); return s*n;
}

GitHubClient::GitHubClient(GHConfig cfg): cfg_(std::move(cfg)) {}

static GHResult http_req(const std::string& url, const std::string& method,
                  const std::string& token,
                  const std::string& body = "",
                  const std::string& content_type = "application/json") {
    CURL* h = curl_easy_init();
    if (!h) throw std::runtime_error("curl init fail");
    std::string resp; long code=0;
    struct curl_slist* hdrs = nullptr;
    std::string ua = "User-Agent: telepaint";
    hdrs = curl_slist_append(hdrs, ua.c_str());
    if (!token.empty()) {
        std::string auth = "Authorization: token " + token;
        hdrs = curl_slist_append(hdrs, auth.c_str());
    }
    if (method=="PUT" || method=="POST")
        hdrs = curl_slist_append(hdrs, ("Content-Type: "+content_type).c_str());

    curl_easy_setopt(h, CURLOPT_URL, url.c_str());
    curl_easy_setopt(h, CURLOPT_HTTPHEADER, hdrs);
    curl_easy_setopt(h, CURLOPT_WRITEFUNCTION, wr);
    curl_easy_setopt(h, CURLOPT_WRITEDATA, &resp);
    if (method=="PUT") curl_easy_setopt(h, CURLOPT_CUSTOMREQUEST, "PUT");
    else curl_easy_setopt(h, CURLOPT_CUSTOMREQUEST, method.c_str());
    if (!body.empty()) curl_easy_setopt(h, CURLOPT_POSTFIELDS, body.c_str());

    curl_easy_perform(h);
    curl_easy_getinfo(h, CURLINFO_RESPONSE_CODE, &code);
    curl_slist_free_all(hdrs);
    curl_easy_cleanup(h);
    return {(int)code, resp};
}

GHResult GitHubClient::get_pages_json(const std::string& path, bool bust){
    std::string u = cfg_.pages_base + path;
    if (bust) {
        u += (u.find('?')==std::string::npos?"?":"&");
        u += "t=" + std::to_string(std::time(nullptr));
    }
    return http_req(u, "GET", ""); // public read
}

GHResult GitHubClient::get_contents(const std::string& path){
    std::ostringstream u;
    u << "https://api.github.com/repos/"<<cfg_.user<<"/"<<cfg_.repo
      << "/contents/"<<path<<"?ref="<<cfg_.branch;
    return http_req(u.str(), "GET", cfg_.token);
}

GHResult GitHubClient::put_contents(const std::string& path,
    const std::string& msg, const std::string& b64, const std::optional<std::string>& sha){
    std::ostringstream u;
    u << "https://api.github.com/repos/"<<cfg_.user<<"/"<<cfg_.repo
      << "/contents/"<<path;
    std::ostringstream body;
    body << "{\"message\":\""<<msg<<"\",\"content\":\""<<b64<<"\"";
    if (sha) body << ",\"sha\":\""<<*sha<<"\"";
    body << ",\"branch\":\""<<cfg_.branch<<"\"}";
    return http_req(u.str(), "PUT", cfg_.token, body.str());
}

std::optional<std::string> extract_sha_from_contents_response(const std::string& body){
    auto pos = body.find("\"sha\":\"");
    if (pos==std::string::npos) return std::nullopt;
    pos += 7;
    auto end = body.find('"', pos);
    if (end==std::string::npos) return std::nullopt;
    return body.substr(pos, end-pos);
}

std::string base64_encode(const std::string& in){
    BIO *bio, *b64; BUF_MEM *bufferPtr;
    b64 = BIO_new(BIO_f_base64());
    bio = BIO_new(BIO_s_mem());
    b64 = BIO_push(b64, bio);
    BIO_set_flags(b64, BIO_FLAGS_BASE64_NO_NL);
    BIO_write(b64, in.data(), (int)in.size());
    BIO_flush(b64);
    BIO_get_mem_ptr(b64, &bufferPtr);
    std::string out(bufferPtr->data, bufferPtr->length);
    BIO_free_all(b64);
    return out;
}

std::string base64_decode(const std::string& in){
    BIO *bio, *b64;
    int len = (int)in.size();
    std::string out; out.resize(len);
    b64 = BIO_new(BIO_f_base64());
    bio = BIO_new_mem_buf(in.data(), len);
    b64 = BIO_push(b64, bio);
    BIO_set_flags(b64, BIO_FLAGS_BASE64_NO_NL);
    int outlen = BIO_read(b64, out.data(), len);
    BIO_free_all(b64);
    if (outlen < 0) outlen = 0;
    out.resize(outlen);
    return out;
}
