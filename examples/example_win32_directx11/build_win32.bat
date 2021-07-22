@REM Build for Visual Studio compiler. Run your copy of vcvars32.bat or vcvarsall.bat to setup command-line compiler.
@set OUT_DIR=Debug
@set OUT_EXE=example_win32_directx11
@REM TEX_INSPECT_CHANGE...
@set IMGUI_DIR=..\..\..\imgui
@set TEX_INSPECT_DIR=..\..
@set INCLUDES=/I%IMGUI_DIR% /I%IMGUI_DIR%\backends /I%TEX_INSPECT_DIR% /I%TEX_INSPECT_DIR%/backends /I%TEX_INSPECT_DIR%/examples/loaders /I "%WindowsSdkDir%Include\um" /I "%WindowsSdkDir%Include\shared" /I "%DXSDK_DIR%Include"
@set SOURCES=main.cpp %IMGUI_DIR%\backends\imgui_impl_dx11.cpp %IMGUI_DIR%\backends\imgui_impl_win32.cpp %IMGUI_DIR%\imgui*.cpp %TEX_INSPECT_DIR%/imgui_tex_*.cpp %TEX_INSPECT_DIR%/backends/tex_inspect_directx11.cpp
@REM ...TEX_INSPECT_CHANGE
@set LIBS=/LIBPATH:"%DXSDK_DIR%/Lib/x86" d3d11.lib d3dcompiler.lib
mkdir %OUT_DIR%
cl /nologo /Zi /MD %INCLUDES% /D UNICODE /D _UNICODE %SOURCES% /Fe%OUT_DIR%/%OUT_EXE%.exe /Fo%OUT_DIR%/ /link %LIBS%

