# При генерации Code::Blocks проекта с MinGW можно использовать так:
# cmake -G "CodeBlocks - MinGW Makefiles" -S . -B build-cb
cmake_minimum_required(VERSION 3.18)
project(optionx_cpp LANGUAGES CXX)

set(OPTIONX_TOP_LEVEL OFF)
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    set(OPTIONX_TOP_LEVEL ON)
endif()

# Папка для статических библиотек (.a, .lib)
if(OPTIONX_TOP_LEVEL)
    set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib CACHE PATH "")

# Папка для динамических библиотек (.so, .dll)
    set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib CACHE PATH "")
endif()

if(OPTIONX_TOP_LEVEL)
    get_property(OPTIONX_HAS_BUILD_DEPS CACHE OPTIONX_BUILD_DEPS PROPERTY TYPE SET)
    get_property(OPTIONX_HAS_LEGACY_BUILD_DEPS CACHE BUILD_DEPS PROPERTY TYPE SET)
    if(OPTIONX_HAS_LEGACY_BUILD_DEPS AND NOT OPTIONX_HAS_BUILD_DEPS)
        set(OPTIONX_BUILD_DEPS "${BUILD_DEPS}" CACHE BOOL "Build bundled optionx dependencies" FORCE)
    endif()

    get_property(OPTIONX_HAS_BUILD_EXAMPLES CACHE OPTIONX_BUILD_EXAMPLES PROPERTY TYPE SET)
    get_property(OPTIONX_HAS_LEGACY_BUILD_EXAMPLES CACHE BUILD_EXAMPLES PROPERTY TYPE SET)
    if(OPTIONX_HAS_LEGACY_BUILD_EXAMPLES AND NOT OPTIONX_HAS_BUILD_EXAMPLES)
        set(OPTIONX_BUILD_EXAMPLES "${BUILD_EXAMPLES}" CACHE BOOL "Build optionx examples" FORCE)
    endif()

    get_property(OPTIONX_HAS_BUILD_TESTS CACHE OPTIONX_BUILD_TESTS PROPERTY TYPE SET)
    get_property(OPTIONX_HAS_LEGACY_BUILD_TESTS CACHE BUILD_TESTS PROPERTY TYPE SET)
    if(OPTIONX_HAS_LEGACY_BUILD_TESTS AND NOT OPTIONX_HAS_BUILD_TESTS)
        set(OPTIONX_BUILD_TESTS "${BUILD_TESTS}" CACHE BOOL "Build optionx tests" FORCE)
    endif()

    get_property(OPTIONX_HAS_DEPS_BUILD_DIR CACHE OPTIONX_DEPS_BUILD_DIR PROPERTY TYPE SET)
    get_property(OPTIONX_HAS_LEGACY_DEPS_BUILD_DIR CACHE DEPS_BUILD_DIR PROPERTY TYPE SET)
    if(OPTIONX_HAS_LEGACY_DEPS_BUILD_DIR AND NOT OPTIONX_HAS_DEPS_BUILD_DIR)
        set(OPTIONX_DEPS_BUILD_DIR "${DEPS_BUILD_DIR}" CACHE PATH "Path to prebuilt optionx dependencies" FORCE)
    endif()
endif()

option(OPTIONX_BUILD_DEPS "Build bundled optionx dependencies" OFF)
option(OPTIONX_BUILD_EXAMPLES "Build optionx examples" OFF)
option(OPTIONX_BUILD_TESTS "Build optionx tests" OFF)
set(OPTIONX_DEPS_BUILD_DIR "" CACHE PATH "Path to prebuilt optionx dependencies")

if(OPTIONX_TOP_LEVEL)
    set(BUILD_DEPS "${OPTIONX_BUILD_DEPS}" CACHE BOOL "Legacy alias for OPTIONX_BUILD_DEPS" FORCE)
    set(BUILD_EXAMPLES "${OPTIONX_BUILD_EXAMPLES}" CACHE BOOL "Legacy alias for OPTIONX_BUILD_EXAMPLES" FORCE)
    set(BUILD_TESTS "${OPTIONX_BUILD_TESTS}" CACHE BOOL "Legacy alias for OPTIONX_BUILD_TESTS" FORCE)
    set(DEPS_BUILD_DIR "${OPTIONX_DEPS_BUILD_DIR}" CACHE PATH "Legacy alias for OPTIONX_DEPS_BUILD_DIR" FORCE)
