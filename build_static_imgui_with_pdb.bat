@REM Compile ImGui into a static library
@setlocal
@echo off
@set IMGUI_DIR=%~dp0libs\imgui-1.91.6
@set OUT_DIR=%~dp0libs\imgui-static-lib
@set INCLUDES=	/I %IMGUI_DIR% ^
				/I %IMGUI_DIR%\backends ^
				/I %~dp0libs\glfw-3.4.bin.WIN64\include

mkdir %OUT_DIR% >NUL 2>&1
mkdir %OUT_DIR%\obj >NUL 2>&1

echo [ImGui] Compiling sources...
cl /nologo /O2 /arch:AVX2 /Zi /MD /utf-8 /c %INCLUDES% ^
				%IMGUI_DIR%\imgui*.cpp ^
				%IMGUI_DIR%\backends\imgui_impl_glfw.cpp ^
				%IMGUI_DIR%\backends\imgui_impl_opengl3.cpp ^
				/Fo%OUT_DIR%\obj\ ^
				/Fd%OUT_DIR%\imgui_static.pdb

echo [ImGui] Linking static lib...
lib /nologo /OUT:%OUT_DIR%\imgui.lib %OUT_DIR%\obj\*.obj

echo [ImGui] Done: %OUT_DIR%\imgui.lib

@endlocal