@echo off
setlocal

mkdir build-tests 2>nul
cd build-tests

:: Генерация проекта с указанием исходников и языка
cmake -G "MinGW Makefiles" -S .. ^
  -DCMAKE_CXX_STANDARD=17 ^
  -DCMAKE_BUILD_TYPE=Release ^
  -DBUILD_TESTS=ON ^
  -DBUILD_DEPS=OFF ^
  -DDEPS_BUILD_DIR=../build-libs ^
  -DBUILD_EXAMPLES=OFF

:: Сборка в Release-конфигурации
cmake --build . --config Release 2> build-errors.txt

pause
