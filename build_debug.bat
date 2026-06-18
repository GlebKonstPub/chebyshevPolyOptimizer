@REM Build for Visual Studio compiler. Run your copy of vcvars64.bat or vcvarsall.bat to setup command-line compiler.
@setlocal
@echo off
@set OUT_DIR=%~dp0Debug
@set OUT_EXE=chebPolyOptimizer
@set SOURCES=	%~dp0main.cpp
@set INCLUDES=	/I %~dp0libs\imgui-1.91.6 ^
				/I %~dp0libs\imgui-1.91.6\backends ^
				/I %~dp0libs\glfw-3.4.bin.WIN64\include
@set LIBS=		/LIBPATH:%~dp0libs\glfw-3.4.bin.WIN64\lib-vc2022 glfw3.lib ^
				/LIBPATH:%~dp0libs\imgui-static-lib imgui.lib ^
				opengl32.lib gdi32.lib shell32.lib

@REM Checking for pre-build ImGUI static lib with .pdb
@set IMGUI_LIB=%~dp0libs\imgui-static-lib\imgui.lib
@set IMGUI_PDB=%~dp0libs\imgui-static-lib\imgui_static.pdb
if not exist "%IMGUI_LIB%" (
	echo [build_debug] %IMGUI_LIB% not found, building imgui...
	call %~dp0build_static_imgui_with_pdb.bat
)
if not exist "%IMGUI_PDB%" (
	echo [build_debug] %IMGUI_PDB% not found, rebuilding imgui...
	call %~dp0build_static_imgui_with_pdb.bat
)

mkdir %OUT_DIR% >NUL 2>&1
mkdir %OUT_DIR%\tmp >NUL 2>&1
cl /std:c++20 /nologo /Od /arch:AVX2 /Zi /MD /utf-8 %INCLUDES% %SOURCES% ^
				/Fe%OUT_DIR%\%OUT_EXE%.exe ^
				/Fd%OUT_DIR%\tmp\ ^
				/Fo%OUT_DIR%\ ^
				/link %LIBS% /SUBSYSTEM:WINDOWS /INCREMENTAL:NO /PDB:%OUT_DIR%\%OUT_EXE%.pdb
rmdir /s /q %OUT_DIR%\tmp >NUL 2>&1
@endlocal