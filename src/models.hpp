#pragma once
#include <string>
#include <sstream>
#include <iomanip>
#include <iostream>
#include <algorithm>
#include "lib/sqlite3.h"
#include "lib/env_parser.hpp"

inline sqlite3* db = nullptr;

#ifdef _WIN32
#include <windows.h>
#include <wincrypt.h>
inline std::string sha256(const std::string& str) {
    HCRYPTPROV hProv = 0; HCRYPTHASH hHash = 0; BYTE rgbHash[32]; DWORD cbHash = 32; std::stringstream ss;
    if (CryptAcquireContext(&hProv, NULL, NULL, PROV_RSA_AES, CRYPT_VERIFYCONTEXT)) {
        if (CryptCreateHash(hProv, CALG_SHA_256, 0, 0, &hHash)) {
            if (CryptHashData(hHash, (const BYTE*)str.c_str(), str.length(), 0)) {
                if (CryptGetHashParam(hHash, HP_HASHVAL, rgbHash, &cbHash, 0)) {
                    for (DWORD i = 0; i < cbHash; i++) ss << std::hex << std::setw(2) << std::setfill('0') << (int)rgbHash[i];
                }
            }
            CryptDestroyHash(hHash);
        }
        CryptReleaseContext(hProv, 0);
    }
    return ss.str();
}
#else
inline std::string sha256(const std::string& str) {
    unsigned long hash = 5381; for (char c : str) hash = ((hash << 5) + hash) + c;
    std::stringstream ss; ss << std::hex << std::setw(16) << std::setfill('0') << hash;
    std::string partial = ss.str(); while(partial.length() < 64) partial += "f"; return partial;
}
#endif

struct QueryParams { int limit; int offset; std::string from; std::string to; };
template <typename T>
inline QueryParams parse_query_params(const T& req) {
    int max_l = app_env.get_int("MAX_HISTORY_LIMIT", 20);
    return {
        req.has_param("limit") ? std::min(std::stoi(req.get_param_value("limit")), max_l) : max_l,
        req.has_param("offset") ? std::stoi(req.get_param_value("offset")) : 0,
        req.has_param("date_from") ? req.get_param_value("date_from") : "1970-01-01",
        req.has_param("date_to") ? req.get_param_value("date_to") : "2099-12-31"
    };
}

inline bool init_database() {
    #if defined(_WIN32)
        system("if not exist data mkdir data");
    #else
        system("mkdir -p data");
    #endif
    if (sqlite3_open("data/rest_project.db", &db) != 0) return false;
    const char* sql = 
        "CREATE TABLE IF NOT EXISTS users (id INTEGER PRIMARY KEY AUTOINCREMENT, username TEXT UNIQUE, password TEXT, role TEXT);"
        "CREATE TABLE IF NOT EXISTS sessions (token TEXT PRIMARY KEY, user_id INTEGER, datetime DATETIME DEFAULT(datetime('now','localtime')), expires_at DATETIME);"
        "CREATE TABLE IF NOT EXISTS works (id INTEGER PRIMARY KEY AUTOINCREMENT, user_id INTEGER, datetime DATETIME DEFAULT(datetime('now','localtime')), input_json JSON, result_json JSON);"
        "CREATE TABLE IF NOT EXISTS logs (id INTEGER PRIMARY KEY AUTOINCREMENT, datetime DATETIME DEFAULT(datetime('now','localtime')), status TEXT, message TEXT);";
    sqlite3_exec(db, sql, nullptr, nullptr, nullptr);
    sqlite3_exec(db, ("INSERT OR IGNORE INTO users (id, username, password, role) VALUES (1, 'owner1', '" + sha256("owner1") + "', 'owner');").c_str(), nullptr, nullptr, nullptr);
    sqlite3_exec(db, ("INSERT OR IGNORE INTO users (id, username, password, role) VALUES (2, 'admin1', '" + sha256("admin1") + "', 'admin');").c_str(), nullptr, nullptr, nullptr);
    sqlite3_exec(db, ("INSERT OR IGNORE INTO users (id, username, password, role) VALUES (3, 'user1', '" + sha256("user1") + "', 'user');").c_str(), nullptr, nullptr, nullptr);
    return true;
}

inline void write_log(const std::string& status, const std::string& message) {
    std::string query = "INSERT INTO logs (status, message) VALUES (?, ?);";
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db, query.c_str(), -1, &stmt, nullptr) == 0) {
        sqlite3_bind_text(stmt, 1, status.c_str(), -1, nullptr); sqlite3_bind_text(stmt, 2, message.c_str(), -1, nullptr);
        sqlite3_step(stmt); sqlite3_finalize(stmt);
    }
}