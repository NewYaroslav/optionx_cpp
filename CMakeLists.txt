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

if(BUILD_DEPS)
    add_subdirectory(libs)
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
    )

    set(PROJECT_LIBS
        ws2_32
        wsock32
        crypt32
        ssl
        crypto
        curl
        mdbx
        ntdll
        bcrypt
        AES
		gtest
    )

    file(GLOB_RECURSE PROJECT_HEADERS
        RELATIVE ${CMAKE_CURRENT_SOURCE_DIR}
        include/*.hpp
    )
    add_custom_target(project_headers SOURCES ${PROJECT_HEADERS})

    file(GLOB TEST_SOURCES RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} tests/*.cpp)

    foreach(test_src ${TEST_SOURCES})
        get_filename_component(test_name ${test_src} NAME_WE)

        add_executable(${test_name} ${test_src})

        target_include_directories(${test_name} PRIVATE
            ${PROJECT_INCLUDE_DIRS}
            ${DEPS_INCLUDE_DIRS}
        )

        target_link_directories(${test_name} PRIVATE ${PROJECT_LIBRARY_DIRS})
        target_compile_definitions(${test_name} PRIVATE ${PROJECT_DEFINES})
        target_link_libraries(${test_name} PRIVATE ${PROJECT_LIBS})

        set_source_files_properties(${PROJECT_HEADERS} PROPERTIES HEADER_FILE_ONLY ON)
        target_sources(${test_name} PRIVATE ${PROJECT_HEADERS})

        foreach(dll ${DLL_FILES})
            add_custom_command(TARGET ${test_name} POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${dll}" "$<TARGET_FILE_DIR:${test_name}>"
            )
        endforeach()
    endforeach()
endif()

