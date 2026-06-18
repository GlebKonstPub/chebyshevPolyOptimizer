@REM Build for Visual Studio compiler. Run your copy of vcvars64.bat or vcvarsall.bat to setup command-line compiler.
@setlocal
@echo off

@set OUT_DIR=%~dp0Release
@set OUT_EXE=chebPolyOptimizer
@set INCLUDES=	/I %~dp0libs\imgui-1.91.6 ^
				/I %~dp0libs\imgui-1.91.6\backends ^
				/I %~dp0libs\glfw-3.4.bin.WIN64\include
@set SOURCES=	%~dp0main.cpp ^
				%~dp0libs\imgui-1.91.6\backends\imgui_impl_glfw.cpp ^
				%~dp0libs\imgui-1.91.6\backends\imgui_impl_opengl3.cpp ^
				%~dp0libs\imgui-1.91.6\imgui*.cpp
@set LIBS=		/LIBPATH:%~dp0libs\glfw-3.4.bin.WIN64\lib-vc2022 glfw3.lib opengl32.lib gdi32.lib shell32.lib

mkdir %OUT_DIR% >NUL 2>&1
mkdir %OUT_DIR%\obj >NUL 2>&1
cl /std:c++20 /nologo /O2 /arch:AVX2 /MD /utf-8 %INCLUDES% %SOURCES% /Fe%OUT_DIR%/%OUT_EXE%.exe /Fo%OUT_DIR%/obj/ /link %LIBS% /SUBSYSTEM:WINDOWS

@endlocal