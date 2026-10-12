cmake_minimum_required(VERSION 3.10)
project(rest_c_server VERSION 1.0)

# ----------------------------------------------------------------------------
# Стандарт C++17 (нужен для inline-переменных и structured bindings)
# ----------------------------------------------------------------------------
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ----------------------------------------------------------------------------
# Собираем все .cpp и .hpp файлы проекта
# ----------------------------------------------------------------------------
file(GLOB_RECURSE SOURCES "main.cpp" "src/**/*.hpp" "src/**/*.cpp")
add_executable(rest_server ${SOURCES})

# ----------------------------------------------------------------------------
# Многопоточность (используется httplib.h)
# ----------------------------------------------------------------------------
find_package(Threads REQUIRED)
target_link_libraries(rest_server PRIVATE Threads::Threads)

# ----------------------------------------------------------------------------
# Платформенные зависимости
# ----------------------------------------------------------------------------
if(WIN32)
    # Windows: сокеты линкуются отдельно
    target_link_libraries(rest_server PRIVATE ws2_32)
else()
    # Linux / macOS: системная SQLite
    find_package(SQLite3 REQUIRED)
    target_link_libraries(rest_server PRIVATE SQLite3::SQLite3)
endif()