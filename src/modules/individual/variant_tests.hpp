#pragma once
#include "variant_mine.hpp"
#include <vector>
#include <utility>
namespace FleetTests {
inline std::vector<std::pair<std::string, bool>> run() {
    using namespace TaskVariant;
    const std::string sample = R"({"total_vehicles":20,"active_vehicles_day":15,"total_fuel_consumed":300,"total_km_driven":3000,"average_fuel_price":1.5})";
    auto valid = [&](const std::string& s) { return validate(parse(s)).empty(); };
    auto changed = [&](const std::string& key, const std::string& old, const std::string& value) {
        auto s = sample; const auto token = "\"" + key + "\":" + old;
        s.replace(s.find(token), token.size(), "\"" + key + "\":" + value); return s;
    };
    auto out = calculate(parse(sample));
    return {
        {"Control example: all three results", valid(sample) && std::fabs(out.utilization_rate_percentage-75)<1e-10 && std::fabs(out.average_consumption_per_100km-10)<1e-10 && std::fabs(out.cost_per_km-.15)<1e-10},
        {"Minimum fleet and inclusive 1500 km boundary", valid(R"({"total_vehicles":1,"active_vehicles_day":1,"total_fuel_consumed":1,"total_km_driven":1500,"average_fuel_price":1})")},
        {"All vehicles active", valid(changed("active_vehicles_day","15","20"))},
        {"Zero total vehicles", !valid(changed("total_vehicles","20","0"))},
        {"Negative total vehicles", !valid(changed("total_vehicles","20","-1"))},
        {"Zero active vehicles", !valid(changed("active_vehicles_day","15","0"))},
        {"Active exceeds fleet", !valid(changed("active_vehicles_day","15","21"))},
        {"Zero fuel", !valid(changed("total_fuel_consumed","300","0"))},
        {"Negative fuel", !valid(changed("total_fuel_consumed","300","-1"))},
        {"Zero distance", !valid(changed("total_km_driven","3000","0"))},
        {"Negative distance", !valid(changed("total_km_driven","3000","-1"))},
        {"Distance above 1500 per active vehicle", !valid(changed("total_km_driven","3000","22500.01"))},
        {"Zero price", !valid(changed("average_fuel_price","1.5","0"))},
        {"Negative price", !valid(changed("average_fuel_price","1.5","-1"))},
        {"Missing required field", !valid(R"({"total_vehicles":20})")},
        {"String instead of number", !valid(changed("total_fuel_consumed","300","\"300\""))},
        {"Fractional vehicle count", !valid(changed("total_vehicles","20","20.5"))},
        {"Integer overflow", !valid(changed("total_vehicles","20","2147483648"))},
        {"Malformed JSON and duplicate keys", !valid(sample+"junk") && !valid("{"+sample.substr(1,sample.size()-2)+",\"total_vehicles\":20}") && !valid("{}")},
        {"Exponent input and numeric overflow", valid(changed("total_fuel_consumed","300","3e2")) && !valid(changed("total_fuel_consumed","300","1e999")) && !valid(changed("total_fuel_consumed","300","1e308").replace(changed("total_fuel_consumed","300","1e308").find("3000"),4,"1e-200"))}
    };
}
}
