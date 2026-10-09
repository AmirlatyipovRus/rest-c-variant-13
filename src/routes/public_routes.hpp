#pragma once
#include <fstream>
#include <sstream>
#include <string>
#include <chrono>
#include <iomanip>
#include <map>
#include "httplib.h"
#include "models.hpp"

using namespace std;
using namespace chrono;
using namespace httplib;

extern map<string, SessionData> active_sessions;
extern int TOKEN_TTL_SECONDS;

inline string get_time_str(system_clock::time_point tp) {
    auto t = system_clock::to_time_t(tp);
    stringstream ss; ss << put_time(localtime(&t), "%Y-%m-%d %H:%M:%S");
    return ss.str();
}

inline string parse_json(const string& body, const string& key) {
    size_t pos = body.find("\"" + key + "\"");
    if (pos == string::npos) return "";
    pos = body.find("\"", body.find(":", pos));
    return body.substr(pos + 1, body.find("\"", pos + 1) - pos - 1);
}

inline string get_user_role(const string& user, const string& pass) {
    ifstream file("data/users.txt"); string line;
    while (getline(file, line)) {
        stringstream ss(line); string u, p, r;
        if (getline(ss, u, ':') && getline(ss, p, ':') && getline(ss, r, ':')) {
            if (u == user && p == pass) return r;
        }
    }
    return "";
}

inline void setup_public_routes(Server& svr) {
    
    svr.Get("/api/v1.1/counters", [](const Request&, Response& res) {
        string now = get_time_str(system_clock::now());
        res.set_content("{\n  \"data\": {\n    \"total_users\": 4,\n    \"total_successful_calculations\": 142,\n    \"total_failed_validations\": 18\n  },\n  \"response_time\": \"" + now + "\"\n}", "application/json; charset=utf-8");
    });
}
