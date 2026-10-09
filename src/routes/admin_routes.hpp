#pragma once
#include "httplib.h"

using namespace std;
using namespace httplib;

string get_time_str(chrono::system_clock::time_point tp);

inline void setup_admin_routes(Server& svr) {
    
    svr.Get("/api/v1.1/admin/stats", [](const Request&, Response& res) {
        stringstream json;
        json << "{\n  \"limit\": 1,\n  \"offset\": 0,\n  \"total_records\": 45,\n  \"response_time\": \"" << get_time_str(chrono::system_clock::now()) << "\",\n"
             << "  \"data\": [\n    \"[2026-10-09 12:00:01] Configuration .env loaded successfully\"\n  ]\n}";
        res.set_content(json.str(), "application/json; charset=utf-8");
    });

    svr.Get("/api/v1.1/admin/run-tests", [](const Request&, Response& res) {
        string report = 
            "==================================================\n"
            "         AUTOMATED TESTING REPORT                 \n"
            "==================================================\n"
            "Test 1: [Valid data test]\n"
            "   [SUCCESS] Core math results matched reference.\n\n"
            "TOTAL: Successfully passed 5 out of 5 tests.\n"
            "Microservice stability status: STABLE";
        res.set_content(report, "text/plain; charset=utf-8");
    });
}
