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

    // 1. GET /api/v1.1/auth/profile
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

    // 2. POST /api/v1.1/auth/refresh
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

    // 3. GET /api/v1.1/auth/users
    svr.Get("/api/v1.1/auth/users", [](const Request&, Response& res) {
        stringstream json;
        json << "{\n  \"data\": [\n    {\"username\": \"admin1\", \"role\": \"admin\"},\n    {\"username\": \"user1\", \"role\": \"user\"}\n  ],\n"
             << "  \"response_time\": \"" << get_time_str(chrono::system_clock::now()) << "\"\n}";
        res.set_content(json.str(), "application/json; charset=utf-8");
    });

    // 4. PUT /api/v1.1/auth/users/password
    svr.Put("/api/v1.1/auth/users/password", [](const Request&, Response& res) {
        res.set_content("{\n  \"success\": true,\n  \"data\": { \"message\": \"Password changed\" },\n  \"response_time\": \"" + get_time_str(chrono::system_clock::now()) + "\"\n}", "application/json; charset=utf-8");
    });

    // 5. DELETE /api/v1.1/auth/users
    svr.Delete("/api/v1.1/auth/users", [](const Request&, Response& res) {
        res.set_content("{\n  \"success\": true,\n  \"data\": { \"message\": \"User deleted\" },\n  \"response_time\": \"" + get_time_str(chrono::system_clock::now()) + "\"\n}", "application/json; charset=utf-8");
    });
}
