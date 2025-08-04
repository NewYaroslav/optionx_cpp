# libgtest-wrapper.cmake
# Подключение и установка GoogleTest как библиотеки

set(GTEST_DIR ${CMAKE_SOURCE_DIR}/libs/googletest)

# Опции для сборки
option(INSTALL_GTEST OFF)
option(BUILD_GMOCK OFF)
if (MSVC)
	option(gtest_force_shared_crt OFF)
endif()

# Добавляем googletest
add_subdirectory(${GTEST_DIR})

# Установка заголовков
install_headers_to_include(gtest ${GTEST_DIR}/googletest/include/gtest)

# Создаём псевдоцель для зависимости от сборки
add_custom_target(gtest_built ALL
    COMMENT "GoogleTest has been built and headers installed."
)
