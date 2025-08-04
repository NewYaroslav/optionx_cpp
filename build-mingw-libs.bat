@echo off
setlocal

mkdir build-libs 2>nul
cd build-libs

:: Генерация проекта с указанием исходников и языка
cmake -G "MinGW Makefiles" -S .. ^
  -DCMAKE_CXX_STANDARD=17 ^
  -DCMAKE_BUILD_TYPE=Release ^
  -DBUILD_DEPS=ON ^
  -DBUILD_TESTS=OFF ^
  -DBUILD_EXAMPLES=OFF

:: Сборка в Release-конфигурации
cmake --build . --config Release 

pause
