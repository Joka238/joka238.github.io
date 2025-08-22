#include <SDL2/SDL.h>
#include <cstdio>
#include <vector>
#include <string>
#include <optional>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <cstdint>
#include <stdexcept>
#include "strokes.hpp"
#include "util.hpp"
#include "github_client.hpp"
#include "crypto.hpp"

extern "C" {
#include <json-c/json.h>
}

static uint64_t now_s() {
    using namespace std::chrono;
    return duration_cast<seconds>(steady_clock::now().time_since_epoch()).count();
}

static std::string to_plain_json(const FrameData& f){
    std::string j = "{";
    j += "\"session\":\""+escape_json(f.session)+"\",";
    j += "\"user\":\""+escape_json(f.user)+"\",";
    j += "\"ts\":"+std::to_string(f.ts)+",";
    j += "\"strokes\":[";
    bool firstStroke = true;
    for (const auto& s: f.strokes){
        if(!firstStroke) j += ",";
        firstStroke = false;
        j += "{";
        j += "\"id\":\""+escape_json(s.id)+"\",";
        j += "\"color\":\""+escape_json(s.color)+"\",";
        j += "\"width\":"+std::to_string(s.width)+",";
        j += "\"points\":[";
        bool firstPoint = true;
        for (const auto& p: s.pts){
            if(!firstPoint) j += ",";
            firstPoint = false;
            j += "[" + std::to_string(p.x) + "," + std::to_string(p.y) + "," + std::to_string(p.t) + "]";
        }
        j += "]";
        j += "}";
    }
    j += "]";
    j += "}";
    return j;
}

struct AppCfg {
    std::string user, repo, branch, token, pages_base, my_file, peer_file, session, user_id, passphrase;
    int poll_seconds = 15; 
    int commit_min_interval_sec = 20;
};

static AppCfg load_cfg(const std::string& path){
    auto s = read_file(path);
    json_object* root = json_tokener_parse(s.c_str());
    if (!root) throw std::runtime_error("config.json parse error");

    AppCfg c{};
    auto getS=[&](const char* k)->std::string{
        json_object* v=nullptr; 
        if(!json_object_object_get_ex(root,k,&v)) return "";
        const char* t = json_object_get_string(v);
        return t? std::string(t) : std::string();
    };
    auto getI=[&](const char* k, int def)->int{
        json_object* v=nullptr;
        if(!json_object_object_get_ex(root,k,&v)) return def;
        return json_object_get_int(v);
    };

    c.user = getS("user");
    c.repo = getS("repo");
    c.branch = getS("branch");
    c.token = getS("personal_access_token");
    c.pages_base = getS("pages_base");
    c.my_file = getS("my_file");
    c.peer_file = getS("peer_file");
    c.session = getS("session");
    c.user_id = getS("user_id");
    c.passphrase = getS("passphrase");
    c.poll_seconds = getI("poll_seconds", 15);
    c.commit_min_interval_sec = getI("commit_min_interval_sec", 20);

    json_object_put(root);
    if (c.user.empty()||c.repo.empty()||c.branch.empty()||c.token.empty()||c.my_file.empty())
        throw std::runtime_error("config.json missing required fields");
    return c;
}

static void draw_polyline(SDL_Renderer* r, const Stroke& s, int W, int H, int r255, int g255, int b255){
    SDL_SetRenderDrawColor(r, r255,g255,b255,255);
    for (size_t i=1;i<s.pts.size();++i){
        int x1 = (int)std::lround(s.pts[i-1].x*W);
        int y1 = (int)std::lround(s.pts[i-1].y*H);
        int x2 = (int)std::lround(s.pts[i].x*W);
        int y2 = (int)std::lround(s.pts[i].y*H);
        SDL_RenderDrawLine(r, x1,y1,x2,y2);
    }
}

static bool parse_frame_json(const std::string& body, FrameData& out){
    json_object* root = json_tokener_parse(body.c_str());
    if (!root) return false;
    json_object* jstrokes=nullptr;
    json_object* jsess=nullptr; json_object* juser=nullptr; json_object* jts=nullptr;
    if (json_object_object_get_ex(root,"session",&jsess)) out.session = json_object_get_string(jsess);
    if (json_object_object_get_ex(root,"user",&juser)) out.user = json_object_get_string(juser);
    if (json_object_object_get_ex(root,"ts",&jts)) out.ts = (uint64_t)json_object_get_int64(jts);
    if (!json_object_object_get_ex(root,"strokes",&jstrokes) || !json_object_is_type(jstrokes, json_type_array)){ json_object_put(root); return false; }

    size_t n = json_object_array_length(jstrokes);
    out.strokes.clear(); out.strokes.reserve(n);
    for (size_t i=0;i<n;i++){
        json_object* js = json_object_array_get_idx(jstrokes, i);
        Stroke s{};
        json_object* jid=nullptr; json_object* jcol=nullptr; json_object* jwidth=nullptr; json_object* jpoints=nullptr;
        if (json_object_object_get_ex(js,"id",&jid)) s.id = json_object_get_string(jid);
        if (json_object_object_get_ex(js,"color",&jcol)) s.color = json_object_get_string(jcol);
        if (json_object_object_get_ex(js,"width",&jwidth)) s.width = json_object_get_int(jwidth);
        if (json_object_object_get_ex(js,"points",&jpoints) && json_object_is_type(jpoints, json_type_array)){
            size_t m = json_object_array_length(jpoints);
            s.pts.reserve(m);
            for (size_t k=0;k<m;k++){
                json_object* jp = json_object_array_get_idx(jpoints, k);
                if (!json_object_is_type(jp, json_type_array) || json_object_array_length(jp)<2) continue;
                float x = (float)json_object_get_double(json_object_array_get_idx(jp,0));
                float y = (float)json_object_get_double(json_object_array_get_idx(jp,1));
                uint64_t t = 0;
                if (json_object_array_length(jp) >= 3) t = (uint64_t)json_object_get_int64(json_object_array_get_idx(jp,2));
                s.pts.push_back({x,y,t});
            }
        }
        out.strokes.push_back(std::move(s));
    }
    json_object_put(root);
    return true;
}