endif()

set(LOGIT_BASE_PATH "${CMAKE_CURRENT_SOURCE_DIR}" CACHE PATH "Base path for LOGIT logs")
file(TO_CMAKE_PATH "${LOGIT_BASE_PATH}" LOGIT_BASE_PATH_FWD)

function(optionx_get_network_link_libraries out_var)
    if(TARGET OpenSSL::SSL AND TARGET OpenSSL::Crypto AND TARGET CURL::libcurl)
        set(network_libs
            OpenSSL::SSL
            OpenSSL::Crypto
            CURL::libcurl
        )
    elseif(WIN32)
        set(network_libs
            ssl
            crypto
            curl
        )
    else()
        find_package(OpenSSL REQUIRED)
        find_package(CURL REQUIRED)

        set(network_libs
            OpenSSL::SSL
            OpenSSL::Crypto
            CURL::libcurl
        )
    endif()

    set(${out_var} ${network_libs} PARENT_SCOPE)
endfunction()

function(optionx_link_preferred_dependency target raw_name)
    foreach(candidate IN LISTS ARGN)
        if(TARGET ${candidate})
            target_link_libraries(${target} INTERFACE ${candidate})
            return()
        endif()
    endforeach()

    target_link_libraries(${target} INTERFACE ${raw_name})
endfunction()

function(optionx_link_existing_dependencies target)
    foreach(candidate IN LISTS ARGN)
        if(TARGET ${candidate})
            target_link_libraries(${target} INTERFACE ${candidate})
        endif()
    endforeach()
endfunction()

set(OPTIONX_INCLUDE_DIRS
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/include/optionx_cpp
)

set(OPTIONX_COMPILE_DEFINITIONS
    ASIO_STANDALONE
    KURLYK_OAUTH_SUPPORT=0
)

set(OPTIONX_WINDOWS_SYSTEM_LIBS
    ws2_32
    wsock32
    crypt32
    ntdll
    bcrypt
)

add_library(optionx_cpp INTERFACE)
add_library(optionx_cpp::optionx_cpp ALIAS optionx_cpp)

target_include_directories(optionx_cpp INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include/optionx_cpp>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/external/SimpleNamedPipe/include>
    $<INSTALL_INTERFACE:include>
    $<INSTALL_INTERFACE:include/optionx_cpp>
)

target_compile_features(optionx_cpp INTERFACE cxx_std_17)
target_compile_definitions(optionx_cpp INTERFACE ${OPTIONX_COMPILE_DEFINITIONS})

if(WIN32)
    target_link_libraries(optionx_cpp INTERFACE ${OPTIONX_WINDOWS_SYSTEM_LIBS})
endif()

if(OPTIONX_BUILD_DEPS)
    set(OPTIONX_DEPS_OUTPUT_DIR "${CMAKE_CURRENT_BINARY_DIR}")
    add_subdirectory(external)
elseif(OPTIONX_DEPS_BUILD_DIR)
    set(OPTIONX_DEPS_OUTPUT_DIR "${OPTIONX_DEPS_BUILD_DIR}")
endif()

if(OPTIONX_DEPS_OUTPUT_DIR)
    target_include_directories(optionx_cpp INTERFACE
        $<BUILD_INTERFACE:${OPTIONX_DEPS_OUTPUT_DIR}/include>
    )
    target_link_directories(optionx_cpp INTERFACE
        $<BUILD_INTERFACE:${OPTIONX_DEPS_OUTPUT_DIR}/lib>
        $<BUILD_INTERFACE:${OPTIONX_DEPS_OUTPUT_DIR}/bin>
    )
endif()

optionx_get_network_link_libraries(OPTIONX_NETWORK_LIBS)
target_link_libraries(optionx_cpp INTERFACE ${OPTIONX_NETWORK_LIBS})

optionx_link_preferred_dependency(optionx_cpp mdbx
    mdbx::mdbx-static
    mdbx-static
    mdbx::mdbx
    mdbx
)
optionx_link_preferred_dependency(optionx_cpp AES AES)
optionx_link_existing_dependencies(optionx_cpp
    time_shield::time_shield
    log-it-cpp::log-it-cpp
    mdbx_containers::mdbx_containers
    nlohmann_json::nlohmann_json
    kurlyk
    simple-websocket-server
)

