cmake_minimum_required(VERSION 3.16)
project(segystack)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Пути к заголовкам
include_directories(include)

# Сбор исходников
file(GLOB_RECURSE SOURCES
    src/main.cpp
    src/Config.cpp   
    src/nmo/*.cpp
    src/sgylib/*.cpp
)

# Создание исполняемого файла
add_executable(segystack ${SOURCES})

# Флаги оптимизации и предупреждений
if (CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(segystack PRIVATE
        -O3                         # Агрессивная оптимизация
        -march=native               # Использовать возможности CPU
        -ffast-math                 # Быстрые, но неточные операции
        -Wall -Wextra -Wpedantic   # Все основные предупреждения
    )
endif()

# Поддержка OpenMP (если доступен)
find_package(OpenMP REQUIRED)
if(OpenMP_CXX_FOUND)
    target_link_libraries(segystack PUBLIC OpenMP::OpenMP_CXX)
    target_compile_definitions(segystack PRIVATE -DUSE_OPENMP)
endif()

