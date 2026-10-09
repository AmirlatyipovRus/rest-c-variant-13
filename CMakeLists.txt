cmake_minimum_required(VERSION 3.15)
project(CppRestApiServer VERSION 1.1)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Threads REQUIRED)

# Добавляем папки src/ и src/routes/ в пути поиска заголовков
include_directories(src)
include_directories(src/routes)

# Сборка исполняемого файла из корня проекта
add_executable(${PROJECT_NAME} main.cpp)

target_link_libraries(${PROJECT_NAME} PRIVATE Threads::Threads)

# Автоматическая линковка сетевой библиотеки Winsock2 для Windows
if(WIN32)
    target_link_libraries(${PROJECT_NAME} PRIVATE ws2_32)
endif()
