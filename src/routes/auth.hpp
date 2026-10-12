#pragma once
// ============================================================================
// Модуль авторизации и RBAC.
//
// 8 эндпоинтов:
//   1. POST   /api/v1.1/auth/register
//   2. POST   /api/v1.1/auth/login
//   3. GET    /api/v1.1/auth/profile
//   4. POST   /api/v1.1/auth/refresh
//   5. GET    /api/v1.1/auth/users          (admin/owner)
//   6. PUT    /api/v1.1/auth/users/password
//   7. DELETE /api/v1.1/auth/users          (admin/owner)
//   8. POST   /api/v1.1/auth/logout
// ============================================================================

#include <string>
#include <sstream>
#include <iomanip>
#include <ctime>
#include "../lib/httplib.h"
#include "../database.hpp"
#include "../json.hpp"
#include "../crypto.hpp"

namespace Auth {
    using namespace std;
    using Server   = httplib::Server;
    using Request  = httplib::Request;
    using Response = httplib::Response;

    // Достать строковое поле из плоского JSON.
    inline string jget(const string& body, const string& key) {
        auto p = body.find("\"" + key + "\"");
        if (p == string::npos) return "";
        p = body.find(':', p); if (p == string::npos) return "";
        p = body.find('"', p); if (p == string::npos) return "";
        auto e = body.find('"', p + 1);
        return e == string::npos ? "" : body.substr(p + 1, e - p - 1);
    }

    // Проверка токена. Возвращает id или 0. Заполняет role и name.
    inline int auth(const Request& req, string& role, string& name) {
        string h = req.get_header_value("Authorization");
        if (h.rfind("Bearer ", 0) != 0) return 0;

        auto r = db_row(
            "SELECT u.id,u.role,u.username,s.expires_at "
            "FROM sessions s JOIN users u ON s.user_id=u.id "
            "WHERE s.token=?;",
            {h.substr(7)});

        if (r.size() < 4) return 0;
        if (r[3] <= now_iso()) return 0;   // просрочено

        role = r[1];
        name = r[2];
        return stoi(r[0]);
    }

    inline bool admin(const string& r) {
        return r == "admin" || r == "owner";
    }

