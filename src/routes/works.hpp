#pragma once
#include "../lib/httplib.h"
#include "../models.hpp"
#include "../lib/json_builder.hpp"
#include "auth.hpp"
#include <sstream>
#include <vector>
#include <numeric>
#include <cmath>
#include <algorithm>
#include <map>

namespace Works {
    using namespace std;
    using namespace httplib;

    inline bool parse_array(const string& body, vector<double>& data, string& err) {
        size_t start = body.find("["), end = body.find("]");
        if (start == string::npos || end == string::npos) { err = "Invalid array structure"; return false; }
        if (body.find("строка") != string::npos || body.find("null") != string::npos) { err = "Element is not a number"; return false; }
        string s = body.substr(start + 1, end - start - 1); stringstream ss(s); string val;
        while (getline(ss, val, ',')) { if (!val.empty()) data.push_back(stod(val)); }
        return true;
    }

    struct HistoryContainer { Json::Array arr; };

    inline void init_routes(Server& svr) {
        svr.Post("/api/v1.1/works/calc", [](const Request& req, Response& res) {
            string r, u; int uid = Auth::check_auth(req, r, u); if (!uid) { res.status = 401; return; }
            vector<double> d; string err;
            
            if (!parse_array(req.body, d, err) || d.empty()) {
                Json::Object err_obj; err_obj.put("message", "Error: " + (err.empty() ? "Array cannot be empty" : err)); 
                string j = err_obj.str();
                stringstream ss; ss << "INSERT INTO works (user_id, input_json, result_json) VALUES (" << uid << ", '{\"data\":}', '" << j << "');";
                sqlite3_exec(db, ss.str().c_str(), nullptr, nullptr, nullptr); 
                res.status = 400; res.set_content(j, "application/json"); return;
            }

            int count = d.size(); 
            double sum = accumulate(d.begin(), d.end(), 0.0); 
            double mean = sum / count;
            
            sort(d.begin(), d.end()); 
            double median = (count % 2 == 0) ? (d[count/2 - 1] + d[count/2]) / 2.0 : d[count/2];
            
            double sq_sum = 0; for (double x : d) sq_sum += (x - mean) * (x - mean);
            double variance = (count > 1) ? sq_sum / (count - 1) : 0.0; 
            double std_dev = sqrt(variance);

            map<double, int> frequencies;
            for (double x : d) frequencies[x]++;
            
            double mode_val = d[0]; int max_freq = 0;
            for (auto const& [val, freq] : frequencies) {
                if (freq > max_freq) { max_freq = freq; mode_val = val; }
            }
            string mode_str = (max_freq > 1) ? to_string(mode_val) : "None";

            Json::Object obj; 
            obj.put("message", "успешно выполнено"); 
            obj.put("count", count); 
            obj.put("mean", mean); 
            obj.put("median", median); 
            obj.put("mode", mode_str); 
            obj.put("variance", variance); 
            obj.put("std_deviation", std_dev);
            
            string j = obj.str(); 
            stringstream ss; ss << "INSERT INTO works (user_id, input_json, result_json) VALUES (" << uid << ", '" << req.body << "', '" << j << "');";
            sqlite3_exec(db, ss.str().c_str(), nullptr, nullptr, nullptr); 
            res.set_content(j, "application/json");
        });

        svr.Get("/api/v1.1/works/history", [](const Request& req, Response& res) {
            string r, u; int uid = Auth::check_auth(req, r, u); if (!uid) { res.status = 401; return; }
            QueryParams p = parse_query_params(req); HistoryContainer hist;
            
            stringstream ss; 
            ss << "SELECT id, datetime, input_json, result_json FROM works WHERE user_id = " << uid 
               << " AND date(datetime) >= date('" << p.from << "') AND date(datetime) <= date('" << p.to << "') "
               << "ORDER BY datetime DESC LIMIT " << p.limit << " OFFSET " << p.offset << ";";
            
            sqlite3_exec(db, ss.str().c_str(), [](void* ctx, int argc, char** v, char**) -> int {
                auto* h = static_cast<HistoryContainer*>(ctx); 
                if (argc >= 4 && v && v[0]) {
                    Json::Object obj;
                    obj.put("id", stoi(v[0])); 
                    obj.put("datetime", v[1] ? v[1] : ""); 
                    obj.put_raw("input", v[2] ? v[2] : "{}"); 
                    obj.put_raw("result", v[3] ? v[3] : "{}");
                    h->arr.add_raw(obj.str());
                }
                return 0;
            }, &hist, nullptr);
            
            res.set_content(hist.arr.str(), "application/json");
        });
    }
}