#
# Makefile to use with emscripten
# See https://emscripten.org/docs/getting_started/downloads.html
# for installation instructions.
#
# This Makefile assumes you have loaded emscripten's environment.
# (On Windows, you may need to execute emsdk_env.bat or encmdprompt.bat ahead)
#
# Running `make` will produce three files:
#  - web/index.html
#  - web/index.js
#  - web/index.wasm
#
# All three are needed to run the demo.

CC = emcc
CXX = em++
WEB_DIR = web
EXE = $(WEB_DIR)/index.html
#TEX_INSPECT_CHANGE...
IMGUI_DIR = ../../../imgui
TEX_INSPECT_DIR = ../..
#...TEX_INSPECT_CHANGE
SOURCES = main.cpp
SOURCES += $(IMGUI_DIR)/imgui.cpp $(IMGUI_DIR)/imgui_demo.cpp $(IMGUI_DIR)/imgui_draw.cpp $(IMGUI_DIR)/imgui_tables.cpp $(IMGUI_DIR)/imgui_widgets.cpp
SOURCES += $(IMGUI_DIR)/backends/imgui_impl_sdl.cpp $(IMGUI_DIR)/backends/imgui_impl_opengl3.cpp
#TEX_INSPECT_CHANGE...
SOURCES += $(TEX_INSPECT_DIR)/imgui_tex_inspect.cpp $(TEX_INSPECT_DIR)/imgui_tex_inspect_demo.cpp
SOURCES += $(TEX_INSPECT_DIR)/backends/tex_inspect_opengl.cpp
#...TEX_INSPECT_CHANGE
OBJS = $(addsuffix .o, $(basename $(notdir $(SOURCES))))
UNAME_S := $(shell uname -s)

##---------------------------------------------------------------------
## EMSCRIPTEN OPTIONS
##---------------------------------------------------------------------

EMS += -s USE_SDL=2 -s WASM=1
EMS += -s ALLOW_MEMORY_GROWTH=1
EMS += -s DISABLE_EXCEPTION_CATCHING=1 -s NO_EXIT_RUNTIME=0
EMS += -s ASSERTIONS=1

# Uncomment next line to fix possible rendering bugs with Emscripten version older then 1.39.0 (https://github.com/ocornut/imgui/issues/2877)
#EMS += -s BINARYEN_TRAP_MODE=clamp
#EMS += -s SAFE_HEAP=1    ## Adds overhead

# Emscripten allows preloading a file or folder to be accessible at runtime.
# The Makefile for this example project suggests embedding the misc/fonts/ folder into our application, it will then be accessible as "/fonts"
# See documentation for more details: https://emscripten.org/docs/porting/files/packaging_files.html
# (Default value is 0. Set to 1 to enable file-system and include the misc/fonts/ folder as part of the build.)
#TEX_INSPECT_CHANGE...
USE_FILE_SYSTEM ?= 1
#...TEX_INSPECT_CHANGE
ifeq ($(USE_FILE_SYSTEM), 0)
EMS += -s NO_FILESYSTEM=1 -DIMGUI_DISABLE_FILE_FUNCTIONS
endif
ifeq ($(USE_FILE_SYSTEM), 1)
#TEX_INSPECT_CHANGE...
LDFLAGS += --no-heap-copy --preload-file $(IMGUI_DIR)/misc/fonts@/fonts
LDFLAGS += --preload-file ../demo_1.png@/demo_1.png
#...TEX_INSPECT_CHANGE
endif

##---------------------------------------------------------------------
## FINAL BUILD FLAGS
##---------------------------------------------------------------------

#TEX_INSPECT_CHANGE...
CXXFLAGS = -I$(IMGUI_DIR) -I$(IMGUI_DIR)/backends -I$(TEX_INSPECT_DIR) -I$(TEX_INSPECT_DIR)/backends -I$(TEX_INSPECT_DIR)/examples/loaders
#...TEX_INSPECT_CHANGE
#CPPFLAGS += -g
CPPFLAGS += -Wall -Wformat -Os
CPPFLAGS += $(EMS)
LIBS += $(EMS)
LDFLAGS += --shell-file shell_minimal.html

##---------------------------------------------------------------------
## BUILD RULES
##---------------------------------------------------------------------

%.o:%.cpp
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c -o $@ $<

%.o:$(IMGUI_DIR)/%.cpp
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c -o $@ $<

%.o:$(IMGUI_DIR)/backends/%.cpp
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c -o $@ $<

#TEX_INSPECT_CHANGE...
%.o:$(TEX_INSPECT_DIR)/backends/%.cpp
	$(CXX) $(CXXFLAGS) -c -o $@ $<

%.o:$(TEX_INSPECT_DIR)/examples/loaders/%.cpp
	$(CXX) $(CXXFLAGS) -c -o $@ $<

%.o:$(TEX_INSPECT_DIR)/%.cpp
	$(CXX) $(CXXFLAGS) -c -o $@ $<
#...TEX_INSPECT_CHANGE

%.o:../libs/gl3w/GL/%.c
	$(CC) $(CPPFLAGS) $(CFLAGS) -c -o $@ $<

all: $(EXE)
	@echo Build complete for $(EXE)

$(WEB_DIR):
	mkdir $@

serve: all
	python3 -m http.server -d $(WEB_DIR)

$(EXE): $(OBJS) $(WEB_DIR)
	$(CXX) -o $@ $(OBJS) $(LIBS) $(LDFLAGS)

clean:
	rm -rf $(OBJS) $(WEB_DIR)
