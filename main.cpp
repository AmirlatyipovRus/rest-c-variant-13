#include <iostream>
#include <fstream>
#include <string>
#include <map>
#include <chrono>

#include "src/httplib.h"
#include "src/models.hpp"

using namespace std;
using namespace chrono;
using namespace httplib;

map<string, SessionData> active_sessions;
int TOKEN_TTL_SECONDS = 300;
bool AUTO_REFRESH_TOKEN = true;

void reply_with_file(const string& path, const string& mime_type, Response& res) {
    ifstream file(path);
    if (!file.is_open()) {
        res.status = 404;
        res.set_content("Specification file not found on server. Ensure 'data' folder is next to exe.", "text/plain; charset=utf-8");
        return;
    }
    string content((istreambuf_iterator<char>(file)), istreambuf_iterator<char>());
    res.set_content(content, mime_type);
}

// Импорт модулей по новой структуре папок
#include "src/routes/public_routes.hpp"
#include "src/routes/works_routes.hpp"
#include "src/routes/auth_routes.hpp"
#include "src/routes/admin_routes.hpp"

int main() {
    Server svr;
    srand(static_cast<unsigned int>(time(nullptr)));

    cout << "[INIT] Starting cross-platform HTTP server..." << endl;

    svr.set_pre_routing_handler([](const Request& req, Response& res) {
        string token = "";
        if (req.has_header("Authorization")) {
            string auth = req.get_header_value("Authorization");
            if (auth.find("Bearer ") == 0) token = auth.substr(7);
        }

        if (!token.empty()) {
            auto it = active_sessions.find(token);
            if (it != active_sessions.end()) {
                auto now = steady_clock::now();
                auto passed = duration_cast<seconds>(now - it->second.created_at).count();
                
                if (passed >= TOKEN_TTL_SECONDS) {
                    active_sessions.erase(it);
                } else {
                    if (AUTO_REFRESH_TOKEN) it->second.created_at = now;
                    const_cast<Request&>(req).path_params["authenticated_token"] = token;
                }
            }
        }
        return Server::HandlerResponse::Unhandled;
    });

    // Раздача статики из папки data/ согласно архиву
    svr.Get("/style.css", [](const Request&, Response& res) { reply_with_file("data/style.css", "text/css; charset=utf-8", res); });
    svr.Get("/", [](const Request&, Response& res) { reply_with_file("data/index.html", "text/html; charset=utf-8", res); });
    
    // Путь изменен под имя файла из архива: data/protected.html
    svr.Get("/docs/protected", [](const Request&, Response& res) { reply_with_file("data/protected.html", "text/html; charset=utf-8", res); });

    setup_public_routes(svr);
    setup_works_routes(svr);
    setup_auth_routes(svr);
    setup_admin_routes(svr);

    svr.set_error_handler([](const Request&, Response& res) {
        if (res.status == 404) {
            res.set_content("<h1>404 Not Found</h1><p>Specification route or file not found.</p>", "text/html; charset=utf-8");
        }
    });

    cout << "[RUN] Server is running successfully at: http://localhost:8080" << endl;
    svr.listen("127.0.0.1", 8080);
    return 0;
}
