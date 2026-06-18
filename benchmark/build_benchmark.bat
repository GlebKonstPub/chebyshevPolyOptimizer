@echo off
mkdir	%~dp0build >NUL 2>&1
mkdir	%~dp0build\obj >NUL 2>&1
cl		%~dp0BH4_benchmark.c	/O2 /arch:AVX2 /fp:fast /nologo /Fo: %~dp0build\obj\ /Fe: %~dp0build\BH4_bench_MSVC.exe
cl		%~dp0AVX2_benchmark.c	/O2 /arch:AVX2 /fp:fast /nologo /Fo: %~dp0build\obj\ /Fe: %~dp0build\AVX2_bench_MSVC.exe
cl		%~dp0graph_bench.c		/O2 /arch:AVX2 /fp:fast /nologo /Fo: %~dp0build\obj\ /Fe: %~dp0build\graph_bench_MSVC.exe
clang	%~dp0BH4_benchmark.c	-O3 -mavx2 -mfma -ffast-math -o %~dp0build\BH4_bench_clang.exe
clang	%~dp0AVX2_benchmark.c	-O3 -mavx2 -mfma -ffast-math -o %~dp0build\AVX2_bench_clang.exe
clang	%~dp0graph_bench.c		-O3 -mavx2 -mfma -ffast-math -o %~dp0build\graph_bench_clang.exe