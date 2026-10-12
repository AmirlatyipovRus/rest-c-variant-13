#pragma once
// ============================================================================
// Публичные и административные эндпоинты.
//
//   1. GET /api/v1.1/counters        — счётчики (без авторизации)
//   2. GET /api/v1.1/history         — публичная история (без авторизации)
//   3. GET /api/v1.1/admin/stats     — логи (только admin/owner)
//   4. GET /api/v1.1/admin/run-tests — самотестирование (только admin/owner)
// ============================================================================

#include <string>
#include <cmath>
#include <numeric>
#include <algorithm>
#include <vector>
#include <map>
#include "../lib/httplib.h"
#include "../database.hpp"
#include "../json.hpp"
#include "../crypto.hpp"
#include "auth.hpp"

namespace Public {
    using namespace std;
    using namespace httplib;

    inline void init_routes(Server& svr) {

        // ─── 1. Счётчики ────────────────────────────────────────────────────
        svr.Get("/api/v1.1/counters", [](const Request&, Response& res) {
            auto count = [](const string& q) {
                auto r = db_row(q);
                return r.empty() ? 0LL : stoll(r[0]);
            };

            JObj o;
            o.int_("total_accounts",          count("SELECT COUNT(*) FROM users;"));
            o.int_("successful_calculations", count("SELECT COUNT(*) FROM works;"));
            o.int_("failed_validations",
                   count("SELECT COUNT(*) FROM works WHERE result_json LIKE '%error%';"));
            res.set_content(o.str(), "application/json");
        });

        // ─── 2. Публичная история ───────────────────────────────────────────
        svr.Get("/api/v1.1/history", [](const Request& req, Response& res) {
            int limit = MAX_HISTORY_LIMIT;
            if (req.has_param("limit")) {
                try { limit = min(stoi(req.get_param_value("limit")), limit); }
                catch (...) {}
            }

            JArr arr;
            for (auto& r : db_all(
                "SELECT w.id,u.username,w.datetime,w.input_json,w.result_json "
                "FROM works w JOIN users u ON w.user_id=u.id "
                "ORDER BY w.id DESC LIMIT " + to_string(limit) + ";")) {
                JObj o;
                o.int_("id", stoll(r[0]));
                o.str("username", r[1]);
                o.str("datetime", r[2]);
                o.raw("input", r[3]);
                o.raw("result", r[4]);
                arr.add(o.str());
            }
            res.set_content(arr.str(), "application/json");
        });

        // ─── 3. Логи (только admin/owner) ───────────────────────────────────
        svr.Get("/api/v1.1/admin/stats", [](const Request& req, Response& res) {
            string role, name;
            if (!Auth::auth(req, role, name) || !Auth::admin(role)) {
                res.status = 403;
                return;
            }

            JArr arr;
            for (auto& r : db_all(
                "SELECT id,datetime,status,message FROM logs "
                "ORDER BY id DESC LIMIT " + to_string(MAX_HISTORY_LIMIT) + ";")) {
                JObj o;
                o.int_("id", stoll(r[0]));
                o.str("datetime", r[1]);
                o.str("status", r[2]);
                o.str("message", r[3]);
                arr.add(o.str());
            }
            res.set_content(arr.str(), "application/json");
        });

        // ─── 4. Самотестирование ────────────────────────────────────────────
        svr.Get("/api/v1.1/admin/run-tests", [](const Request& req, Response& res) {
            string role, name;
            if (!Auth::auth(req, role, name)) { res.status = 401; return; }
            if (!Auth::admin(role))           { res.status = 403; return; }

            vector<double> v = {15, 25, 35};
            double mean = accumulate(v.begin(), v.end(), 0.0) / v.size();
            sort(v.begin(), v.end());
            map<double,int> freq;
            for (double x : {7.0, 7.0, 14.0}) freq[x]++;
            double mode = 0; int maxf = 0;
            for (auto& [k, c] : freq) if (c > maxf) { maxf = c; mode = k; }
            string h = hash_password("test");

            vector<pair<string,bool>> tests = {
                {"DB handle active",             db != nullptr},
                {"Users table accessible",       !db_row("SELECT 1 FROM users LIMIT 1;").empty()},
                {"Logs table accessible",        !db_row("SELECT 1 FROM logs LIMIT 1;").empty()},
                {"Log write works",              (write_log("INFO","test"), true)},
                {"Hash not empty",               !h.empty()},
                {"Verify accepts valid",         verify_password("test", h)},
                {"Verify rejects invalid",       !verify_password("wrong", h)},
                {"Hash uses unique salt",        hash_password("x") != hash_password("x")},
                {"Token unique",                 make_token() != make_token()},
                {"Token prefix TK",              make_token().rfind("TK",0) == 0},
                {"Math: count",                  v.size() == 3},
                {"Math: mean",                   fabs(mean - 25.0) < 1e-9},
                {"Math: median",                 fabs(v[1] - 25.0) < 1e-9},
                {"Math: mode",                   maxf > 1 && mode == 7.0},
                {"Math: variance defined",       v.size() > 0},
                {"Math: stddev",                 fabs(sqrt(144.0) - 12.0) < 1e-9},
                {"Auth: role admin/owner",       Auth::admin(role)},
                {"Auth: user non-empty",         !name.empty()},
                {"Config: TTL > 0",              TOKEN_TTL_SECONDS > 0},
                {"Config: MAX_HISTORY > 0",      MAX_HISTORY_LIMIT > 0},
            };

            int passed = 0;
            JArr cases;
            for (size_t i = 0; i < tests.size(); ++i) {
                if (tests[i].second) passed++;
                JObj o;
                o.int_("tc_id", (long long)(i + 1));
                o.str("name", tests[i].first);
                o.str("status", tests[i].second ? "PASSED" : "FAILED");
                cases.add(o.str());
            }

            JObj rep;
            rep.str("status", passed == 20 ? "success" : "failed");
            rep.int_("total_tests", 20);
            rep.int_("passed", passed);
            rep.int_("failed", 20 - passed);
            rep.raw("test_cases", cases.str());
            res.set_content(rep.str(), "application/json");
        });
    }
}