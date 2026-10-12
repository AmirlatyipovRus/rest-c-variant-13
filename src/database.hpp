#pragma once
// ============================================================================
// Работа с базой данных SQLite.
//
// Три хелпера:
//   db_run(sql, args)  — выполнить запрос без результата (INSERT/UPDATE/DELETE)
//   db_all(sql, args)  — все строки результата
//   db_row(sql, args)  — первая строка (или пустой вектор)
//
// Все запросы параметризованы через '?'. Параллельный доступ защищён мьютексом.
// Время вычисляется в C++, а не в SQL — чтобы не зависеть от версии SQLite.
// ============================================================================

#include <string>
#include <vector>
#include <mutex>
#include <iostream>
#include <ctime>
#include "lib/sqlite_loader.hpp"
#include "crypto.hpp"

// ---------------------------------------------------------------------------
// Глобальные ресурсы.
// ---------------------------------------------------------------------------
inline sqlite3*   db      = nullptr;
inline std::mutex db_lock;

// ---------------------------------------------------------------------------
// Выполнить запрос без результата.
// ---------------------------------------------------------------------------
inline bool db_run(const std::string& sql,
                   const std::vector<std::string>& args = {}) {
    std::lock_guard<std::mutex> g(db_lock);
    sqlite3_stmt* s = nullptr;

    if (sqlite3_prepare_v2(db, sql.c_str(), -1, &s, nullptr) != SQLITE_OK) {
        std::cerr << "[DB] prepare failed: " << sql << std::endl;
        return false;
    }

    for (size_t i = 0; i < args.size(); ++i)
        sqlite3_bind_text(s, (int)i + 1, args[i].c_str(), -1, SQLITE_TRANSIENT);

    bool ok = sqlite3_step(s) == SQLITE_DONE;
    if (!ok) std::cerr << "[DB] step failed: " << sql << std::endl;

    sqlite3_finalize(s);
    return ok;
}

// ---------------------------------------------------------------------------
// Все строки результата.
// ---------------------------------------------------------------------------
inline std::vector<std::vector<std::string>> db_all(
        const std::string& sql, const std::vector<std::string>& args = {}) {
    std::lock_guard<std::mutex> g(db_lock);
    std::vector<std::vector<std::string>> out;
    sqlite3_stmt* s = nullptr;

    if (sqlite3_prepare_v2(db, sql.c_str(), -1, &s, nullptr) != SQLITE_OK) {
        std::cerr << "[DB] prepare failed: " << sql << std::endl;
        return out;
    }

    for (size_t i = 0; i < args.size(); ++i)
        sqlite3_bind_text(s, (int)i + 1, args[i].c_str(), -1, SQLITE_TRANSIENT);

    int cols = sqlite3_column_count(s);
    while (sqlite3_step(s) == SQLITE_ROW) {
        std::vector<std::string> row;
        row.reserve(cols);
        for (int i = 0; i < cols; ++i) {
            auto t = sqlite3_column_text(s, i);
            row.push_back(t ? (const char*)t : "");
        }
        out.push_back(std::move(row));
    }

    sqlite3_finalize(s);
    return out;
}

// ---------------------------------------------------------------------------
// Первая строка результата или пустой вектор.
// ---------------------------------------------------------------------------
inline std::vector<std::string> db_row(const std::string& sql,
                                        const std::vector<std::string>& args = {}) {
    auto all = db_all(sql, args);
    return all.empty() ? std::vector<std::string>{} : std::move(all[0]);
}

// ---------------------------------------------------------------------------
// Запись в системный лог.
// ---------------------------------------------------------------------------
inline void write_log(const std::string& status, const std::string& msg) {
    db_run("INSERT INTO logs (status,message) VALUES (?,?);", {status, msg});
}

// ---------------------------------------------------------------------------
// Время: вычисляем в C++, не доверяем SQLite-функциям datetime().
//
// Формат "YYYY-MM-DD HH:MM:SS" — лексикографически сравним, что удобно.
// ---------------------------------------------------------------------------
inline std::string now_iso() {
    std::time_t t = std::time(nullptr);
    std::tm tm_buf{};
#if defined(_WIN32)
    localtime_s(&tm_buf, &t);
#else
    localtime_r(&t, &tm_buf);
#endif
    char buf[32];
    std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &tm_buf);
    return std::string(buf);
}

inline std::string future_iso(int seconds) {
    std::time_t t = std::time(nullptr) + seconds;
    std::tm tm_buf{};
#if defined(_WIN32)
    localtime_s(&tm_buf, &t);
#else
    localtime_r(&t, &tm_buf);
#endif
    char buf[32];
    std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &tm_buf);
    return std::string(buf);
}

// ---------------------------------------------------------------------------
// Инициализация БД: схема + стартовые пользователи.
// ---------------------------------------------------------------------------
inline bool init_database() {
#if defined(_WIN32)
    system("if not exist data mkdir data");
#else
    system("mkdir -p data");
#endif

    if (sqlite3_open("data/rest.db", &db) != 0) {
        std::cerr << "[DB] cannot open data/rest.db" << std::endl;
        return false;
    }

    sqlite3_exec(db, "PRAGMA busy_timeout=3000;", nullptr, nullptr, nullptr);

    sqlite3_exec(db,
        "CREATE TABLE IF NOT EXISTS users ("
        "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  username TEXT UNIQUE,"
        "  password TEXT,"
        "  role TEXT DEFAULT 'user');"
        "CREATE TABLE IF NOT EXISTS sessions ("
        "  token TEXT PRIMARY KEY,"
        "  user_id INTEGER,"
        "  expires_at TEXT);"
        "CREATE TABLE IF NOT EXISTS works ("
        "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  user_id INTEGER,"
        "  datetime TEXT,"
        "  input_json TEXT,"
        "  result_json TEXT);"
        "CREATE TABLE IF NOT EXISTS logs ("
        "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  datetime TEXT,"
        "  status TEXT,"
        "  message TEXT);",
        nullptr, nullptr, nullptr);

    const char* seeds[][3] = {
        {"owner1", "owner1", "owner"},
        {"admin1", "admin1", "admin"},
        {"user1",  "user1",  "user"}
    };
    for (auto& s : seeds)
        db_run("INSERT OR IGNORE INTO users (username,password,role) VALUES (?,?,?);",
               {s[0], hash_password(s[1]), s[2]});

    return true;
}

// ---------------------------------------------------------------------------
// КОНФИГУРАЦИЯ.
// ---------------------------------------------------------------------------
inline constexpr int TOKEN_TTL_SECONDS = 300;
inline constexpr int MAX_HISTORY_LIMIT = 20;