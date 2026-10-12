#pragma once
// ============================================================================
// Загрузчик SQLite.
//
// На Windows: подтягиваем функции из sqlite3.dll в рантайме через
//             LoadLibrary/GetProcAddress. Сам DLL кладём рядом с .exe.
// На Linux/macOS: просто подключаем системный <sqlite3.h> — он уже есть.
//
// Используется ТОЛЬКО этот набор функций. Если понадобится новая —
// добавьте её в оба блока (#ifdef _WIN32 и #else).
// ============================================================================

#ifdef _WIN32

#include <windows.h>
#include <iostream>

extern "C" {
    // --- Типы SQLite (дублируем, т.к. заголовок не подключён) ---
    typedef struct sqlite3      sqlite3;
    typedef struct sqlite3_stmt sqlite3_stmt;
    typedef void (*destructor_t)(void*);
    typedef long long i64;

    #define SQLITE_OK        0
    #define SQLITE_ROW       100
    #define SQLITE_DONE      101
    // SQLITE_TRANSIENT заставляет SQLite сделать копию строки —
    // это безопасно, когда буфер скоро освободится.
    #define SQLITE_TRANSIENT ((destructor_t)-1)

    // --- Указатели на функции (заполняются в load_sqlite_dll) ---
    inline int  (*sqlite3_open)(const char*, sqlite3**)                              = nullptr;
    inline int  (*sqlite3_close)(sqlite3*)                                           = nullptr;
    inline int  (*sqlite3_exec)(sqlite3*, const char*, void*, void*, char**)         = nullptr;
    inline int  (*sqlite3_prepare_v2)(sqlite3*, const char*, int, sqlite3_stmt**, const char**) = nullptr;
    inline int  (*sqlite3_step)(sqlite3_stmt*)                                       = nullptr;
    inline int  (*sqlite3_finalize)(sqlite3_stmt*)                                   = nullptr;
    inline int  (*sqlite3_bind_text)(sqlite3_stmt*, int, const char*, int, destructor_t) = nullptr;
    inline int  (*sqlite3_column_count)(sqlite3_stmt*)                               = nullptr;
    inline int  (*sqlite3_column_int)(sqlite3_stmt*, int)                            = nullptr;
    inline const unsigned char* (*sqlite3_column_text)(sqlite3_stmt*, int)           = nullptr;

    // --- Загрузка всех функций ---
    inline bool load_sqlite_dll() {
        // Сначала пробуем sqlite3.dll (стандартное имя),
        // потом winsqlite3.dll (встроена в Windows 10+).
        HMODULE h = LoadLibraryA("sqlite3.dll");
        if (!h) h = LoadLibraryA("winsqlite3.dll");
        if (!h) {
            std::cerr << "[FATAL] sqlite3.dll not found. "
                         "Place it next to the .exe." << std::endl;
            return false;
        }

        // decltype(...) избавляет от повторного указания типа функции
        sqlite3_open         = (decltype(sqlite3_open))         GetProcAddress(h, "sqlite3_open");
        sqlite3_close        = (decltype(sqlite3_close))        GetProcAddress(h, "sqlite3_close");
        sqlite3_exec         = (decltype(sqlite3_exec))         GetProcAddress(h, "sqlite3_exec");
        sqlite3_prepare_v2   = (decltype(sqlite3_prepare_v2))   GetProcAddress(h, "sqlite3_prepare_v2");
        sqlite3_step         = (decltype(sqlite3_step))         GetProcAddress(h, "sqlite3_step");
        sqlite3_finalize     = (decltype(sqlite3_finalize))     GetProcAddress(h, "sqlite3_finalize");
        sqlite3_bind_text    = (decltype(sqlite3_bind_text))    GetProcAddress(h, "sqlite3_bind_text");
        sqlite3_column_count = (decltype(sqlite3_column_count)) GetProcAddress(h, "sqlite3_column_count");
        sqlite3_column_int   = (decltype(sqlite3_column_int))   GetProcAddress(h, "sqlite3_column_int");
        sqlite3_column_text  = (decltype(sqlite3_column_text))  GetProcAddress(h, "sqlite3_column_text");

        // Минимальный набор — то, без чего проект не запустится
        if (!sqlite3_open || !sqlite3_prepare_v2 || !sqlite3_step) {
            std::cerr << "[FATAL] sqlite3.dll is missing required symbols." << std::endl;
            return false;
        }
        return true;
    }
}

#else // ---------------- Linux / macOS ----------------

#include <sqlite3.h>
// На Unix SQLite уже установлена системно (apt/brew) — грузить нечего.
inline bool load_sqlite_dll() { return true; }

#endif