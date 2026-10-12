#include "src/lib/httplib.h"
#include "src/models.hpp"
#include "src/lib/env_parser.hpp"
#include "src/routes/auth.hpp"
#include "src/routes/works.hpp"
#include "src/routes/public.hpp"
#include <iostream>

int main() {
    if (!app_env.load(".env")) std::cout << "[WARN] .env file not found." << std::endl;
    if (!load_sqlite_dll()) return 1;
    if (!init_database()) return 1;
    
    sqlite3_exec(db, "DELETE FROM sessions;", nullptr, nullptr, nullptr);
    httplib::Server svr;

    Auth::init_routes(svr);
    Works::init_routes(svr);
    Public::init_routes(svr);

    std::cout << "[SUCCESS] Server is listening on http://localhost:8080" << std::endl;
    svr.listen("0.0.0.0", 8080);

    if (db) sqlite3_close(db);
    return 0;
}