# libs/CMakeLists.txt
cmake_minimum_required(VERSION 3.18)
project(LibsProject LANGUAGES C CXX)

option(BUILD_SHARED_LIBS "Build shared libraries by default" OFF)

if (CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(
        -Wall -Wextra -Wpedantic
    )
    add_link_options(
        -static-libgcc
        -static-libstdc++
        -static-libwinpthread
    )

elseif (MSVC)
    add_compile_options(
        /W4
        /wd4201  # anonymous union/struct
        /wd4324  # padding warning
    )
    # Статическая линковка — глобально:
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>" CACHE STRING "" FORCE)
endif()

set(LIBS_CMAKE_DIR ${CMAKE_CURRENT_LIST_DIR}/cmake)
include(${LIBS_CMAKE_DIR}/install-headers.cmake)
include(${LIBS_CMAKE_DIR}/install-static-libs.cmake)
include(${LIBS_CMAKE_DIR}/install-runtime-dlls.cmake)

# mdbx
include(${LIBS_CMAKE_DIR}/libmdbx-wrapper.cmake)

# openssl
install_headers_to_include(openssl ${CMAKE_CURRENT_LIST_DIR}/openssl-win64-v3.4.0/include/openssl)
install_static_libs_to_lib(${CMAKE_CURRENT_LIST_DIR}/openssl-win64-v3.4.0/lib/VC/x64/MT)
install_runtime_dlls_to_bin(${CMAKE_CURRENT_LIST_DIR}/openssl-win64-v3.4.0/bin)

# curl
install_headers_to_include(curl ${CMAKE_CURRENT_LIST_DIR}/curl-8.11.0_1-win64-mingw/include/curl)

file(GLOB CURL_STATIC_LIBS "${CMAKE_CURRENT_LIST_DIR}/curl-8.11.0_1-win64-mingw/lib/*.a")
# Удаляем libssl.a и libcrypto.a
list(FILTER CURL_STATIC_LIBS EXCLUDE REGEX ".*libssl.a$")
list(FILTER CURL_STATIC_LIBS EXCLUDE REGEX ".*libcrypto.a$")
install_lib_files_to_lib(${CURL_STATIC_LIBS})
# install_static_libs_to_lib(${CMAKE_CURRENT_LIST_DIR}/curl-8.11.0_1-win64-mingw/lib)
install_runtime_dlls_to_bin(${CMAKE_CURRENT_LIST_DIR}/curl-8.11.0_1-win64-mingw/bin)

# AES
add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/AES)
install_headers_to_include("." ${CMAKE_CURRENT_LIST_DIR}/AES/src)

# googletest
include(${LIBS_CMAKE_DIR}/libgtest-wrapper.cmake)
# install_static_libs_to_lib(${GTEST_DIR}/build)

# header-only
install_headers_to_include("."             ${CMAKE_CURRENT_LIST_DIR}/time-shield-cpp/include/time_shield_cpp)
install_headers_to_include(logit_cpp       ${CMAKE_CURRENT_LIST_DIR}/log-it-cpp/include/logit_cpp)
install_headers_to_include("."             ${CMAKE_CURRENT_LIST_DIR}/asio/asio/include)
install_headers_to_include(nlohmann        ${CMAKE_CURRENT_LIST_DIR}/json/include/nlohmann)
install_headers_to_include("."             ${CMAKE_CURRENT_LIST_DIR}/Simple-WebSocket-Server)
install_headers_to_include("."             ${CMAKE_CURRENT_LIST_DIR}/kurlyk/include)
install_headers_to_include("."             ${CMAKE_CURRENT_LIST_DIR}/mdbx-containers/include)

add_custom_target(all_libs_built ALL
    COMMENT "All libraries and headers have been prepared."
)