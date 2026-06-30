cmake_minimum_required(VERSION 3.18)
project(optionx_submodule_consumer LANGUAGES CXX)

set(OPTIONX_BUILD_DEPS ON CACHE BOOL "" FORCE)
set(OPTIONX_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(OPTIONX_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)

add_subdirectory("${CMAKE_CURRENT_LIST_DIR}/../../.." optionx_build)

add_executable(optionx_submodule_consumer main.cc)
target_link_libraries(
    optionx_submodule_consumer
    PRIVATE
    optionx_cpp::optionx_cpp
)

if(WIN32)
    add_custom_command(TARGET optionx_submodule_consumer POST_BUILD
        COMMAND ${CMAKE_COMMAND}
            -DOPTIONX_RUNTIME_DLL_DIR="${CMAKE_CURRENT_BINARY_DIR}/optionx_build/bin"
            -DOPTIONX_RUNTIME_TARGET_DIR="$<TARGET_FILE_DIR:optionx_submodule_consumer>"
            -P "${CMAKE_CURRENT_LIST_DIR}/../../../cmake/copy_runtime_dlls.cmake"
    )
endif()
