#pragma once
// ============================================================================
// ВАРИАНТ: ROMI / ROAS (маркетинговая эффективность).
//
// Это ПРИМЕР реализации. Студент создаёт свой файл variant_mine.hpp
// по этому образцу и меняет только формулы в calculate().
//
// ВАЖНО: имя namespace должно остаться TaskVariant — так работает task.hpp.
// ============================================================================

#include <string>
#include <cmath>
#include "../../json.hpp"

namespace TaskVariant {

    // ------------------------------------------------------------------------
    // Структура входных данных. Соответствует полям JSON-запроса.
    // ------------------------------------------------------------------------
    struct Input {
        double marketing_spend;       // бюджет рекламной кампании
        double attributed_revenue;    // выручка с кампании
        double agency_fee;            // комиссия агентства
        double production_cost;       // затраты на креативы
        int    period_days;           // длительность кампании
    };

    // ------------------------------------------------------------------------
    // Структура выходных данных. Соответствует полям JSON-ответа.
    // ------------------------------------------------------------------------
    struct Output {
        double roas;                  // return on ad spend
        double net_marketing_profit;  // чистая прибыль
        double romi_percentage;       // ROMI в процентах
    };

    // ------------------------------------------------------------------------
    // Проверка входных данных.
    // Возвращает "" если всё ок, иначе — текст ошибки.
    // ------------------------------------------------------------------------
    inline std::string validate(const Input& in) {
        if (in.marketing_spend < 1000.0)
            return "marketing_spend must be >= 1000.0";
        if (in.attributed_revenue < 0.0)
            return "attributed_revenue must be >= 0.0";
        if (in.agency_fee < 0.0)
            return "agency_fee must be >= 0.0";
        if (in.production_cost < 0.0)
            return "production_cost must be >= 0.0";
        if (in.agency_fee + in.production_cost > in.marketing_spend * 2.0)
            return "agency_fee + production_cost must not exceed 2 * marketing_spend";
        if (in.period_days < 1 || in.period_days > 365)
            return "period_days must be in [1, 365]";
        return "";
    }

    // ------------------------------------------------------------------------
    // Основные расчёты. Здесь студент меняет формулы под свой вариант.
    // ------------------------------------------------------------------------
    inline Output calculate(const Input& in) {
        Output out{};

        // ROAS = выручка / бюджет
        out.roas = in.attributed_revenue / in.marketing_spend;

        // Чистая прибыль = выручка - (бюджет + комиссия + продакшн)
        out.net_marketing_profit = in.attributed_revenue
                                 - (in.marketing_spend + in.agency_fee + in.production_cost);

        // ROMI = (чистая прибыль / общие затраты) * 100%
        double total_cost = in.marketing_spend + in.agency_fee + in.production_cost;
        out.romi_percentage = (out.net_marketing_profit / total_cost) * 100.0;

        return out;
    }

    // ------------------------------------------------------------------------
    // Разбор JSON-тела запроса в Input.
    // Достаём числа по ключам. Простой парсер подходит для плоских JSON.
    // ------------------------------------------------------------------------
    inline Input parse(const std::string& body) {
        Input in{};

        auto get_d = [&](const std::string& key) -> double {
            auto p = body.find("\"" + key + "\"");
            if (p == std::string::npos) return 0.0;
            p = body.find(':', p);
            if (p == std::string::npos) return 0.0;
            try { return std::stod(body.substr(p + 1)); }
            catch (...) { return 0.0; }
        };

        in.marketing_spend    = get_d("marketing_spend");
        in.attributed_revenue = get_d("attributed_revenue");
        in.agency_fee         = get_d("agency_fee");
        in.production_cost    = get_d("production_cost");
        in.period_days        = (int)get_d("period_days");
        return in;
    }

    // ------------------------------------------------------------------------
    // Формирование JSON-ответа из Output.
    // ------------------------------------------------------------------------
    inline std::string to_json(const Output& out) {
        JObj o;
        o.num("roas", out.roas);
        o.num("net_marketing_profit", out.net_marketing_profit);
        o.num("romi_percentage", out.romi_percentage);
        return o.str();
    }

} // namespace TaskVariant