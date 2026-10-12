#pragma once
#include <string>
#include <sstream>
#include <random>
#include <algorithm>
#include <iostream>
#include "../lib/httplib.h"
#include "../models.hpp"
#include "../lib/json_builder.hpp"
#include "../lib/env_parser.hpp"

namespace Auth {
    using namespace std;
    using Server = httplib::Server; using Request = httplib::Request; using Response = httplib::Response;

    inline string trim(string s) {
        s.erase(s.begin(), find_if(s.begin(), s.end(), [](unsigned char c) { return !isspace(c) && c!='\r' && c!='\n'; }));
        s.erase(find_if(s.rbegin(), s.rend(), [](unsigned char c) { return !isspace(c) && c!='\r' && c!='\n'; }).base(), s.end());
        return s;
    }

    inline string get_json_value(const string& body, const string& key) {
        size_t pos = body.find("\"" + key + "\""); if (pos == string::npos) return "";
        pos = body.find(":", pos); if (pos == string::npos) return "";
        size_t start = body.find("\"", pos); if (start == string::npos) return ""; start++;
        size_t end = body.find("\"", start); if (end == string::npos) return "";
        return body.substr(start, end - start);
    }

    inline string generate_token() {
        string chars = "0123456789ABCDEF", token = "TK";
        random_device rd; mt19937 gen(rd()); uniform_int_distribution<> dis(0, 15);
        for (int i = 0; i < 10; ++i) token += chars[dis(gen)];
        return token;
    }

    struct User { int id = 0; string role = ""; string name = ""; int expired = 0; };
    inline int login_cb(void* d, int, char** v, char**) { auto* r = static_cast<User*>(d); if(v && v[0]) r->id = stoi(v[0]); if(v && v[1]) r->role = v[1]; return 0; }
    inline int auth_cb(void* d, int, char** v, char**) { auto* r = static_cast<User*>(d); if(v && v[0]) r->id = stoi(v[0]); if(v && v[1]) r->role = v[1]; if(v && v[2]) r->name = v[2]; if(v && v[3]) r->expired = stoi(v[3]); return 0; }

    inline int check_auth(const Request& req, string& role, string& user) {
        auto auth = req.get_header_value("Authorization"); if (auth.empty() || auth.find("Bearer ") != 0) return 0;
        string token = trim(auth.substr(7));
        stringstream ss; ss << "SELECT u.id, u.role, u.username, (s.expires_at < datetime('now', 'localtime')) FROM sessions s JOIN users u ON s.user_id = u.id WHERE s.token = '" << token << "';";
        User u_data; sqlite3_exec(db, ss.str().c_str(), auth_cb, &u_data, nullptr);
        if (u_data.id == 0 || u_data.expired == 1) return 0;
        role = u_data.role; user = u_data.name; return u_data.id;
    }

