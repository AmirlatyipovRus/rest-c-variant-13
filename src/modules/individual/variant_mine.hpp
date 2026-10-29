#pragma once
// Вариант 13: анализатор утилизации и эффективности автопарка.
#include <string>
#include <map>
#include <regex>
#include <cmath>
#include <limits>
#include <sstream>
#include <iomanip>
#include <locale>

namespace TaskVariant {
struct Input {
    int total_vehicles = 0, active_vehicles_day = 0;
    double total_fuel_consumed = 0, total_km_driven = 0, average_fuel_price = 0;
    std::string parse_error;
};
struct Output {
    double utilization_rate_percentage, average_consumption_per_100km, cost_per_km;
};
// Строгий плоский JSON-контракт: пять уникальных числовых полей.
inline Input parse(const std::string& body) {
    Input in;
    size_t pos = 0;
    auto ws = [&] { while (pos < body.size() &&
        (body[pos]==' ' || body[pos]=='\t' || body[pos]=='\r' || body[pos]=='\n')) ++pos; };
    auto fail = [&](const std::string& e) { in.parse_error = e; return in; };
    ws();
    if (pos == body.size() || body[pos++] != '{') return fail("Expected JSON object");
    std::map<std::string, double> values;
    static const std::regex field(R"json("([a-z_0-9]+)"\s*:\s*(-?(?:0|[1-9][0-9]*)(?:\.[0-9]+)?(?:[eE][+-]?[0-9]+)?))json");
    ws();
    if (pos < body.size() && body[pos] != '}') {
        while (true) {
            std::smatch m;
            const std::string tail = body.substr(pos);
            if (!std::regex_search(tail, m, field, std::regex_constants::match_continuous))
                return fail("Expected numeric field in JSON object");
            const std::string key = m[1];
            if (key != "total_vehicles" && key != "active_vehicles_day" &&
                key != "total_fuel_consumed" && key != "total_km_driven" && key != "average_fuel_price")
                return fail("Unknown field: " + key);
            if (values.count(key)) return fail("Duplicate field: " + key);
            std::istringstream number(m[2].str()); number.imbue(std::locale::classic());
            double value; number >> value;
            if (number.fail() || !std::isfinite(value)) return fail("Number out of range: " + key);
            values[key] = value;
            pos += m.length(); ws();
            if (pos < body.size() && body[pos] == ',') { ++pos; ws(); continue; }
            break;
        }
    }
    if (pos == body.size() || body[pos++] != '}') return fail("Expected closing brace");
    ws(); if (pos != body.size()) return fail("Unexpected content after JSON object");
    for (const auto& key : {"total_vehicles", "active_vehicles_day", "total_fuel_consumed", "total_km_driven", "average_fuel_price"})
        if (!values.count(key)) return fail(std::string("Missing field: ") + key);
    for (const auto& key : {"total_vehicles", "active_vehicles_day"}) {
        double v = values[key];
        if (v < 1 || v > std::numeric_limits<int>::max() || std::trunc(v) != v)
            return fail(std::string(key) + " must be a positive integer within int range");
    }
    in.total_vehicles = static_cast<int>(values["total_vehicles"]);
    in.active_vehicles_day = static_cast<int>(values["active_vehicles_day"]);
    in.total_fuel_consumed = values["total_fuel_consumed"];
    in.total_km_driven = values["total_km_driven"];
    in.average_fuel_price = values["average_fuel_price"];
    return in;
}
inline Output calculate(const Input& in) {
    return {static_cast<double>(in.active_vehicles_day) / in.total_vehicles * 100.0,
            static_cast<double>((static_cast<long double>(in.total_fuel_consumed) / in.total_km_driven) * 100.0L),
            static_cast<double>((static_cast<long double>(in.total_fuel_consumed) / in.total_km_driven) * in.average_fuel_price)};
}
inline std::string validate(const Input& in) {
    if (!in.parse_error.empty()) return in.parse_error;
    if (in.total_vehicles <= 0) return "total_vehicles must be > 0";
    if (in.active_vehicles_day < 1 || in.active_vehicles_day > in.total_vehicles)
        return "active_vehicles_day must be in [1, total_vehicles]";
    if (!std::isfinite(in.total_fuel_consumed) || in.total_fuel_consumed <= 0)
        return "total_fuel_consumed must be finite and > 0";
    if (!std::isfinite(in.total_km_driven) || in.total_km_driven <= 0)
        return "total_km_driven must be finite and > 0";
    if (in.total_km_driven / in.active_vehicles_day > 1500.0)
        return "total_km_driven / active_vehicles_day must be <= 1500 km";
    if (!std::isfinite(in.average_fuel_price) || in.average_fuel_price <= 0)
        return "average_fuel_price must be finite and > 0";
    // Проверка представимости без деления на ноль и переполнения double.
    const long double consumption = (static_cast<long double>(in.total_fuel_consumed) / in.total_km_driven) * 100.0L;
    const long double cost = (static_cast<long double>(in.total_fuel_consumed) / in.total_km_driven) * in.average_fuel_price;
    if (consumption > std::numeric_limits<double>::max() || cost > std::numeric_limits<double>::max())
        return "Calculated result exceeds double range";
    return "";
}
inline std::string to_json(const Output& out) {
    std::ostringstream s; s.imbue(std::locale::classic());
    s << std::setprecision(std::numeric_limits<double>::max_digits10)
      << "{\"utilization_rate_percentage\":" << out.utilization_rate_percentage
      << ",\"average_consumption_per_100km\":" << out.average_consumption_per_100km
      << ",\"cost_per_km\":" << out.cost_per_km << "}";
    return s.str();
}
}