if(OPTIONX_BUILD_EXAMPLES)
    if(OPTIONX_BUILD_DEPS)
        set(EXAMPLE_BUILD_LIBS_DIR ${OPTIONX_DEPS_OUTPUT_DIR})
    else()
        if(NOT OPTIONX_DEPS_BUILD_DIR)
            message(FATAL_ERROR "OPTIONX_BUILD_DEPS is OFF, but OPTIONX_DEPS_BUILD_DIR is not set")
        endif()
        set(EXAMPLE_BUILD_LIBS_DIR ${OPTIONX_DEPS_BUILD_DIR})
    endif()

    set(EXAMPLE_INCLUDE_DIRS
        ${OPTIONX_INCLUDE_DIRS}
    )

    set(EXAMPLE_DEPS_INCLUDE_DIRS
        ${EXAMPLE_BUILD_LIBS_DIR}/include
    )

    set(EXAMPLE_LIBRARY_DIRS
        ${EXAMPLE_BUILD_LIBS_DIR}/lib
        ${EXAMPLE_BUILD_LIBS_DIR}/bin
    )

    file(GLOB EXAMPLE_DLL_FILES "${EXAMPLE_LIBRARY_DIRS}/*.dll")

    set(EXAMPLE_DEFINES
        ${OPTIONX_COMPILE_DEFINITIONS}
    )

    set(EXAMPLE_LIBS)

    add_executable(trade_record_db_example examples/trade_record_db_example.cpp)

    target_include_directories(trade_record_db_example PRIVATE
        ${EXAMPLE_INCLUDE_DIRS}
        ${EXAMPLE_DEPS_INCLUDE_DIRS}
    )

    target_link_directories(trade_record_db_example PRIVATE ${EXAMPLE_LIBRARY_DIRS})
    target_compile_definitions(
        trade_record_db_example PRIVATE
        ${EXAMPLE_DEFINES}
        LOGIT_BASE_PATH="${LOGIT_BASE_PATH_FWD}"
    )
    target_link_libraries(trade_record_db_example PRIVATE ${EXAMPLE_LIBS} optionx_cpp)

    if(OPTIONX_BUILD_DEPS)
        add_dependencies(trade_record_db_example mdbx-static AES)
    endif()

    foreach(dll ${EXAMPLE_DLL_FILES})
        add_custom_command(TARGET trade_record_db_example POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${dll}" "$<TARGET_FILE_DIR:trade_record_db_example>"
        )
    endforeach()

    if(WIN32)
        add_custom_command(TARGET trade_record_db_example POST_BUILD
            COMMAND ${CMAKE_COMMAND}
                -DOPTIONX_RUNTIME_DLL_DIR="${EXAMPLE_BUILD_LIBS_DIR}/bin"
                -DOPTIONX_RUNTIME_TARGET_DIR="$<TARGET_FILE_DIR:trade_record_db_example>"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/copy_runtime_dlls.cmake"
        )
    endif()
endif()