    inline void init_routes(Server& svr) {
        svr.Post("/api/v1.1/auth/register", [](const Request& req, Response& res) {
            string user = get_json_value(req.body, "username"), pass = get_json_value(req.body, "password");
            if (user.empty() || pass.empty()) { res.status = 400; return; }
            stringstream ss; ss << "INSERT INTO users (username, password, role) VALUES ('" << user << "', '" << sha256(pass) << "', 'user');";
            Json::Object obj;
            if (sqlite3_exec(db, ss.str().c_str(), nullptr, nullptr, nullptr) == 0) {
                write_log("SUCCESS", "Registered: " + user); obj.put("message", "User registered"); res.set_content(obj.str(), "application/json");
            } else { res.status = 400; obj.put("error", "Already exists"); res.set_content(obj.str(), "application/json"); }
        });

        svr.Post("/api/v1.1/auth/login", [](const Request& req, Response& res) {
            string user = get_json_value(req.body, "username"), pass = get_json_value(req.body, "password");
            stringstream ss; ss << "SELECT id, role FROM users WHERE username = '" << user << "' AND password = '" << sha256(pass) << "';";
            User u_data; sqlite3_exec(db, ss.str().c_str(), login_cb, &u_data, nullptr);
            Json::Object obj;
            if (u_data.id > 0) {
                string token = generate_token(); int ttl = app_env.get_int("TOKEN_TTL_SECONDS", 300);
                stringstream s_ss; s_ss << "INSERT INTO sessions (token, user_id, expires_at) VALUES ('" << token << "', " << u_data.id << ", datetime('now', 'localtime', '+" << ttl << " seconds'));";
                sqlite3_exec(db, s_ss.str().c_str(), nullptr, nullptr, nullptr);
                write_log("SUCCESS", "Logged in: " + user); obj.put("token", token); obj.put("role", u_data.role); res.set_content(obj.str(), "application/json"); return;
            }
            res.status = 401; obj.put("error", "Invalid credentials"); res.set_content(obj.str(), "application/json");
        });

        svr.Get("/api/v1.1/auth/profile", [](const Request& req, Response& res) {
            string role, user; if (!check_auth(req, role, user)) { res.status = 401; return; }
            string token = trim(req.get_header_value("Authorization").substr(7));
            stringstream ss; ss << "SELECT (strftime('%s', expires_at) - strftime('%s', datetime('now', 'localtime'))) FROM sessions WHERE token = '" << token << "';";
            int sec = 0; sqlite3_stmt* s;
            if (sqlite3_prepare_v2(db, ss.str().c_str(), -1, &s, nullptr) == 0) { if (sqlite3_step(s) == 100) sec = sqlite3_column_int(s, 0); sqlite3_finalize(s); }
            Json::Object obj; obj.put("username", user); obj.put("role", role); obj.put("ttl_seconds_left", sec); res.set_content(obj.str(), "application/json");
        });

        svr.Post("/api/v1.1/auth/refresh", [](const Request& req, Response& res) {
            string r, u; if (!check_auth(req, r, u)) { res.status = 401; return; }
            string token = trim(req.get_header_value("Authorization").substr(7)); int ttl = app_env.get_int("TOKEN_TTL_SECONDS", 300);
            stringstream ss; ss << "UPDATE sessions SET expires_at = datetime('now', 'localtime', '+" << ttl << " seconds') WHERE token = '" << token << "';";
            sqlite3_exec(db, ss.str().c_str(), nullptr, nullptr, nullptr);
            Json::Object obj; obj.put("message", "Token refreshed"); res.set_content(obj.str(), "application/json");
        });

        svr.Get("/api/v1.1/auth/users", [](const Request& req, Response& res) {
            string r, u; if (!check_auth(req, r, u) || (r != "admin" && r != "owner")) { res.status = 403; return; }
            struct List { Json::Array arr; } l_data;
            sqlite3_exec(db, "SELECT id, username, role FROM users;", [](void* d, int, char** v, char**) -> int {
                auto* l = static_cast<List*>(d); Json::Object obj; obj.put("id", stoi(v[0])); obj.put("username", v[1]); obj.put("role", v[2]); l->arr.add_raw(obj.str()); return 0;
            }, &l_data, nullptr); res.set_content(l_data.arr.str(), "application/json");
        });

        svr.Put("/api/v1.1/auth/users/password", [](const Request& req, Response& res) {
            string r, u; if (!check_auth(req, r, u)) { res.status = 401; return; }
            string target = req.get_param_value("username"), pass = req.get_param_value("new_password");
            stringstream ss; ss << "UPDATE users SET password = '" << sha256(pass) << "' WHERE username = '" << target << "';";
            sqlite3_exec(db, ss.str().c_str(), nullptr, nullptr, nullptr);
            Json::Object obj; obj.put("status", "Password updated"); res.set_content(obj.str(), "application/json");
        });

        svr.Delete("/api/v1.1/auth/users", [](const Request& req, Response& res) {
            string r, u; if (!check_auth(req, r, u)) { res.status = 401; return; }
            string target = req.get_param_value("username"); stringstream ss; ss << "DELETE FROM users WHERE username = '" << target << "';";
            sqlite3_exec(db, ss.str().c_str(), nullptr, nullptr, nullptr);
            Json::Object obj; obj.put("status", "User deleted"); res.set_content(obj.str(), "application/json");
        });
    }
}