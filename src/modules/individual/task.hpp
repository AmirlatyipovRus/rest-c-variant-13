#pragma once
// ============================================================================
// МОДУЛЬ INDIVIDUAL: каркас роута.
// Не содержит математики — только вызовы функций варианта.
// ============================================================================

#include <string>
#include "../../lib/httplib.h"
#include "../../database.hpp"
#include "../../json.hpp"
#include "../../routes/auth.hpp"

// ────────────────────────────────────────────────────────────────────────────
// ПОДКЛЮЧИТЕ СВОЙ ВАРИАНТ ЗДЕСЬ (замените одну строку).
// ────────────────────────────────────────────────────────────────────────────
#include "variant_mine.hpp"
#include "variant_tests.hpp"

namespace Individual {
    using namespace std;
    using namespace httplib;

    inline void init_routes(Server& svr) {

        svr.Get("/api/v1.1/admin/run-tests-individual", [](const Request& req, Response& res) {
            string role, name;
            if (!Auth::auth(req, role, name)) { res.status = 401; return; }
            if (!Auth::admin(role)) { res.status = 403; return; }
            JArr cases; int passed = 0;
            const auto tests = FleetTests::run();
            for (size_t i = 0; i < tests.size(); ++i) {
                JObj item; item.int_("tc_id", i + 1); item.str("name", tests[i].first);
                item.str("status", tests[i].second ? "PASSED" : "FAILED");
                cases.add(item.str()); if (tests[i].second) ++passed;
            }
            JObj report; report.str("status", passed == (int)tests.size() ? "success" : "failed");
            report.int_("total_tests", tests.size()); report.int_("passed", passed);
            report.int_("failed", tests.size()-passed); report.raw("test_cases", cases.str());
            res.set_content(report.str(), "application/json");
        });

        // ─── POST /works/calc-individual ────────────────────────────────────
        svr.Post("/api/v1.1/works/calc-individual", [](const Request& req, Response& res) {
            string role, name;
            int uid = Auth::auth(req, role, name);
            if (!uid) { res.status = 401; return; }

            auto in = TaskVariant::parse(req.body);

            string err = TaskVariant::validate(in);
            if (!err.empty()) {
                JObj o; o.str("error", err);
                res.status = 400;
                res.set_content(o.str(), "application/json");
                return;
            }

            auto out = TaskVariant::calculate(in);
            string result = TaskVariant::to_json(out);

            if (!db_run("INSERT INTO works (user_id,datetime,input_json,result_json) "
                   "VALUES (?,?,?,?);",
                   {to_string(uid), now_iso(), req.body, result})) {
                res.status = 500;
                res.set_content("{\"error\":\"Could not save result\"}", "application/json");
                return;
            }
            res.set_content(result, "application/json");
        });

        // ─── GET /works/history-individual ──────────────────────────────────
        svr.Get("/api/v1.1/works/history-individual", [](const Request& req, Response& res) {
            string role, name;
            int uid = Auth::auth(req, role, name);
            if (!uid) { res.status = 401; return; }

            int limit = MAX_HISTORY_LIMIT;
            if (req.has_param("limit")) {
                try { limit = min(stoi(req.get_param_value("limit")), limit); }
                catch (...) {}
            }

            JArr arr;
            for (auto& r : db_all(
                "SELECT id,datetime,input_json,result_json FROM works "
                "WHERE user_id=" + to_string(uid) +
                " ORDER BY id DESC LIMIT " + to_string(limit) + ";")) {
                JObj o;
                o.int_("id", stoll(r[0]));
                o.str("datetime", r[1]);
                o.raw("input", r[2]);
                o.raw("result", r[3]);
                arr.add(o.str());
            }
            res.set_content(arr.str(), "application/json");
        });
    }
}