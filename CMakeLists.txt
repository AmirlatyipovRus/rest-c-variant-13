cmake_minimum_required(VERSION 3.10)
project(rest_c_server VERSION 1.1)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Поиск всех исходных файлов бэкенда в папках
file(GLOB_RECURSE SOURCES "main.cpp" "src/routes/*.hpp")
add_executable(rest_server_bin ${SOURCES})

# Подключение библиотеки потоков
find_package(Threads REQUIRED)
target_link_libraries(rest_server_bin PRIVATE Threads::Threads)

if(WIN32)
    # Линковка системной сетевой библиотеки для Windows сокетов
    target_link_libraries(rest_server_bin PRIVATE "ws2_32")
else()
    # Автопоиск нативной системной СУБД для macOS и Linux
    find_package(SQLite3 REQUIRED)
    target_link_libraries(rest_server_bin PRIVATE SQLite3::SQLite3)
endif()