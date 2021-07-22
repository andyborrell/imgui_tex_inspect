@REM TODO
@REM Build for Visual Studio compiler. Run your copy of vcvars32.bat or vcvarsall.bat to setup command-line compiler.
@set OUT_DIR=Debug
@set OUT_EXE=example_sdl_opengl3
@REM TEX_INSPECT_CHANGE...
@set IMGUI_DIR=..\..\..\imgui
@set TEX_INSPECT_DIR=..\..
@set INCLUDES=/I%IMGUI_DIR% /I%IMGUI_DIR%\backends /I%SDL2_DIR%\include /I..\libs\gl3w /I%TEX_INSPECT_DIR% /I%TEX_INSPECT_DIR%/backends /I%TEX_INSPECT_DIR%/examples/loaders
@set SOURCES=main.cpp %IMGUI_DIR%\backends\imgui_impl_sdl.cpp %IMGUI_DIR%\backends\imgui_impl_opengl3.cpp %IMGUI_DIR%\imgui*.cpp %IMGUI_DIR%\examples\libs\gl3w\GL\gl3w.c %TEX_INSPECT_DIR%/imgui_tex_inspect.cpp %TEX_INSPECT_DIR%/imgui_tex_inspect_demo.cpp %TEX_INSPECT_DIR%/backends/tex_inspect_opengl.cpp 
@REM ...TEX_INSPECT_CHANGE
@set LIBS=/LIBPATH:%SDL2_DIR%\lib\x86 SDL2.lib SDL2main.lib opengl32.lib shell32.lib
mkdir %OUT_DIR%
cl /nologo /Zi /MD %INCLUDES% %SOURCES% /Fe%OUT_DIR%/%OUT_EXE%.exe /Fo%OUT_DIR%/ /link %LIBS% /subsystem:console
