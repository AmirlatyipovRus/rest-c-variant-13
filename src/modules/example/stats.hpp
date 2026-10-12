#pragma once
// ============================================================================
// МОДУЛЬ EXAMPLE: статистический анализ (демонстрация).
// ============================================================================

#include <string>
#include <vector>
#include <sstream>
#include <numeric>
#include <cmath>
#include <algorithm>
#include <map>
#include "../../lib/httplib.h"
#include "../../database.hpp"
#include "../../json.hpp"
#include "../../routes/auth.hpp"

namespace Example {
    using namespace std;
    using namespace httplib;

    inline bool parse(const string& body, vector<double>& out, string& err) {
        auto a = body.find('[');
        auto b = body.rfind(']');
        if (a == string::npos || b == string::npos || b <= a) {
            err = "No array found";
            return false;
        }
        stringstream ss(body.substr(a + 1, b - a - 1));
        string tok;
        while (getline(ss, tok, ',')) {
            size_t s = tok.find_first_not_of(" \t");
            if (s == string::npos) continue;
            try { out.push_back(stod(tok.substr(s))); }
            catch (...) { err = "Element is not a number"; return false; }
        }
        if (out.empty()) { err = "Array is empty"; return false; }
        return true;
    }

    inline void init_routes(Server& svr) {

        // ─── POST /works/calc ───────────────────────────────────────────────
        svr.Post("/api/v1.1/works/calc", [](const Request& req, Response& res) {
            string role, name;
            int uid = Auth::auth(req, role, name);
            if (!uid) { res.status = 401; return; }

            vector<double> d;
            string err;
            if (!parse(req.body, d, err)) {
                JObj o; o.str("error", err);
                res.status = 400;
                res.set_content(o.str(), "application/json");
                return;
            }

            int n = (int)d.size();
            double mean = accumulate(d.begin(), d.end(), 0.0) / n;

            sort(d.begin(), d.end());
            double median = (n % 2) ? d[n / 2] : (d[n / 2 - 1] + d[n / 2]) / 2.0;

            double sq = 0;
            for (double x : d) sq += (x - mean) * (x - mean);
            double var = (n > 1) ? sq / (n - 1) : 0.0;

            map<double, int> freq;
            for (double x : d) freq[x]++;
            double mode = d[0];
            int maxf = 0;
            for (auto& [k, c] : freq) if (c > maxf) { maxf = c; mode = k; }

            JObj o;
            o.int_("count", n);
            o.num("mean", mean);
            o.num("median", median);
            if (maxf > 1) o.num("mode", mode); else o.null_("mode");
            o.num("variance", var);
            o.num("std_deviation", sqrt(var));

            string result = o.str();

            // Записываем с явным datetime (в C++), а не через SQL-функцию.
            db_run("INSERT INTO works (user_id,datetime,input_json,result_json) "
                   "VALUES (?,?,?,?);",
                   {to_string(uid), now_iso(), req.body, result});
            res.set_content(result, "application/json");
        });

        // ─── GET /works/history ─────────────────────────────────────────────
        svr.Get("/api/v1.1/works/history", [](const Request& req, Response& res) {
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