#pragma once
#include <string>
#include <chrono>

using namespace std;
using namespace chrono;

struct SessionData {
    string username;
    string role;
    steady_clock::time_point created_at;
};
