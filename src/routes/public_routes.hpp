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
    
    // 1. Counters
    svr.Get("/api/v1.1/guest/counters", [](const Request&, Response& res) {
        string now = get_time_str(system_clock::now());
        res.set_content("{\n  \"data\": {\n    \"total_users\": 4,\n    \"total_successful_calculations\": 142,\n    \"total_failed_validations\": 18\n  },\n  \"response_time\": \"" + now + "\"\n}", "application/json; charset=utf-8");
    });

    // 2. Register
    svr.Post("/api/v1.1/auth/register", [](const Request& req, Response& res) {
        string user = parse_json(req.body, "username");
        string pass = parse_json(req.body, "password");
        string now = get_time_str(system_clock::now());

        if (user.empty() || pass.empty()) {
            res.status = 400; res.set_content("{\"success\":false,\"data\":{\"message\":\"Заполните поля!\"}}", "application/json; charset=utf-8"); return;
        }

        ifstream file_in("data/users.txt"); string line; bool exist = false;
        while (getline(file_in, line)) {
            size_t sep = line.find(':');
            if (sep != string::npos && line.substr(0, sep) == user) { exist = true; break; }
        }
        file_in.close();

        if (exist) {
            res.status = 409;
            res.set_content("{\n  \"success\": false,\n  \"data\": {\n    \"message\": \"Ошибка регистрации: имя пользователя '" + user + "' уже занято в системе.\"\n  },\n  \"response_time\": \"" + now + "\"\n}", "application/json; charset=utf-8"); return;
        }

        ofstream file_out("data/users.txt", ios_base::app);
        if (file_out.is_open()) { file_out << user << ":" << pass << ":user\n"; file_out.close(); }

        res.set_content("{\n  \"success\": true,\n  \"data\": {\n    \"message\": \"Пользователь " + user + " успешно зарегистрирован.\",\n    \"role\": \"user\"\n  },\n  \"response_time\": \"" + now + "\"\n}", "application/json; charset=utf-8");
    });

    // 3. Login
    svr.Post("/api/v1.1/auth/login", [](const Request& req, Response& res) {
        string user = parse_json(req.body, "username");
        string pass = parse_json(req.body, "password");
        string role = get_user_role(user, pass);

        if (!role.empty()) {
            string token = "TK" + to_string(rand() % 90000 + 10000);
            auto now = system_clock::now();

            active_sessions[token] = { user, role, steady_clock::now() };

            stringstream json;
            json << "{\n  \"success\": true,\n  \"data\": {\n    \"token\": \"\"" << token << "\",\n"
                 << "    \"role\": \"" << role << "\",\n"
                 << "    \"ttl_seconds\": " << TOKEN_TTL_SECONDS << ",\n"
                 << "    \"created_at\": \"" << get_time_str(now) << "\",\n"
                 << "    \"expires_at\": \"" << get_time_str(now + seconds(TOKEN_TTL_SECONDS)) << "\"\n"
                 << "  },\n  \"response_time\": \"" << get_time_str(now) << "\"\n}";
            res.set_content(json.str(), "application/json; charset=utf-8");
        } else {
            res.status = 401; res.set_content("{\"success\":false,\"data\":{\"message\":\"Неверный логин или пароль\"}}", "application/json; charset=utf-8");
        }
    });
}