if(OPTIONX_BUILD_TESTS)
    if(OPTIONX_BUILD_DEPS)
        set(BUILD_LIBS_DIR ${OPTIONX_DEPS_OUTPUT_DIR})
    else()
        if(NOT OPTIONX_DEPS_BUILD_DIR)
            message(FATAL_ERROR "OPTIONX_BUILD_DEPS is OFF, but OPTIONX_DEPS_BUILD_DIR is not set")
        endif()
        set(BUILD_LIBS_DIR ${OPTIONX_DEPS_BUILD_DIR})
    endif()

    set(PROJECT_INCLUDE_DIRS
        ${OPTIONX_INCLUDE_DIRS}
        ${CMAKE_CURRENT_SOURCE_DIR}/tests
    )

    set(DEPS_INCLUDE_DIRS
        ${BUILD_LIBS_DIR}/include
    )

    set(PROJECT_LIBRARY_DIRS
        ${BUILD_LIBS_DIR}/lib
        ${BUILD_LIBS_DIR}/bin
    )

    file(GLOB DLL_FILES "${PROJECT_LIBRARY_DIRS}/*.dll")

    set(PROJECT_DEFINES
        ${OPTIONX_COMPILE_DEFINITIONS}
    )

    set(PROJECT_LIBS
        gtest
    )

    file(GLOB_RECURSE PROJECT_HEADERS
        RELATIVE ${CMAKE_CURRENT_SOURCE_DIR}
        include/*.hpp
    )
    add_custom_target(project_headers SOURCES ${PROJECT_HEADERS})

    file(GLOB_RECURSE TEST_SOURCES
        CONFIGURE_DEPENDS
        RELATIVE ${CMAKE_CURRENT_SOURCE_DIR}
        tests/*.cpp
    )
    list(SORT TEST_SOURCES)

    set(TEST_SOURCE_STEMS)
    foreach(test_src ${TEST_SOURCES})
        get_filename_component(test_stem "${test_src}" NAME_WE)
        list(APPEND TEST_SOURCE_STEMS "${test_stem}")
    endforeach()

    set(TEST_TARGET_NAMES)
    set(TEST_TARGET_SOURCES)

    # Keep existing basename targets while they are unique. If recursive
    # discovery finds duplicate stems, use path-derived names for that stem.
    foreach(test_src ${TEST_SOURCES})
        get_filename_component(test_name "${test_src}" NAME_WE)
        set(test_name_count 0)
        foreach(test_stem ${TEST_SOURCE_STEMS})
            if(test_stem STREQUAL test_name)
                math(EXPR test_name_count "${test_name_count} + 1")
            endif()
        endforeach()

        if(test_name_count GREATER 1)
            set(test_name "${test_src}")
            string(REGEX REPLACE "\\.[^.]*$" "" test_name "${test_name}")
            string(MAKE_C_IDENTIFIER "${test_name}" test_name)
        endif()

        list(FIND TEST_TARGET_NAMES "${test_name}" existing_test_index)
        if(NOT existing_test_index EQUAL -1)
            list(GET TEST_TARGET_SOURCES ${existing_test_index} existing_test_src)
            message(FATAL_ERROR "Duplicate test target name '${test_name}' from '${test_src}' conflicts with '${existing_test_src}'")
        endif()

        if(TARGET ${test_name})
            message(FATAL_ERROR "Test target name '${test_name}' from '${test_src}' conflicts with an existing CMake target")
        endif()

        list(APPEND TEST_TARGET_NAMES "${test_name}")
        list(APPEND TEST_TARGET_SOURCES "${test_src}")

        add_executable(${test_name} ${test_src})

        target_include_directories(${test_name} PRIVATE
            ${PROJECT_INCLUDE_DIRS}
            ${DEPS_INCLUDE_DIRS}
        )

        target_link_directories(${test_name} PRIVATE ${PROJECT_LIBRARY_DIRS})
        target_compile_definitions(
            ${test_name} PRIVATE 
            ${PROJECT_DEFINES}
            LOGIT_BASE_PATH="${LOGIT_BASE_PATH_FWD}"
        )
        target_link_libraries(${test_name} PRIVATE ${PROJECT_LIBS} optionx_cpp)

        if(OPTIONX_BUILD_DEPS)
            foreach(test_dep mdbx-static AES gtest)
                if(TARGET ${test_dep})
                    add_dependencies(${test_name} ${test_dep})
                endif()
            endforeach()
        endif()

        set_source_files_properties(${PROJECT_HEADERS} PROPERTIES HEADER_FILE_ONLY ON)
        target_sources(${test_name} PRIVATE ${PROJECT_HEADERS})

        foreach(dll ${DLL_FILES})
            add_custom_command(TARGET ${test_name} POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${dll}" "$<TARGET_FILE_DIR:${test_name}>"
            )
        endforeach()

        if(WIN32)
            add_custom_command(TARGET ${test_name} POST_BUILD
                COMMAND ${CMAKE_COMMAND}
                    -DOPTIONX_RUNTIME_DLL_DIR="${BUILD_LIBS_DIR}/bin"
                    -DOPTIONX_RUNTIME_TARGET_DIR="$<TARGET_FILE_DIR:${test_name}>"
                    -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/copy_runtime_dlls.cmake"
            )
        endif()
    endforeach()

    if(TARGET header_only_odr_test)
        target_sources(header_only_odr_test PRIVATE tests/header_only_odr_companion.cc)
    endif()

    if(TARGET intrade_bar_api_response_test)
        target_include_directories(intrade_bar_api_response_test PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/external/kurlyk/external/Simple-Web-Server
        )
    endif()
endif()
