cmake_minimum_required(VERSION 3.21)

# Must be set before project() to take effect.
if(NOT DEFINED CMAKE_TOOLCHAIN_FILE)
    set(CMAKE_TOOLCHAIN_FILE "${CMAKE_CURRENT_SOURCE_DIR}/external/vcpkg/scripts/buildsystems/vcpkg.cmake"
        CACHE STRING "Vcpkg toolchain file")
endif()

project(lyra C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

if(MSVC)
    add_compile_options(/W4)
    add_compile_definitions(_CRT_SECURE_NO_WARNINGS)
else()
    add_compile_options(-Wall -Wextra)
endif()

find_path(UTHASH_INCLUDE_DIR NAMES uthash.h REQUIRED)
find_package(Threads REQUIRED)
find_package(replxx CONFIG REQUIRED)
find_package(check CONFIG REQUIRED)
set(CHECK_LIBRARY $<IF:$<TARGET_EXISTS:Check::check>,Check::check,Check::checkShared>)

file(GLOB_RECURSE LYRA_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/src/*.c")
list(REMOVE_ITEM LYRA_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/src/main.c")

# Core interpreter, shared by the CLI executable and the test binaries.
add_library(lyra_core STATIC ${LYRA_SOURCES})
target_include_directories(lyra_core PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/src" ${UTHASH_INCLUDE_DIR})

add_executable(lyra src/main.c)
target_link_libraries(lyra PRIVATE lyra_core replxx::replxx)

if(NOT WIN32)
    target_link_libraries(lyra_core PUBLIC m)
endif()

enable_testing()
file(GLOB TEST_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/tests/*.c")
foreach(TEST_SRC ${TEST_SOURCES})
    get_filename_component(TEST_NAME ${TEST_SRC} NAME_WE)
    add_executable(${TEST_NAME} ${TEST_SRC})
    target_link_libraries(${TEST_NAME} PRIVATE lyra_core ${CHECK_LIBRARY})
    add_test(NAME ${TEST_NAME} COMMAND ${TEST_NAME})
endforeach()