    inline void init_routes(Server& svr) {

        // ─── 1. Регистрация ─────────────────────────────────────────────────
        svr.Post("/api/v1.1/auth/register", [](const Request& req, Response& res) {
            string u = jget(req.body, "username");
            string p = jget(req.body, "password");

            JObj o;
            if (u.empty() || p.size() < 4) {
                res.status = 400;
                o.str("error", "Invalid input");
            } else if (db_run("INSERT INTO users (username,password) VALUES (?,?);",
                              {u, hash_password(p)})) {
                write_log("SUCCESS", "Registered: " + u);
                o.str("message", "User registered");
            } else {
                res.status = 409;
                o.str("error", "Username already exists");
            }
            res.set_content(o.str(), "application/json");
        });

        // ─── 2. Логин ───────────────────────────────────────────────────────
        svr.Post("/api/v1.1/auth/login", [](const Request& req, Response& res) {
            string u = jget(req.body, "username");
            string p = jget(req.body, "password");

            auto r = db_row("SELECT id,role,password FROM users WHERE username=?;", {u});

            JObj o;
            if (r.size() < 3 || !verify_password(p, r[2])) {
                res.status = 401;
                o.str("error", "Invalid credentials");
            } else {
                string token = make_token();
                string expires = future_iso(TOKEN_TTL_SECONDS);
                db_run("INSERT INTO sessions (token,user_id,expires_at) VALUES (?,?,?);",
                       {token, r[0], expires});
                write_log("SUCCESS", "Login: " + u);
                o.str("token", token);
                o.str("role", r[1]);
            }
            res.set_content(o.str(), "application/json");
        });

        // ─── 3. Профиль ─────────────────────────────────────────────────────
        svr.Get("/api/v1.1/auth/profile", [](const Request& req, Response& res) {
            string role, name;
            if (!auth(req, role, name)) { res.status = 401; return; }

            auto r = db_row(
                "SELECT expires_at FROM sessions WHERE token=?;",
                {req.get_header_value("Authorization").substr(7)});

            // Считаем TTL в C++.
            long long ttl = 0;
            if (!r.empty() && !r[0].empty()) {
                std::tm tm_buf{};
                std::istringstream ss(r[0]);
                ss >> std::get_time(&tm_buf, "%Y-%m-%d %H:%M:%S");
                if (!ss.fail()) {
                    std::time_t exp = std::mktime(&tm_buf);
                    std::time_t now = std::time(nullptr);
                    ttl = (long long)(exp - now);
                    if (ttl < 0) ttl = 0;
                }
            }

            JObj o;
            o.str("username", name);
            o.str("role", role);
            o.int_("ttl_seconds_left", ttl);
            res.set_content(o.str(), "application/json");
        });

        // ─── 4. Refresh ─────────────────────────────────────────────────────
        svr.Post("/api/v1.1/auth/refresh", [](const Request& req, Response& res) {
            string role, name;
            if (!auth(req, role, name)) { res.status = 401; return; }

            db_run("UPDATE sessions SET expires_at=? WHERE token=?;",
                   {future_iso(TOKEN_TTL_SECONDS),
                    req.get_header_value("Authorization").substr(7)});

            JObj o;
            o.str("message", "Token refreshed");
            res.set_content(o.str(), "application/json");
        });

        // ─── 5. Список пользователей (admin/owner) ──────────────────────────
        svr.Get("/api/v1.1/auth/users", [](const Request& req, Response& res) {
            string role, name;
            if (!auth(req, role, name) || !admin(role)) { res.status = 403; return; }

            JArr arr;
            for (auto& r : db_all("SELECT id,username,role FROM users;")) {
                JObj o;
                o.int_("id", stoll(r[0]));
                o.str("username", r[1]);
                o.str("role", r[2]);
                arr.add(o.str());
            }
            res.set_content(arr.str(), "application/json");
        });

        // ─── 6. Смена пароля ────────────────────────────────────────────────
        svr.Put("/api/v1.1/auth/users/password", [](const Request& req, Response& res) {
            string role, name;
            if (!auth(req, role, name)) { res.status = 401; return; }

            string target = jget(req.body, "username");
            if (target.empty()) target = name;
            string np = jget(req.body, "new_password");

            JObj o;
            if (target != name && !admin(role)) {
                res.status = 403; o.str("error", "Forbidden");
            } else if (np.size() < 4) {
                res.status = 400; o.str("error", "Password too short");
            } else if (db_run("UPDATE users SET password=? WHERE username=?;",
                              {hash_password(np), target})) {
                write_log("SUCCESS", "Password changed: " + target);
                o.str("status", "Password updated");
            } else {
                res.status = 500; o.str("error", "DB error");
            }
            res.set_content(o.str(), "application/json");
        });

        // ─── 7. Удаление пользователя (admin/owner) ─────────────────────────
                // ─── 7. Удаление пользователя (admin/owner) ─────────────────────────
        svr.Delete("/api/v1.1/auth/users", [](const Request& req, Response& res) {
            string role, name;
            if (!auth(req, role, name) || !admin(role)) { res.status = 403; return; }

            string target = jget(req.body, "username");

            JObj o;
            // Защита от удаления владельца и пустого имени.
            if (target.empty() || target == "owner1") {
                res.status = 400;
                o.str("error", "Invalid target");
            }
            // Проверяем, что пользователь существует, ДО удаления.
            else if (db_row("SELECT id FROM users WHERE username=?;", {target}).empty()) {
                res.status = 404;
                o.str("error", "User not found");
            }
            else if (db_run("DELETE FROM users WHERE username=?;", {target})) {
                write_log("SUCCESS", "User deleted: " + target);
                o.str("status", "User deleted");
            }
            else {
                res.status = 500;
                o.str("error", "DB error");
            }
            res.set_content(o.str(), "application/json");
        });

        // ─── 8. Logout ──────────────────────────────────────────────────────
        svr.Post("/api/v1.1/auth/logout", [](const Request& req, Response& res) {
            string role, name;
            if (!auth(req, role, name)) { res.status = 401; return; }

            db_run("DELETE FROM sessions WHERE token=?;",
                   {req.get_header_value("Authorization").substr(7)});

            JObj o;
            o.str("message", "Logged out");
            res.set_content(o.str(), "application/json");
        });
    }
}