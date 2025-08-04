# libmdbx-wrapper.cmake

# Конфигурация сборки libmdbx
set(MDBX_ENABLE_INSTALL OFF CACHE BOOL "" FORCE)
set(MDBX_BUILD_SHARED OFF CACHE BOOL "" FORCE)
set(MDBX_BUILD_STATIC ON CACHE BOOL "" FORCE)
set(MDBX_BUILD_TOOLS OFF CACHE BOOL "" FORCE)
set(MDBX_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(MDBX_USE_VALGRIND OFF CACHE BOOL "" FORCE)

# Только для MSVC: отключить использование <crtdbg.h>
if (MSVC)
    set(MDBX_WITHOUT_MSVC_CRTDBG ON CACHE BOOL "" FORCE)
endif()

# Подключение исходников MDBX
add_subdirectory(${CMAKE_SOURCE_DIR}/libs/libmdbx EXCLUDE_FROM_ALL)
# add_subdirectory(${CMAKE_SOURCE_DIR}/libs/libmdbx)

# Конфигурация флагов под компилятор
foreach(target mdbx-static)
    if (TARGET ${target})
        message(STATUS "libmdbx-wrapper: using static MDBX from submodule.")
        set_target_properties(${target} PROPERTIES
            RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
            LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
            ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib
        )
    endif()
endforeach()

if(TARGET mdbx-static)
    add_custom_target(build_mdbx ALL
        DEPENDS mdbx-static
        COMMENT "Forcing build of libmdbx"
    )
endif()

# Копирование заголовка mdbx.h в include/
set(MDBX_HEADER_SRC "${CMAKE_SOURCE_DIR}/libs/libmdbx/mdbx.h")
set(MDBX_HEADER_DST_DIR "${CMAKE_BINARY_DIR}/include")

file(MAKE_DIRECTORY "${MDBX_HEADER_DST_DIR}")
file(COPY "${MDBX_HEADER_SRC}" DESTINATION "${MDBX_HEADER_DST_DIR}")
message(STATUS "Copied mdbx.h to ${MDBX_HEADER_DST_DIR}")


