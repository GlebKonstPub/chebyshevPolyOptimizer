@REM Build with Clang. Requires clang++ in PATH (e.g. from LLVM installer or Visual Studio).
@setlocal
@echo off

@set OUT_DIR=%~dp0Release
@set OUT_EXE=chebPolyOptimizer_clang
@set INCLUDES=	-I %~dp0libs\imgui-1.91.6 ^
				-I %~dp0libs\imgui-1.91.6\backends ^
				-I %~dp0libs\glfw-3.4.bin.WIN64\include
@set SOURCES=	%~dp0main.cpp ^
				%~dp0libs\imgui-1.91.6\backends\imgui_impl_glfw.cpp ^
				%~dp0libs\imgui-1.91.6\backends\imgui_impl_opengl3.cpp ^
				%~dp0libs\imgui-1.91.6\imgui*.cpp
@set LIBS=		-L %~dp0libs\glfw-3.4.bin.WIN64\lib-vc2022 -lglfw3 -lopengl32 -lgdi32 -lshell32 -luser32

mkdir %OUT_DIR% >NUL 2>&1
clang -std=c++20 -O3 -mavx2 -mfma ^
	-Wall -Wextra -Wpedantic ^
	-Wshadow -Wconversion ^
	-Wnull-dereference -Wdouble-promotion ^
	%INCLUDES% %SOURCES% -o %OUT_DIR%\%OUT_EXE%.exe %LIBS% -Xlinker /SUBSYSTEM:WINDOWS

@endlocal