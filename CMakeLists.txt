# При генерации Code::Blocks проекта с MinGW можно использовать так:
# cmake -G "CodeBlocks - MinGW Makefiles" -S . -B build-cb
cmake_minimum_required(VERSION 3.18)
project(optionx_cpp LANGUAGES CXX)

# Папка для статических библиотек (.a, .lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib CACHE PATH "")

# Папка для динамических библиотек (.so, .dll)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib CACHE PATH "")

option(BUILD_DEPS "Build internal dependencies" OFF)
option(BUILD_EXAMPLES "Build examples" OFF)
option(BUILD_TESTS "Build tests" OFF)
set(DEPS_BUILD_DIR "" CACHE PATH "Path to directory where dependencies were built")

set(LOGIT_BASE_PATH "${CMAKE_SOURCE_DIR}" CACHE PATH "Base path for LOGIT logs")
file(TO_CMAKE_PATH "${LOGIT_BASE_PATH}" LOGIT_BASE_PATH_FWD)

function(optionx_get_network_link_libraries out_var)
    if(WIN32)
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

if(BUILD_DEPS)
    add_subdirectory(external)
endif()

if(BUILD_EXAMPLES)
    if(BUILD_DEPS)
        set(EXAMPLE_BUILD_LIBS_DIR ${CMAKE_BINARY_DIR})
    else()
        if(NOT DEPS_BUILD_DIR)
            message(FATAL_ERROR "BUILD_DEPS is OFF, but DEPS_BUILD_DIR is not set")
        endif()
        set(EXAMPLE_BUILD_LIBS_DIR ${DEPS_BUILD_DIR})
    endif()

    set(EXAMPLE_INCLUDE_DIRS
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}/include/optionx_cpp
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
        ASIO_STANDALONE
        KURLYK_OAUTH_SUPPORT=0
    )

    optionx_get_network_link_libraries(EXAMPLE_NETWORK_LIBS)

    set(EXAMPLE_LIBS
        ${EXAMPLE_NETWORK_LIBS}
        mdbx
        AES
    )

    if(WIN32)
        list(APPEND EXAMPLE_LIBS
            ws2_32
            wsock32
            crypt32
            ntdll
            bcrypt
        )
    endif()

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
    target_link_libraries(trade_record_db_example PRIVATE ${EXAMPLE_LIBS})

    if(BUILD_DEPS)
        add_dependencies(trade_record_db_example mdbx-static AES)
    endif()

    foreach(dll ${EXAMPLE_DLL_FILES})
        add_custom_command(TARGET trade_record_db_example POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${dll}" "$<TARGET_FILE_DIR:trade_record_db_example>"
        )
    endforeach()
endif()

if(BUILD_TESTS)
    if(BUILD_DEPS)
        set(BUILD_LIBS_DIR ${CMAKE_BINARY_DIR})
    else()
        if(NOT DEPS_BUILD_DIR)
            message(FATAL_ERROR "BUILD_DEPS is OFF, but DEPS_BUILD_DIR is not set")
        endif()
        set(BUILD_LIBS_DIR ${DEPS_BUILD_DIR})
    endif()

    set(PROJECT_INCLUDE_DIRS
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}/include/optionx_cpp
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
        ASIO_STANDALONE
        KURLYK_OAUTH_SUPPORT=0
    )

    optionx_get_network_link_libraries(PROJECT_NETWORK_LIBS)

    set(PROJECT_LIBS
        ${PROJECT_NETWORK_LIBS}
        mdbx
        AES
        gtest
    )

    if(WIN32)
        list(APPEND PROJECT_LIBS
            ws2_32
            wsock32
            crypt32
            ntdll
            bcrypt
        )
    endif()

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
        target_link_libraries(${test_name} PRIVATE ${PROJECT_LIBS})

        if(BUILD_DEPS)
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
