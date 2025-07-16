cmake_minimum_required(VERSION 3.16)
project(segystack LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# --- 1. Поиск зависимостей ---
find_package(OpenMP REQUIRED)
find_package(SQLite3 REQUIRED)

# --- 2. Определение цели и ее исходных файлов ---
add_executable(segystack
    src/main.cpp
    src/Config.cpp
    src/nmo/nmo.cpp
    src/sgylib/SegyReader.cpp
    src/sgylib/SegyWriter.cpp
    src/sgylib/TraceMap.cpp
)

# --- 3. Настройка путей к заголовочным файлам ---
target_include_directories(segystack PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include 
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

# --- 4. Настройка флагов компиляции ---
target_compile_options(segystack PRIVATE
    -O3
    -march=native
    -ffast-math
    -Wall
    -Wextra
    -Wpedantic  
)

# --- 5. Линковка с библиотеками ---
# ИСПРАВЛЕНИЕ: Используем более надежный подход для линковки с filesystem
# Сначала пытаемся скомпилировать небольшой тестовый код
include(CheckCXXSourceCompiles)
check_cxx_source_compiles("
    #include <filesystem>
    int main() { std::filesystem::exists(\".\"); return 0; }
" CXX_FILESYSTEM_WORKS_WITHOUT_LIBS)

# Линкуем все необходимые библиотеки с нашей целью
target_link_libraries(segystack PRIVATE
    OpenMP::OpenMP_CXX
    SQLite::SQLite3
)

# Если тестовый код не скомпилировался, добавляем явную линковку
if(NOT CXX_FILESYSTEM_WORKS_WITHOUT_LIBS)
    target_link_libraries(segystack PRIVATE stdc++fs)
    message(STATUS "Note: Explicitly linking with stdc++fs for <filesystem> support.")
endif()


# --- 6. Вывод полезной информации ---
message(STATUS "Compiler: ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}")
message(STATUS "OpenMP found: ${OpenMP_CXX_FOUND}")
message(STATUS "SQLite3 found: ${SQLite3_FOUND}")