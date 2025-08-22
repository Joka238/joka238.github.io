#pragma once
#include <string>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <cstdio>

inline std::string read_file(const std::string& p) {
    std::ifstream f(p, std::ios::binary);
    if (!f) throw std::runtime_error("cannot open: " + p);
    std::ostringstream ss; ss << f.rdbuf();
    return ss.str();
}

inline void write_file(const std::string& p, const std::string& s) {
    std::ofstream f(p, std::ios::binary);
    f << s;
}

inline std::string escape_json(const std::string& s){
    std::string o; o.reserve(s.size()+8);
    for(char c: s){
        switch(c){
            case '\"': o += "\\\""; break;
            case '\\': o += "\\\\"; break;
            case '\b': o += "\\b"; break;
            case '\f': o += "\\f"; break;
            case '\n': o += "\\n"; break;
            case '\r': o += "\\r"; break;
            case '\t': o += "\\t"; break;
            default:
                if ((unsigned char)c < 0x20) { char buf[7]; snprintf(buf,sizeof(buf),"\\u%04x", c); o += buf; }
                else o += c;
        }
    }
    return o;
}
