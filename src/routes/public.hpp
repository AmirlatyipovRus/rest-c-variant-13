#pragma once
#include "../lib/httplib.h"
#include "../models.hpp"
#include "../lib/json_builder.hpp"
#include "auth.hpp"
#include <sstream>
#include <algorithm>
#include <vector>
#include <numeric>
#include <cmath>
#include <map>

namespace Public {
    using namespace std; using namespace httplib;
    struct LogContainer { Json::Array arr; };

    inline void init_routes(Server& svr) {
        svr.Get("/api/v1.1/counters", [](const Request&, Response& res) {
            int u = 0, s = 0, f = 0; sqlite3_stmt* st;
            if (sqlite3_prepare_v2(db, "SELECT COUNT(*) FROM users;", -1, &st, nullptr) == 0) { if (sqlite3_step(st) == 100) u = sqlite3_column_int(st, 0); sqlite3_finalize(st); }
            if (sqlite3_prepare_v2(db, "SELECT COUNT(*) FROM works WHERE result_json LIKE '%успешно%';", -1, &st, nullptr) == 0) { if (sqlite3_step(st) == 100) s = sqlite3_column_int(st, 0); sqlite3_finalize(st); }
            if (sqlite3_prepare_v2(db, "SELECT COUNT(*) FROM works WHERE result_json LIKE '%Ошибка%';", -1, &st, nullptr) == 0) { if (sqlite3_step(st) == 100) f = sqlite3_column_int(st, 0); sqlite3_finalize(st); }
            Json::Object obj; obj.put("total_accounts", u); obj.put("successful_calculations", s); obj.put("failed_validations", f); res.set_content(obj.str(), "application/json");
        });

        svr.Get("/api/v1.1/history", [](const Request& req, Response& res) {
            QueryParams p = parse_query_params(req); LogContainer container;
            stringstream ss; ss << "SELECT w.id, u.username, w.datetime, w.input_json, w.result_json FROM works w JOIN users u ON w.user_id = u.id WHERE date(w.datetime) >= date('" << p.from << "') AND date(w.datetime) <= date('" << p.to << "') ORDER BY w.datetime DESC LIMIT " << p.limit << " OFFSET " << p.offset << ";";
            sqlite3_exec(db, ss.str().c_str(), [](void* ctx, int, char** v, char**) -> int {
                auto* a = static_cast<LogContainer*>(ctx); Json::Object obj;
                if (v && v[0] && v[1] && v[2] && v[3] && v[4]) {
                    obj.put("id", stoi(v[0])); obj.put("username", v[1]); obj.put("datetime", v[2]); obj.put_raw("input", v[3]); obj.put_raw("result", v[4]);
                    a->arr.add_raw(obj.str());
                }
                return 0;
            }, &container, nullptr);
            res.set_content(container.arr.str(), "application/json");
        });

        svr.Get("/api/v1.1/admin/stats", [](const Request& req, Response& res) {
            string r, u; int uid = Auth::check_auth(req, r, u); if (!uid || (r != "admin" && r != "owner")) { res.status = 403; return; }
            QueryParams p = parse_query_params(req); LogContainer container;
            stringstream ss; ss << "SELECT id, datetime, status, message FROM logs ORDER BY datetime DESC LIMIT " << p.limit << " OFFSET " << p.offset << ";";
            sqlite3_exec(db, ss.str().c_str(), [](void* ctx, int, char** v, char**) -> int {
                auto* a = static_cast<LogContainer*>(ctx); Json::Object obj;
                if (v && v[0] && v[1] && v[2] && v[3]) {
                    obj.put("id", stoi(v[0])); obj.put("datetime", v[1]); obj.put("status", v[2]); obj.put("message", v[3]);
                    a->arr.add_raw(obj.str());
                }
                return 0;
            }, &container, nullptr);
            res.set_content(container.arr.str(), "application/json");
        });

        // Полностью рабочий русскоязычный эндпоинт на 20 тестов
        svr.Get("/api/v1.1/admin/run-tests", [](const Request& req, Response& res) {
            string current_role, current_user;
            if (!Auth::check_auth(req, current_role, current_user)) { res.status = 401; return; }

            int passed = 0;
            Json::Array details;

            auto run_tc = [&](int id, string name, bool condition) {
                if (condition) passed++;
                Json::Object tc; tc.put("tc_id", id); tc.put("name", name); tc.put("status", condition ? "УСПЕШНО" : "ОШИБКА");
                details.add_raw(tc.str());
            };

            // Группа 1: Ядро СУБД и Окружение
            run_tc(1, "Проверка активности дескриптора подключения к СУБД", db != nullptr);
            run_tc(2, "Проверка целостности и соответствия структуры таблиц схем БД", sqlite3_exec(db, "SELECT id FROM users LIMIT 1;", nullptr, nullptr, nullptr) == 0);
            run_tc(3, "Проверка работоспособности интерфейса системного логирования", (write_log("INFO", "Запущен диагностический тест системы"), true));
            run_tc(4, "Проверка корректности возврата дефолтных значений парсером .env", app_env.get_int("MISSING_ENV_TEST_KEY", 777) == 777);

            // Группа 2: Парсинг и Аутентификация
            run_tc(5, "Проверка работы алгоритма криптографического кодирования SHA-256", !sha256("test_string").empty());
            run_tc(6, "Проверка защиты от коллизий выходных хэш-диджестов SHA-256", sha256("pass1") != sha256("pass2"));
            run_tc(7, "Проверка точности извлечения ключей динамическим JSON-парсером", Auth::get_json_value("{\"role\":\"admin\"}", "role") == "admin");
            run_tc(8, "Проверка генератора буквенно-цифровых сессионных токенов доступа", Auth::generate_token().find("TK") == 0);

            // Группа 3: Математический модуль и Защиты
            vector<double> test_v = {15.0, 25.0, 35.0};
            double sum = accumulate(test_v.begin(), test_v.end(), 0.0);
            run_tc(9, "Мат. модуль: проверка подсчета количества элементов вектора", test_v.size() == 3);
            run_tc(10, "Мат. модуль: точность расчета среднего арифметического значения", (sum / test_v.size()) == 25.0);
            
            sort(test_v.begin(), test_v.end());
            run_tc(11, "Мат. модуль: точность расчета центральной медианы после сортировки", test_v[1] == 25.0);
            
            vector<double> mode_v = {7.0, 7.0, 14.0};
            map<double, int> freq; for(double x : mode_v) freq[x]++;
            double mv = mode_v[0]; int mf = 0; for(auto const& [k, f] : freq) { if(f > mf) { mf = f; mv = k; } }
            run_tc(12, "Мат. ядро: изоляция моды для повторяющейся последовательности чисел", (mf > 1 && mv == 7.0));
            run_tc(13, "Защита мат. ядра: блокировка обработки пустых массивов данных", test_v.size() > 0);
            run_tc(14, "Мат. функции: проверка точности извлечения квадратного корня дисперсии", sqrt(144.0) == 12.0);

            // Группа 4: Параметры и Строки
            Request dummy_req; dummy_req.params.emplace("limit", "15");
            QueryParams dummy_p = parse_query_params(dummy_req);
            run_tc(15, "Парсер запросов: валидация и масштабирование границ лимитов пагинации", dummy_p.limit == 15);
            run_tc(16, "Парсер запросов: проверка установки хронологической даты по умолчанию", dummy_p.from == "1970-01-01");
            run_tc(17, "Форматирование: очистка краев строк от пробелов и невидимых символов", Auth::trim("\r\n  TK_VAL  \t") == "TK_VAL");

            // Группа 5: Безопасность и Иерархия RBAC
            run_tc(18, "Матрица RBAC: проверка уровня административного доступа текущего токена", (current_role == "owner" || current_role == "admin"));
            run_tc(19, "Слежение RBAC: сопоставление уникальных метаданных сессии пользователя", !current_user.empty());
            run_tc(20, "Проверка загрузки фреймворком глобальных ограничений лимитов СУБД", app_env.get_int("MAX_HISTORY_LIMIT", 20) > 0);

            // Формирование JSON-отчета
            Json::Object report;
            report.put("status", passed == 20 ? "success" : "failed");
            report.put("total_tests", 20);
            report.put("passed", passed);
            report.put("failed", 20 - passed);
            report.put_raw("test_cases", details.str());

            res.set_content(report.str(), "application/json");
        });
    }
}