@echo off
REM ============================================================================
REM Falcon AI 2.0 — Native Windows C++20 Engine Compiler (MSYS2 / MinGW64 g++)
REM Compiles cpp_engine\src\main.cpp -> bin\falcon_server.exe (Zero Python!)
REM ============================================================================
if not exist bin mkdir bin
g++ -std=c++20 -O3 -fopenmp -pthread -Icpp_engine/include cpp_engine/src/pack_web.cpp -o bin/pack_web.exe -lz
bin\pack_web.exe frontend/web_bundle cpp_engine/include/embedded_web.hpp
g++ -std=c++20 -O3 -fopenmp -pthread -Icpp_engine/include cpp_engine/src/main.cpp -o bin/falcon_server.exe -lcurl -lpng -ljpeg -lz -lws2_32
echo [OK] Compiled bin\falcon_server.exe! Launch Falcon-AI-2.0.0-x64.exe to use full OpenMP C++20 speed.
