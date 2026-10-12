#ifndef MIN_SQLITE3_H
#define MIN_SQLITE3_H

#ifndef _WIN32
#include <sqlite3.h>
inline bool load_sqlite_dll() { return true; }
#else
#include <windows.h>
#include <iostream>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct sqlite3 sqlite3;
typedef struct sqlite3_stmt sqlite3_stmt;
typedef void (*sqlite3_destructor_type)(void*);

#define SQLITE_OK           0
#define SQLITE_ROW          100
#define SQLITE_DONE         101
#define SQLITE_TRANSIENT    ((sqlite3_destructor_type)-1)

typedef int (*fn_open)(const char*, sqlite3**);
typedef int (*fn_close)(sqlite3*);
typedef const char* (*fn_errmsg)(sqlite3*);
typedef int (*fn_exec)(sqlite3*, const char*, int (*)(void*,int,char**,char**), void*, char**);
typedef int (*fn_prepare)(sqlite3*, const char*, int, sqlite3_stmt**, const char**);
typedef int (*fn_step)(sqlite3_stmt*);
typedef int (*fn_finalize)(sqlite3_stmt*);
typedef int (*fn_bind_int)(sqlite3_stmt*, int, int);
typedef int (*fn_bind_text)(sqlite3_stmt*, int, const char*, int, void(*)(void*));
typedef int (*fn_col_int)(sqlite3_stmt*, int);
typedef const unsigned char* (*fn_col_text)(sqlite3_stmt*, int);
typedef void (*fn_free)(void*);

inline fn_open sqlite3_open = nullptr; inline fn_close sqlite3_close = nullptr;
inline fn_errmsg sqlite3_errmsg = nullptr; inline fn_exec sqlite3_exec = nullptr;
inline fn_prepare sqlite3_prepare_v2 = nullptr; inline fn_step sqlite3_step = nullptr;
inline fn_finalize sqlite3_finalize = nullptr; inline fn_bind_int sqlite3_bind_int = nullptr;
inline fn_bind_text sqlite3_bind_text = nullptr; inline fn_col_int sqlite3_column_int = nullptr;
inline fn_col_text sqlite3_column_text = nullptr; inline fn_free sqlite3_free = nullptr;

inline bool load_sqlite_dll() {
    HMODULE hDll = LoadLibraryA("sqlite3.dll");
    if (!hDll) hDll = LoadLibraryA("winsqlite3.dll");
    if (!hDll) { std::cerr << "CRITICAL: sqlite3.dll not found!" << std::endl; return false; }
    sqlite3_open = (fn_open)GetProcAddress(hDll, "sqlite3_open");
    sqlite3_close = (fn_close)GetProcAddress(hDll, "sqlite3_close");
    sqlite3_errmsg = (fn_errmsg)GetProcAddress(hDll, "sqlite3_errmsg");
    sqlite3_exec = (fn_exec)GetProcAddress(hDll, "sqlite3_exec");
    sqlite3_prepare_v2 = (fn_prepare)GetProcAddress(hDll, "sqlite3_prepare_v2");
    sqlite3_step = (fn_step)GetProcAddress(hDll, "sqlite3_step");
    sqlite3_finalize = (fn_finalize)GetProcAddress(hDll, "sqlite3_finalize");
    sqlite3_bind_int = (fn_bind_int)GetProcAddress(hDll, "sqlite3_bind_int");
    sqlite3_bind_text = (fn_bind_text)GetProcAddress(hDll, "sqlite3_bind_text");
    sqlite3_column_int = (fn_col_int)GetProcAddress(hDll, "sqlite3_column_int");
    sqlite3_column_text = (fn_col_text)GetProcAddress(hDll, "sqlite3_column_text");
    sqlite3_free = (fn_free)GetProcAddress(hDll, "sqlite3_free");
    return sqlite3_open && sqlite3_close && sqlite3_prepare_v2;
}
#ifdef __cplusplus
}
#endif
#endif
#endif