int main(int argc, char** argv){
    const char* cfgPath = argc>1? argv[1] : "config.json";
    AppCfg cfg = load_cfg(cfgPath);
    GitHubClient gh({cfg.user,cfg.repo,cfg.branch,cfg.token,cfg.pages_base});

    if (SDL_Init(SDL_INIT_VIDEO|SDL_INIT_TIMER|SDL_INIT_EVENTS)!=0){
        fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
        return 1;
    }
    SDL_Window* win = SDL_CreateWindow("telepaint", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 1000, 700, 0);
    SDL_Renderer* ren = SDL_CreateRenderer(win, -1, SDL_RENDERER_ACCELERATED);

    bool quit=false, drawing=false; int W=1000,H=700; SDL_GetRendererOutputSize(ren,&W,&H);
    std::vector<Stroke> my_strokes; Stroke current;
    FrameData peer_frame{};
    uint64_t last_commit = 0; uint64_t last_poll = 0;

    auto new_stroke = [&]{ Stroke s; s.id=std::to_string(my_strokes.size()+1); s.color="#111111"; s.width=3; return s; };

    while(!quit){
        SDL_Event e; while(SDL_PollEvent(&e)){
            if (e.type==SDL_QUIT) quit=true;
            if (e.type==SDL_KEYDOWN && e.key.keysym.sym==SDLK_ESCAPE) quit=true;
            if (e.type==SDL_KEYDOWN && e.key.keysym.sym==SDLK_c){ my_strokes.clear(); } // local clear
            if (e.type==SDL_MOUSEBUTTONDOWN){ drawing=true; current = new_stroke(); }
            if (e.type==SDL_MOUSEBUTTONUP){ drawing=false; if(!current.pts.empty()) my_strokes.push_back(current); }
            if (e.type==SDL_MOUSEMOTION && drawing){
                float x = e.motion.x/(float)W; float y=e.motion.y/(float)H;
                if (x<0) x=0; if (x>1) x=1; if (y<0) y=0; if (y>1) y=1;
                current.pts.push_back({x,y,now_s()});
            }
        }

        SDL_SetRenderDrawColor(ren,255,255,255,255); SDL_RenderClear(ren);
        for (auto& s: peer_frame.strokes) draw_polyline(ren, s, W, H, 210,34,34);
        for (auto& s: my_strokes) draw_polyline(ren, s, W, H, 0,0,0);
        if (drawing && current.pts.size()>1) draw_polyline(ren, current, W, H, 0,0,0);
        SDL_RenderPresent(ren);

        uint64_t now = now_s();

        // Upload wenn es lokale Strokes gibt
        if (now - last_commit >= (uint64_t)cfg.commit_min_interval_sec && (!my_strokes.empty())){
            FrameData f{cfg.session, cfg.user_id, now, my_strokes};
            std::string plain = to_plain_json(f);
            std::string payloadJson;
            if (!cfg.passphrase.empty()){
                CryptoEnvelope env = encrypt_aes_gcm_pbkdf2(plain, cfg.passphrase, 200000);
                payloadJson = env.to_json();
            } else {
                payloadJson = plain;
            }
            auto r1 = gh.get_contents(cfg.my_file);
            std::optional<std::string> sha;
            if (r1.code==200){
                auto shaOpt = extract_sha_from_contents_response(r1.body);
                if (shaOpt) sha = *shaOpt;
            }
            auto b64 = base64_encode(payloadJson);
            auto r = gh.put_contents(cfg.my_file, "update", b64, sha);
            if (!(r.code>=200 && r.code<300)){
                fprintf(stderr, "Upload failed: HTTP %d\n", r.code);
            }
            last_commit = now;
        }

        // Peer poll
        if (now - last_poll >= (uint64_t)cfg.poll_seconds){
            auto r = gh.get_pages_json(cfg.peer_file, true);
            if (r.code==200){
                std::string body = r.body;
                CryptoEnvelope env;
                bool isEnc = try_parse_envelope_json(body, env);
                if (isEnc){
                    try {
                        if (!cfg.passphrase.empty()) {
                            std::string plain = decrypt_aes_gcm_pbkdf2(env, cfg.passphrase, 200000);
                            FrameData tmp{};
                            if (parse_frame_json(plain, tmp)) peer_frame = std::move(tmp);
                        } else {
                            fprintf(stderr, "Peer is encrypted; set passphrase to view.\n");
                        }
                    } catch(const std::exception& ex){
                        fprintf(stderr, "Peer decrypt failed: %s\n", ex.what());
                    }
                } else {
                    FrameData tmp{};
                    if (parse_frame_json(body, tmp)) peer_frame = std::move(tmp);
                }
            }
            last_poll = now;
        }

        SDL_Delay(12);
    }

    SDL_DestroyRenderer(ren); SDL_DestroyWindow(win); SDL_Quit();
    return 0;
}
