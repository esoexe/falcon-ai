@echo off
REM ============================================================================
REM Falcon AI 2.0 — Native Windows x64 C++20 Engine Compiler (MSYS2 / MinGW64)
REM Zero external DLLs: uses Winsock2, WinHTTP TLS, GDI+, and static zlib + OpenMP
REM ============================================================================
if not exist bin mkdir bin
g++ -std=c++20 -O3 cpp_engine/src/pack_web.cpp -o bin/pack_web.exe -lz
bin\pack_web.exe frontend/web_bundle cpp_engine/include/embedded_web.hpp
g++ -std=c++20 -O3 -fopenmp -static -Icpp_engine/include cpp_engine/src/main.cpp -lz -lws2_32 -lwinhttp -lgdiplus -lshlwapi -lole32 -o bin/falcon_server.exe
strip -s bin/falcon_server.exe
echo [OK] Compiled standalone static bin\falcon_server.exe!
