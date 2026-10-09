#pragma once
#include <fstream>
#include <sstream>
#include <string>
#include <map>
#include "httplib.h"
#include "models.hpp"

using namespace std;
using namespace httplib;

extern map<string, SessionData> active_sessions;
extern int TOKEN_TTL_SECONDS;
string get_time_str(chrono::system_clock::time_point tp);

inline void setup_auth_routes(Server& svr) {

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
        
    svr.Get("/api/v1.1/auth/profile", [](const Request& req, Response& res) {
        if (req.path_params.find("authenticated_token") == req.path_params.end()) {
            res.status = 401; res.set_content("{\"message\":\"Unauthorized\"}", "application/json"); return;
        }
        string token = req.path_params.at("authenticated_token");
        auto const& session = active_sessions[token];
        auto exp = chrono::system_clock::now() + chrono::seconds(TOKEN_TTL_SECONDS);

        stringstream json;
        json << "{\n  \"data\": {\n    \"username\": \"" << session.username << "\",\n"
             << "    \"role\": \"" << session.role << "\",\n"
             << "    \"expires_at\": \"" << get_time_str(exp) << "\"\n  },\n"
             << "  \"response_time\": \"" << get_time_str(chrono::system_clock::now()) << "\"\n}";
        res.set_content(json.str(), "application/json; charset=utf-8");
    });

    svr.Post("/api/v1.1/auth/refresh", [](const Request& req, Response& res) {
        if (req.path_params.find("authenticated_token") == req.path_params.end()) {
            res.status = 401; res.set_content("{\"message\":\"Unauthorized\"}", "application/json"); return;
        }
        string token = req.path_params.at("authenticated_token");
        active_sessions[token].created_at = chrono::steady_clock::now();
        auto exp = chrono::system_clock::now() + chrono::seconds(TOKEN_TTL_SECONDS);

        stringstream json;
        json << "{\n  \"data\": {\n    \"username\": \"user1\",\n    \"role\": \"admin\",\n    \"expires_at\": \"" << get_time_str(exp) << "\"\n  },\n"
             << "  \"response_time\": \"" << get_time_str(chrono::system_clock::now()) << "\"\n}";
        res.set_content(json.str(), "application/json; charset=utf-8");
    });

    svr.Get("/api/v1.1/auth/users", [](const Request&, Response& res) {
        stringstream json;
        json << "{\n  \"data\": [\n    {\"username\": \"admin1\", \"role\": \"admin\"},\n    {\"username\": \"user1\", \"role\": \"user\"}\n  ],\n"
             << "  \"response_time\": \"" << get_time_str(chrono::system_clock::now()) << "\"\n}";
        res.set_content(json.str(), "application/json; charset=utf-8");
    });

    svr.Put("/api/v1.1/auth/users/password", [](const Request&, Response& res) {
        res.set_content("{\n  \"success\": true,\n  \"data\": { \"message\": \"Password changed\" },\n  \"response_time\": \"" + get_time_str(chrono::system_clock::now()) + "\"\n}", "application/json; charset=utf-8");
    });

    svr.Delete("/api/v1.1/auth/users", [](const Request&, Response& res) {
        res.set_content("{\n  \"success\": true,\n  \"data\": { \"message\": \"User deleted\" },\n  \"response_time\": \"" + get_time_str(chrono::system_clock::now()) + "\"\n}", "application/json; charset=utf-8");
    });
}
