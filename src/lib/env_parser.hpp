#pragma once
#include <string>
#include <fstream>
#include <sstream>
#include <map>

class EnvParser {
private:
    std::map<std::string, std::string> env_map;

    std::string trim(const std::string& str) {
        size_t first = str.find_first_not_of(" \t\r\n");
        if (first == std::string::npos) return "";
        size_t last = str.find_last_not_of(" \t\r\n");
        return str.substr(first, (last - first + 1));
    }

public:
    bool load(const std::string& filepath) {
        std::ifstream file(filepath);
        if (!file.is_open()) return false;
        std::string line;
        while (std::getline(file, line)) {
            line = trim(line);
            if (line.empty() || line[0] == '#') continue;
            size_t delim = line.find('=');
            if (delim == std::string::npos) continue;
            std::string key = trim(line.substr(0, delim));
            std::string val = trim(line.substr(delim + 1));
            env_map[key] = val;
        }
        return true;
    }

    int get_int(const std::string& key, int default_val) {
        if (env_map.find(key) != env_map.end()) {
            try { return std::stoi(env_map[key]); } catch (...) {}
        }
        return default_val;
    }
};

inline EnvParser app_env;