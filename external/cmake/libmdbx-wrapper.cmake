# libmdbx-wrapper.cmake

set(OPTIONX_EXTERNAL_DIR "${CMAKE_CURRENT_LIST_DIR}/..")

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
add_subdirectory(${OPTIONX_EXTERNAL_DIR}/libmdbx EXCLUDE_FROM_ALL)
# add_subdirectory(${OPTIONX_EXTERNAL_DIR}/libmdbx)

# Конфигурация флагов под компилятор
foreach(target mdbx-static)
    if (TARGET ${target})
        message(STATUS "libmdbx-wrapper: using static MDBX from submodule.")
        set_target_properties(${target} PROPERTIES
            RUNTIME_OUTPUT_DIRECTORY ${OPTIONX_DEPS_OUTPUT_DIR}/bin
            LIBRARY_OUTPUT_DIRECTORY ${OPTIONX_DEPS_OUTPUT_DIR}/bin
            ARCHIVE_OUTPUT_DIRECTORY ${OPTIONX_DEPS_OUTPUT_DIR}/lib
        )
        if(MINGW)
            target_compile_definitions(${target} PRIVATE ERROR_UNHANDLED_EXCEPTION=574)
        endif()
    endif()
endforeach()

if(TARGET mdbx-static)
    add_custom_target(build_mdbx ALL
        DEPENDS mdbx-static
        COMMENT "Forcing build of libmdbx"
    )
endif()

# Копирование заголовка mdbx.h в include/
set(MDBX_HEADER_SRC "${OPTIONX_EXTERNAL_DIR}/libmdbx/mdbx.h")
set(MDBX_HEADER_DST_DIR "${OPTIONX_DEPS_OUTPUT_DIR}/include")

file(MAKE_DIRECTORY "${MDBX_HEADER_DST_DIR}")
file(COPY "${MDBX_HEADER_SRC}" DESTINATION "${MDBX_HEADER_DST_DIR}")
message(STATUS "Copied mdbx.h to ${MDBX_HEADER_DST_DIR}")
