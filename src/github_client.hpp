#pragma once
#include <string>
#include <optional>

struct GHConfig {
    std::string user;
    std::string repo;
    std::string branch;
    std::string token;
    std::string pages_base;
};

struct GHResult { int code; std::string body; };

class GitHubClient {
public:
    explicit GitHubClient(GHConfig cfg);
    GHResult get_pages_json(const std::string& path_within_repo, bool cache_bust);
    GHResult get_contents(const std::string& path_within_repo);
    GHResult put_contents(const std::string& path_within_repo,
                          const std::string& message,
                          const std::string& base64_content,
                          const std::optional<std::string>& sha);
private:
    GHConfig cfg_;
};

std::optional<std::string> extract_sha_from_contents_response(const std::string& body);
std::string base64_encode(const std::string& in);
std::string base64_decode(const std::string& in);
