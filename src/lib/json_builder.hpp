#pragma once
#include <string>
#include <sstream>
#include <map>
#include <vector>

namespace Json {
    using namespace std;

    class Object {
    private:
        map<string, string> fields;
    public:
        void put(const string& key, const string& val) { fields[key] = "\"" + val + "\""; }
        void put(const string& key, double val)        { fields[key] = to_string(val); }
        void put(const string& key, int val)           { fields[key] = to_string(val); }
        void put_raw(const string& key, const string& raw) { fields[key] = raw.empty() ? "{}" : raw; }

        string str() const {
            stringstream ss; ss << "{"; bool first = true;
            for (auto const& [key, val] : fields) {
                if (!first) ss << ","; first = false;
                ss << "\"" << key << "\":" << val;
            }
            ss << "}"; return ss.str();
        }
    };

    class Array {
    private:
        vector<string> items;
    public:
        void add_raw(const string& raw) { items.push_back(raw); }
        string str() const {
            stringstream ss; ss << "[";
            for (size_t i = 0; i < items.size(); ++i) {
                if (i > 0) ss << ","; ss << items[i];
            }
            ss << "]"; return ss.str();
        }
    };
}