// ImGuiTexInspect, a texture inspector widget for dear imgui

#if defined(_MSC_VER) && !defined(_CRT_SECURE_NO_WARNINGS)
#define _CRT_SECURE_NO_WARNINGS
#endif
#if defined EMSCRIPTEN && defined IMGUI_TEX_INSPECT_FLOAT_READ_ENABLED
#warning "Float texture read back is disabled on Emscripten"
#undef IMGUI_TEX_INSPECT_FLOAT_READ_ENABLED
#endif
#include "imgui_tex_inspect_internal.h"

// ==========================================================================
// This file is largely based on:
// https://github.com/ocornut/imgui/blob/master/backends/imgui_impl_opengl3.cpp
//
// In the following section the ImGui_ImplOpenGL3_Init function has been 
// changed to not rewrite global ImGui state.  It has also been wrapped in a 
// namespace to not clash with the main ImGui version.  Aside from that this 
// section is identical to the imgui original.
//
// It's reproduced here because none of this code is exposed in the ImGui API
// in a way that be reused (nor should it be).
//
// Search for "END COPIED" to find the end of the copied segment.
// ===========================================================================

// COPIED FROM imgui_impl_opengl3.cp  ////////////////////////////////////////
#if defined(_MSC_VER) && !defined(_CRT_SECURE_NO_WARNINGS)
#define _CRT_SECURE_NO_WARNINGS
#endif

#include "imgui.h"
#include "imgui_impl_opengl3.h"
#include <stdio.h>
#if defined(_MSC_VER) && _MSC_VER <= 1500 // MSVC 2008 or earlier
#include <stddef.h>     // intptr_t
#else
#include <stdint.h>     // intptr_t
#endif

// GL includes
#if defined(IMGUI_IMPL_OPENGL_ES2)
#include <GLES2/gl2.h>
#elif defined(IMGUI_IMPL_OPENGL_ES3)
#if (defined(__APPLE__) && (TARGET_OS_IOS || TARGET_OS_TV))
#include <OpenGLES/ES3/gl.h>    // Use GL ES 3
#else
#include <GLES3/gl3.h>          // Use GL ES 3
#endif
#else
// About Desktop OpenGL function loaders:
//  Modern desktop OpenGL doesn't have a standard portable header file to load OpenGL function pointers.
//  Helper libraries are often used for this purpose! Here we are supporting a few common ones (gl3w, glew, glad).
//  You may use another loader/header of your choice (glext, glLoadGen, etc.), or chose to manually implement your own.
#if defined(IMGUI_IMPL_OPENGL_LOADER_GL3W)
#include <GL/gl3w.h>            // Needs to be initialized with gl3wInit() in user's code
#elif defined(IMGUI_IMPL_OPENGL_LOADER_GLEW)
#include <GL/glew.h>            // Needs to be initialized with glewInit() in user's code.
#elif defined(IMGUI_IMPL_OPENGL_LOADER_GLAD)
#include <glad/glad.h>          // Needs to be initialized with gladLoadGL() in user's code.
#elif defined(IMGUI_IMPL_OPENGL_LOADER_GLAD2)
#include <glad/gl.h>            // Needs to be initialized with gladLoadGL(...) or gladLoaderLoadGL() in user's code.
#elif defined(IMGUI_IMPL_OPENGL_LOADER_GLBINDING2)
#ifndef GLFW_INCLUDE_NONE
#define GLFW_INCLUDE_NONE       // GLFW including OpenGL headers causes ambiguity or multiple definition errors.
#endif
#include <glbinding/Binding.h>  // Needs to be initialized with glbinding::Binding::initialize() in user's code.
#include <glbinding/gl/gl.h>
using namespace gl;
#elif defined(IMGUI_IMPL_OPENGL_LOADER_GLBINDING3)
#ifndef GLFW_INCLUDE_NONE
#define GLFW_INCLUDE_NONE       // GLFW including OpenGL headers causes ambiguity or multiple definition errors.
#endif
#include <glbinding/glbinding.h>// Needs to be initialized with glbinding::initialize() in user's code.
#include <glbinding/gl/gl.h>
using namespace gl;
#elif defined(IMGUI_IMPL_OPENGL_LOADER_EPOXY)
#include <epoxy/gl.h>
#else
#include IMGUI_IMPL_OPENGL_LOADER_CUSTOM
#endif
#endif

// Desktop GL 3.2+ has glDrawElementsBaseVertex() which GL ES and WebGL don't have.
#if !defined(IMGUI_IMPL_OPENGL_ES2) && !defined(IMGUI_IMPL_OPENGL_ES3) && defined(GL_VERSION_3_2)
#define IMGUI_IMPL_OPENGL_MAY_HAVE_VTX_OFFSET
#endif

// Desktop GL 3.3+ has glBindSampler()
#if !defined(IMGUI_IMPL_OPENGL_ES2) && !defined(IMGUI_IMPL_OPENGL_ES3) && defined(GL_VERSION_3_3)
#define IMGUI_IMPL_OPENGL_MAY_HAVE_BIND_SAMPLER
#endif

// Desktop GL 3.1+ has GL_PRIMITIVE_RESTART state
#if !defined(IMGUI_IMPL_OPENGL_ES2) && !defined(IMGUI_IMPL_OPENGL_ES3) && defined(GL_VERSION_3_1)
#define IMGUI_IMPL_OPENGL_MAY_HAVE_PRIMITIVE_RESTART
#endif

namespace imgui_impl_opengl
{
// OpenGL Data
static GLuint       g_GlVersion = 0;                // Extracted at runtime using GL_MAJOR_VERSION, GL_MINOR_VERSION queries (e.g. 320 for GL 3.2)
static char         g_GlslVersionString[32] = "";   // Specified by user or detected based on compile time GL settings.
static GLuint       g_ShaderHandle = 0, g_VertHandle = 0, g_FragHandle = 0;
static GLint        g_AttribLocationTex = 0, g_AttribLocationProjMtx = 0;                                // Uniforms location
static GLuint       g_AttribLocationVtxPos = 0, g_AttribLocationVtxUV = 0; // Vertex attributes location
static GLint        g_UniformLocationForceNearestSampling = 0;
static GLint        g_UniformLocationGridWidth = 0;

// Functions
static bool    ImGui_ImplOpenGL3_Init(const char* glsl_version)
{
    // Query for GL version (e.g. 320 for GL 3.2)
#if !defined(IMGUI_IMPL_OPENGL_ES2)
    GLint major = 0;
    GLint minor = 0;
    glGetIntegerv(GL_MAJOR_VERSION, &major);
    glGetIntegerv(GL_MINOR_VERSION, &minor);
    if (major == 0 && minor == 0)
    {
        // Query GL_VERSION in desktop GL 2.x, the string will start with "<major>.<minor>"
        const char* gl_version = (const char*)glGetString(GL_VERSION);
        sscanf(gl_version, "%d.%d", &major, &minor);
    }
    g_GlVersion = (GLuint)(major * 100 + minor * 10);
#else
    g_GlVersion = 200; // GLES 2
#endif


    // Store GLSL version string so we can refer to it later in case we recreate shaders.
    // Note: GLSL version is NOT the same as GL version. Leave this to NULL if unsure.
#if defined(IMGUI_IMPL_OPENGL_ES2)
    if (glsl_version == NULL)
        glsl_version = "#version 100";
#elif defined(IMGUI_IMPL_OPENGL_ES3)
    if (glsl_version == NULL)
        glsl_version = "#version 300 es";
#elif defined(__APPLE__)
    if (glsl_version == NULL)
        glsl_version = "#version 150";
#else
    if (glsl_version == NULL)
        glsl_version = "#version 130";
#endif
    IM_ASSERT((int)strlen(glsl_version) + 2 < IM_ARRAYSIZE(g_GlslVersionString));
    strcpy(g_GlslVersionString, glsl_version);
    strcat(g_GlslVersionString, "\n");

    // Debugging construct to make it easily visible in the IDE and debugger which GL loader has been selected.
    // The code actually never uses the 'gl_loader' variable! It is only here so you can read it!
    // If auto-detection fails or doesn't select the same GL loader file as used by your application,
    // you are likely to get a crash below.
    // You can explicitly select a loader by using '#define IMGUI_IMPL_OPENGL_LOADER_XXX' in imconfig.h or compiler command-line.
    const char* gl_loader = "Unknown";
    IM_UNUSED(gl_loader);
#if defined(IMGUI_IMPL_OPENGL_LOADER_GL3W)
    gl_loader = "GL3W";
#elif defined(IMGUI_IMPL_OPENGL_LOADER_GLEW)
    gl_loader = "GLEW";
#elif defined(IMGUI_IMPL_OPENGL_LOADER_GLAD)
    gl_loader = "GLAD";
#elif defined(IMGUI_IMPL_OPENGL_LOADER_GLAD2)
    gl_loader = "GLAD2";
#elif defined(IMGUI_IMPL_OPENGL_LOADER_GLBINDING2)
    gl_loader = "glbinding2";
#elif defined(IMGUI_IMPL_OPENGL_LOADER_GLBINDING3)
    gl_loader = "glbinding3";
#elif defined(IMGUI_IMPL_OPENGL_LOADER_CUSTOM)
    gl_loader = "custom";
#else
    gl_loader = "none";
#endif

    // Make an arbitrary GL call (we don't actually need the result)
    // IF YOU GET A CRASH HERE: it probably means that you haven't initialized the OpenGL function loader used by this code.
    // Desktop OpenGL 3/4 need a function loader. See the IMGUI_IMPL_OPENGL_LOADER_xxx explanation above.
    GLint current_texture;
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &current_texture);

    return true;
}

// ===========================================================================
//  COPIED FROM A DIFFERENT PART OF imgui_impl_opengl3.cpp
// ===========================================================================

// If you get an error please report on github. You may try different GL context version or GLSL version. See GL<>GLSL version table at the top of this file.
static bool CheckShader(GLuint handle, const char* desc)
{
    GLint status = 0, log_length = 0;
    glGetShaderiv(handle, GL_COMPILE_STATUS, &status);
    glGetShaderiv(handle, GL_INFO_LOG_LENGTH, &log_length);
    if ((GLboolean)status == GL_FALSE)
        fprintf(stderr, "ERROR: ImGui_ImplOpenGL3_CreateDeviceObjects: failed to compile %s!\n", desc);
    if (log_length > 1)
    {
        ImVector<char> buf;
        buf.resize((int)(log_length + 1));
        glGetShaderInfoLog(handle, log_length, NULL, (GLchar*)buf.begin());
        fprintf(stderr, "%s\n", buf.begin());
    }
    return (GLboolean)status == GL_TRUE;
}

// If you get an error please report on GitHub. You may try different GL context version or GLSL version.
static bool CheckProgram(GLuint handle, const char* desc)
{
    GLint status = 0, log_length = 0;
    glGetProgramiv(handle, GL_LINK_STATUS, &status);
    glGetProgramiv(handle, GL_INFO_LOG_LENGTH, &log_length);
    if ((GLboolean)status == GL_FALSE)
        fprintf(stderr, "ERROR: ImGui_ImplOpenGL3_CreateDeviceObjects: failed to link %s! (with GLSL '%s')\n", desc, g_GlslVersionString);
    if (log_length > 1)
    {
        ImVector<char> buf;
        buf.resize((int)(log_length + 1));
        glGetProgramInfoLog(handle, log_length, NULL, (GLchar*)buf.begin());
        fprintf(stderr, "%s\n", buf.begin());
    }
    return (GLboolean)status == GL_TRUE;
}

static void ImGui_ImplOpenGL3_SetupRenderState(ImDrawData* draw_data, int fb_width, int fb_height)
{
    // Setup render state: alpha-blending enabled, no face culling, no depth testing, scissor enabled, polygon fill
    glEnable(GL_BLEND);
    glBlendEquation(GL_FUNC_ADD);
    glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
    glDisable(GL_CULL_FACE);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_STENCIL_TEST);
    glEnable(GL_SCISSOR_TEST);
#ifdef IMGUI_IMPL_OPENGL_MAY_HAVE_PRIMITIVE_RESTART
    if (g_GlVersion >= 310)
        glDisable(GL_PRIMITIVE_RESTART);
#endif
#ifdef GL_POLYGON_MODE
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
#endif

    // Support for GL 4.5 rarely used glClipControl(GL_UPPER_LEFT)
#if defined(GL_CLIP_ORIGIN)
    bool clip_origin_lower_left = true;
    if (g_GlVersion >= 450)
    {
        GLenum current_clip_origin = 0; glGetIntegerv(GL_CLIP_ORIGIN, (GLint*)&current_clip_origin);
        if (current_clip_origin == GL_UPPER_LEFT)
            clip_origin_lower_left = false;
    }
#endif
    // Setup viewport, orthographic projection matrix
    // Our visible imgui space lies from draw_data->DisplayPos (top left) to draw_data->DisplayPos+data_data->DisplaySize (bottom right). DisplayPos is (0,0) for single viewport apps.
    glViewport(0, 0, (GLsizei)fb_width, (GLsizei)fb_height);
    float L = draw_data->DisplayPos.x;
    float R = draw_data->DisplayPos.x + draw_data->DisplaySize.x;
    float T = draw_data->DisplayPos.y;
    float B = draw_data->DisplayPos.y + draw_data->DisplaySize.y;
#if defined(GL_CLIP_ORIGIN)
    if (!clip_origin_lower_left) { float tmp = T; T = B; B = tmp; } // Swap top and bottom if origin is upper left
#endif
    const float ortho_projection[4][4] =
    {
        { 2.0f/(R-L),   0.0f,         0.0f,   0.0f },
        { 0.0f,         2.0f/(T-B),   0.0f,   0.0f },
        { 0.0f,         0.0f,        -1.0f,   0.0f },
        { (R+L)/(L-R),  (T+B)/(B-T),  0.0f,   1.0f },
    };
    glUseProgram(g_ShaderHandle);
    glUniform1i(g_AttribLocationTex, 0);
    glUniformMatrix4fv(g_AttribLocationProjMtx, 1, GL_FALSE, &ortho_projection[0][0]);


#ifdef IMGUI_IMPL_OPENGL_MAY_HAVE_BIND_SAMPLER
    if (g_GlVersion >= 330)
        glBindSampler(0, 0); // We use combined texture/sampler state. Applications using GL 3.3 may set that otherwise.
#endif

    glEnableVertexAttribArray(g_AttribLocationVtxPos);
    glEnableVertexAttribArray(g_AttribLocationVtxUV);
    //glEnableVertexAttribArray(g_AttribLocationVtxColor); //Our shader doesn't use vertex color
    glVertexAttribPointer(g_AttribLocationVtxPos,   2, GL_FLOAT,         GL_FALSE, sizeof(ImDrawVert), (GLvoid*)IM_OFFSETOF(ImDrawVert, pos));
    glVertexAttribPointer(g_AttribLocationVtxUV,    2, GL_FLOAT,         GL_FALSE, sizeof(ImDrawVert), (GLvoid*)IM_OFFSETOF(ImDrawVert, uv));
    //glVertexAttribPointer(g_AttribLocationVtxColor, 4, GL_UNSIGNED_BYTE, GL_TRUE,  sizeof(ImDrawVert), (GLvoid*)IM_OFFSETOF(ImDrawVert, col));
}

// ===========================================================================
//  END COPIED FROM imgui_impl_opengl3.cpp 
// ---------------------------------------------------------------------------
//  Note that a lot of the following code still orginated in 
//  imgui_impl_opengl3.cpp but there are more changes from here on.
// ===========================================================================

// New uniforms for ImGuiTexInspect fragment shader
static GLint g_UniformLocationTextureSize;
static GLint g_UniformLocationColorTransform;
static GLint g_UniformLocationColorOffset;
static GLint g_UniformLocationBackgroundColor;
static GLint g_UniformLocationPremultiplyAlpha;
static GLint g_UniformLocationDisableFinalAlpha;
static GLint g_UniformLocationGrid;


//  Vertex shaders are directly from imgui_impl_opengl3.cpp
const GLchar* vertex_shader_glsl_120 =
    "uniform mat4 ProjMtx;\n"
    "attribute vec2 Position;\n"
    "attribute vec2 UV;\n"
    "attribute vec4 Color;\n"
    "varying vec2 Frag_UV;\n"
    "varying vec4 Frag_Color;\n"
    "void main()\n"
    "{\n"
    "    Frag_UV = UV;\n"
    "    Frag_Color = Color;\n"
    "    gl_Position = ProjMtx * vec4(Position.xy,0,1);\n"
    "}\n";

const GLchar* vertex_shader_glsl_130 =
    "uniform mat4 ProjMtx;\n"
    "in vec2 Position;\n"  
    "in vec2 UV;\n"
    "in vec4 Color;\n"
    "out vec2 Frag_UV;\n"
    "out vec4 Frag_Color;\n"
    "void main()\n"
    "{\n"
    "    Frag_UV = UV;\n"
    "    Frag_Color = Color;\n"
    "    gl_Position = ProjMtx * vec4(Position.xy,0,1);\n"
    "}\n";

const GLchar* vertex_shader_glsl_300_es =
    "precision mediump float;\n"
    "layout (location = 0) in vec2 Position;\n"
    "layout (location = 1) in vec2 UV;\n"
    "layout (location = 2) in vec4 Color;\n"
    "uniform mat4 ProjMtx;\n"
    "out vec2 Frag_UV;\n"
    "out vec4 Frag_Color;\n"
    "void main()\n"
    "{\n"
    "    Frag_UV = UV;\n"
    "    Frag_Color = Color;\n"
    "    gl_Position = ProjMtx * vec4(Position.xy,0,1);\n"
    "}\n";

const GLchar* vertex_shader_glsl_410_core =
    "layout (location = 0) in vec2 Position;\n"
    "layout (location = 1) in vec2 UV;\n"
    "layout (location = 2) in vec4 Color;\n"
    "uniform mat4 ProjMtx;\n"
    "out vec2 Frag_UV;\n"
    "out vec4 Frag_Color;\n"
    "void main()\n"
    "{\n"
    "    Frag_UV = UV;\n"
    "    Frag_Color = Color;\n"
    "    gl_Position = ProjMtx * vec4(Position.xy,0,1);\n"
    "}\n";


//-------------------------------------------------------------------------
// [SECTION] IMGUI_TEX_INSPECT FRAGMENT SHADERS
//-------------------------------------------------------------------------
const GLchar *fragment_shader_glsl_120 = "#ifdef GL_ES\n"
                                         "   precision mediump float;\n"
                                         "#endif\n"
                                         "uniform sampler2D Texture;\n"
                                         "uniform vec2 TextureSize;\n"
                                         "uniform mat4 ColorTransform;\n"
                                         "uniform vec4 ColorOffset;\n"
                                         "uniform vec3 BackgroundColor;\n"
                                         "uniform float PremultiplyAlpha;\n"
                                         "uniform float DisableFinalAlpha;\n"
                                         "uniform bool ForceNearestSampling;\n"
                                         "uniform vec4 Grid;\n"
                                         "uniform vec2 GridWidth;\n"
                                         "varying vec2 Frag_UV;\n"
                                         "void main()\n"
                                         "{\n"
                                         "    vec2 uv;\n"
                                         "    vec2 texel = Frag_UV * TextureSize;\n"
                                         "    if (ForceNearestSampling)\n"
                                         "        uv = (floor(texel) + vec2(0.5,0.5)) / TextureSize;\n"
                                         "    else\n"
                                         "        uv = Frag_UV;\n"
                                         "    vec2 texelEdge = step(mod(texel,vec2(1.0,1.0)),GridWidth);\n"
                                         "    float isGrid = max(texelEdge.x, texelEdge.y);\n"
                                         "    vec4 ct = ColorTransform * texture2D(Texture, uv) + ColorOffset;\n"
                                         "    ct.rgb = ct.rgb * mix(1.0, ct.a, PremultiplyAlpha);\n"
                                         "    ct.rgb += BackgroundColor * (1.0-ct.a);\n"
                                         "    ct.a = mix(ct.a, 1.0, DisableFinalAlpha);\n"
                                         "    ct = mix(ct, vec4(Grid.rgb,1), Grid.a * isGrid);\n"
                                         "    gl_FragColor = ct;\n"
                                         "}\n";

const GLchar *fragment_shader_glsl_130 = "uniform sampler2D Texture;\n"
                                         "uniform vec2 TextureSize;\n"
                                         "uniform mat4 ColorTransform;\n"
                                         "uniform vec4 ColorOffset;\n"
                                         "uniform vec3 BackgroundColor;\n"
                                         "uniform float PremultiplyAlpha;\n"
                                         "uniform float DisableFinalAlpha;\n"
                                         "uniform bool ForceNearestSampling;\n"
                                         "uniform vec4 Grid;\n"
                                         "uniform vec2 GridWidth;\n"
                                         "in vec2 Frag_UV;\n"
                                         "out vec4 Out_Color;\n"
                                         "void main()\n"
                                         "{\n"
                                         "    vec2 uv;\n"
                                         "    vec2 texel = Frag_UV * TextureSize;\n"
                                         "    if (ForceNearestSampling)\n"
                                         "        uv = (floor(texel) + vec2(0.5,0.5)) / TextureSize;\n"
                                         "    else\n"
                                         "        uv = Frag_UV;\n"
                                         "    vec2 texelEdge = step(mod(texel,vec2(1.0,1.0)),GridWidth);\n"
                                         "    float isGrid = max(texelEdge.x, texelEdge.y);\n"
                                         "    vec4 ct = ColorTransform * texture2D(Texture, uv) + ColorOffset;\n"
                                         "    ct.rgb = ct.rgb * mix(1.0, ct.a, PremultiplyAlpha);\n"
                                         "    ct.rgb += BackgroundColor * (1.0-ct.a);\n"
                                         "    ct.a = mix(ct.a, 1.0, DisableFinalAlpha);\n"
                                         "    ct = mix(ct, vec4(Grid.rgb,1), Grid.a * isGrid);\n"
                                         "    Out_Color = ct;\n"
                                         "}\n";

const GLchar *fragment_shader_glsl_300_es = "precision mediump float;\n"
                                            "uniform sampler2D Texture;\n"
                                            "uniform vec2 TextureSize;\n"
                                            "uniform mat4 ColorTransform;\n"
                                            "uniform vec4 ColorOffset;\n"
                                            "uniform vec3 BackgroundColor;\n"
                                            "uniform float PremultiplyAlpha;\n"
                                            "uniform float DisableFinalAlpha;\n"
                                            "uniform bool ForceNearestSampling;\n"
                                            "uniform vec4 Grid;\n"
                                            "uniform vec2 GridWidth;\n"
                                            "in vec2 Frag_UV;\n"
                                            "layout (location = 0) out vec4 Out_Color;\n"
                                            "void main()\n"
                                            "{\n"
                                            "    vec2 uv;\n"
                                            "    vec2 texel = Frag_UV * TextureSize;\n"
                                            "    if (ForceNearestSampling)\n"
                                            "        uv = (floor(texel) + vec2(0.5,0.5)) / TextureSize;\n"
                                            "    else\n"
                                            "        uv = Frag_UV;\n"
                                            "    vec2 texelEdge = step(mod(texel,vec2(1.0,1.0)),GridWidth);\n"
                                            "    float isGrid = max(texelEdge.x, texelEdge.y);\n"
                                            "    vec4 ct = ColorTransform * texture(Texture, uv) + ColorOffset;\n"
                                            "    ct.rgb = ct.rgb * mix(1.0, ct.a, PremultiplyAlpha);\n"
                                            "    ct.rgb += BackgroundColor * (1.0-ct.a);\n"
                                            "    ct.a = mix(ct.a, 1.0, DisableFinalAlpha);\n"
                                            "    ct = mix(ct, vec4(Grid.rgb,1), Grid.a * isGrid);\n"
                                            "    Out_Color = ct;\n"
                                            "}\n";

const GLchar *fragment_shader_glsl_410_core = "uniform sampler2D Texture;\n"
                                              "uniform vec2 TextureSize;\n"
                                              "uniform mat4 ColorTransform;\n"
                                              "uniform vec4 ColorOffset;\n"
                                              "uniform vec3 BackgroundColor;\n"
                                              "uniform float PremultiplyAlpha;\n"
                                              "uniform float DisableFinalAlpha;\n"
                                              "uniform bool ForceNearestSampling;\n"
                                              "uniform vec4 Grid;\n"
                                              "uniform vec2 GridWidth;\n"
                                              "in vec2 Frag_UV;\n"
                                              "layout (location = 0) out vec4 Out_Color;\n"
                                              "void main()\n"
                                              "{\n"
                                              "    vec2 uv;\n"
                                              "    vec2 texel = Frag_UV * TextureSize;\n"
                                              "    if (ForceNearestSampling)\n"
                                              "        uv = (floor(texel) + vec2(0.5,0.5)) / TextureSize;\n"
                                              "    else\n"
                                              "        uv = Frag_UV;\n"
                                              "    vec2 texelEdge = step(mod(texel,vec2(1.0,1.0)),GridWidth);\n"
                                              "    float isGrid = max(texelEdge.x, texelEdge.y);\n"
                                              "    vec4 ct = ColorTransform * texture2D(Texture, uv) + ColorOffset;\n"
                                              "    ct.rgb = ct.rgb * mix(1.0, ct.a, PremultiplyAlpha);\n"
                                              "    ct.rgb += BackgroundColor * (1.0-ct.a);\n"
                                              "    ct.a = mix(ct.a, 1.0, DisableFinalAlpha);\n"
                                              "    ct = mix(ct, vec4(Grid.rgb,1), Grid.a * isGrid);\n"
                                              "    Out_Color = ct;\n"
                                              "}\n";

/* BuildShader is from imgui_impl_opengl3.cpp.  Only change is to query the 
 * additional uniform locations for the new fragment shader*/
void BuildShader()
{
    // Shader selection code based on imgui_impl_opengl3.cpp

    // Parse GLSL version string
    int glsl_version = 130;
    sscanf(g_GlslVersionString, "#version %d", &glsl_version);

    // Select shaders matching our GLSL versions
    const GLchar *vertex_shader = NULL;
    const GLchar *fragment_shader = NULL;
    if (glsl_version < 130)
    {
        vertex_shader = vertex_shader_glsl_120;
        fragment_shader = fragment_shader_glsl_120;
    }
    else if (glsl_version >= 410)
    {
        vertex_shader = vertex_shader_glsl_410_core;
        fragment_shader = fragment_shader_glsl_410_core;
    }
    else if (glsl_version == 300)
    {
        vertex_shader = vertex_shader_glsl_300_es;
        fragment_shader = fragment_shader_glsl_300_es;
    }
    else
    {
        vertex_shader = vertex_shader_glsl_130;
        fragment_shader = fragment_shader_glsl_130;
    }

    if (fragment_shader == NULL)
    {
        fprintf(stderr, "ERROR: imgui_tex_inspect fragment shader for %s not implemented yet", g_GlslVersionString);
    }
    else
    {
        // Create shaders
        const GLchar *vertex_shader_with_version[2] = {g_GlslVersionString, vertex_shader};
        g_VertHandle = glCreateShader(GL_VERTEX_SHADER);
        glShaderSource(g_VertHandle, 2, vertex_shader_with_version, NULL);
        glCompileShader(g_VertHandle);
        CheckShader(g_VertHandle, "vertex shader");

        const GLchar *fragment_shader_with_version[2] = {g_GlslVersionString, fragment_shader};
        g_FragHandle = glCreateShader(GL_FRAGMENT_SHADER);
        glShaderSource(g_FragHandle, 2, fragment_shader_with_version, NULL);
        glCompileShader(g_FragHandle);
        CheckShader(g_FragHandle, "fragment shader");

        g_ShaderHandle = glCreateProgram();
        glAttachShader(g_ShaderHandle, g_VertHandle);
        glAttachShader(g_ShaderHandle, g_FragHandle);
        glLinkProgram(g_ShaderHandle);
        CheckProgram(g_ShaderHandle, "shader program");

        g_AttribLocationTex      = glGetUniformLocation(g_ShaderHandle, "Texture");
        g_AttribLocationProjMtx  = glGetUniformLocation(g_ShaderHandle, "ProjMtx");
        g_AttribLocationVtxPos   = (GLuint)glGetAttribLocation(g_ShaderHandle, "Position");
        g_AttribLocationVtxUV    = (GLuint)glGetAttribLocation(g_ShaderHandle, "UV");

        // Change from imgui_impl_opengl3.cpp  (Our shader doesn't use vertex color)
        //g_AttribLocationVtxColor = (GLuint)glGetAttribLocation(g_ShaderHandle, "Color");

        // New uniforms used by imgui_tex_inspect 
        g_UniformLocationTextureSize          = glGetUniformLocation(g_ShaderHandle, "TextureSize");
        g_UniformLocationColorTransform       = glGetUniformLocation(g_ShaderHandle, "ColorTransform");
        g_UniformLocationColorOffset          = glGetUniformLocation(g_ShaderHandle, "ColorOffset");
        g_UniformLocationBackgroundColor      = glGetUniformLocation(g_ShaderHandle, "BackgroundColor");
        g_UniformLocationPremultiplyAlpha     = glGetUniformLocation(g_ShaderHandle, "PremultiplyAlpha");
        g_UniformLocationDisableFinalAlpha    = glGetUniformLocation(g_ShaderHandle, "DisableFinalAlpha");
        g_UniformLocationGrid                 = glGetUniformLocation(g_ShaderHandle, "Grid");
        g_UniformLocationForceNearestSampling = glGetUniformLocation(g_ShaderHandle, "ForceNearestSampling");
        g_UniformLocationGridWidth            = glGetUniformLocation(g_ShaderHandle, "GridWidth");
    }
}

} // namespace imgui_impl_opengl


namespace ImGuiTexInspect
{
using namespace imgui_impl_opengl;

static GLuint readbackFramebuffer = 0;

//-------------------------------------------------------------------------
// [SECTION] Init and Shutdown
//-------------------------------------------------------------------------

bool ImplOpenGL3_Init(const char *glsl_version)
{
    imgui_impl_opengl::ImGui_ImplOpenGL3_Init(glsl_version);
    BuildShader();
    glGenFramebuffers(1, &readbackFramebuffer);
    return true;
}
void ImplOpenGl3_Shutdown()
{
    // No need to call ImGui_ImplOpenGL3_Shutdown, it doesn't even
    // exist in the imgui_impl_opengl namespace.  Our version of
    // ImGui_ImplOpenGL3_Init doesn't affect OpenGL state.

    glDeleteShader(g_VertHandle);
    glDeleteShader(g_FragHandle);
    glDeleteProgram(g_ShaderHandle);

    g_VertHandle = 0;
    g_FragHandle = 0;
    g_ShaderHandle = 0;

    glDeleteFramebuffers(1, &readbackFramebuffer);
    readbackFramebuffer = 0;
}

void GiveNotInitializedWarning()
{
    static bool warningGiven = false;
    if (!warningGiven)
    {
        fprintf(stderr, "ERROR: ImGuiTexInspect backend not initialized\n");
        warningGiven = true;
    }
}

//-------------------------------------------------------------------------
// [SECTION] BackEnd functions declared in imgui_tex_inspect_internal.h
//-------------------------------------------------------------------------

void BackEnd_SetShader(const ImDrawList *, const ImDrawCmd *, const Inspector *inspector)
{
    const ShaderOptions *texConversion = &inspector->CachedShaderOptions;
    if (g_ShaderHandle)
    {
        ImDrawData *draw_data = ImGui::GetDrawData();
        int fb_width = (int)(draw_data->DisplaySize.x * draw_data->FramebufferScale.x);
        int fb_height = (int)(draw_data->DisplaySize.y * draw_data->FramebufferScale.y);

        if (fb_width <= 0 || fb_height <= 0)
            return;

        // Setup normal ImGui GL state
        ImGui_ImplOpenGL3_SetupRenderState(draw_data, fb_width, fb_height);

        // Setup imgui_tex_inspect specific shader uniforms
        glUniformMatrix4fv(g_UniformLocationColorTransform, 1, GL_FALSE, texConversion->ColorTransform);
        glUniform2fv(g_UniformLocationTextureSize,          1, &inspector->TextureSize.x);
        glUniform4fv(g_UniformLocationColorOffset,          1, texConversion->ColorOffset);
        glUniform3fv(g_UniformLocationBackgroundColor,      1, &texConversion->BackgroundColor.x);
        glUniform1f(g_UniformLocationPremultiplyAlpha,         texConversion->PremultiplyAlpha);
        glUniform1f(g_UniformLocationDisableFinalAlpha,        texConversion->DisableFinalAlpha);
        glUniform1i(g_UniformLocationForceNearestSampling,     texConversion->ForceNearestSampling);
        glUniform2fv(g_UniformLocationGridWidth,            1, &texConversion->GridWidth.x);
        glUniform4fv(g_UniformLocationGrid,                 1, &texConversion->GridColor.x);
    }
    else
    {
        GiveNotInitializedWarning();
    }
}
bool BackEnd_GetData(Inspector *inspector, ImTextureID texture, int /*x*/, int /*y*/, int /*width*/, int /*height*/, BufferDesc *bufferDesc)
{
    // Current simple implementation just gets data for whole texture

    if (readbackFramebuffer == 0)
    {
        GiveNotInitializedWarning();
        return false;
    }
    const int numChannels = 4;
    glGetError(); // Discard any current error so that check at end of function is useful
    void * data;
    int texWidth     = (int)inspector->TextureSize.x;
    int texHeight    = (int)inspector->TextureSize.y;
    GLuint glTexture = (GLuint)(uintptr_t)texture; //Double cast to avoid warning

#ifdef IMGUI_TEX_INSPECT_FLOAT_READ_ENABLED
    size_t bufferSize      = sizeof(float) * texWidth * texHeight * numChannels;
    bufferDesc->Data_float = (float *)GetBuffer(inspector, bufferSize);
    GLuint type            = GL_FLOAT;
    data = (void *)bufferDesc->Data_float;
#else
    size_t bufferSize        = sizeof(uint8_t) * texWidth * texHeight * numChannels;
    bufferDesc->Data_uint8_t = (uint8_t *)GetBuffer(inspector, bufferSize);
    GLuint type              = GL_UNSIGNED_BYTE;
    data                     = (void *)bufferDesc->Data_uint8_t;
#endif

    if (data == NULL)
        return false;

    bufferDesc->BufferByteSize = bufferSize;
    bufferDesc->Red            = 0; // Data is packed such that red channel is first
    bufferDesc->Green          = 1; // then green, then blue, the alpha. Hence, 0,1,2,3
    bufferDesc->Blue           = 2; // for these channel order values.
    bufferDesc->Alpha          = 3;
    bufferDesc->ChannelCount   = 4; // RGBA
    bufferDesc->LineStride     = (int)inspector->TextureSize.x * numChannels;
    bufferDesc->Stride         = 4; // No padding between each RGBA texel
    bufferDesc->StartX         = 0; // We queried the whole texture
    bufferDesc->StartY         = 0;
    bufferDesc->Width          = texWidth;
    bufferDesc->Height         = texHeight;

    // Save current frame buffer so we can restore it
    GLuint currentFramebuffer;
    glGetIntegerv(GL_FRAMEBUFFER_BINDING, (GLint *)&currentFramebuffer);

    // Read texture data
    glBindFramebuffer(GL_FRAMEBUFFER, readbackFramebuffer);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, glTexture, 0);
    glReadPixels(0, 0, texWidth, texHeight, GL_RGBA, type, data);

    // Restore previous framebuffer
    glBindFramebuffer(GL_FRAMEBUFFER, currentFramebuffer);

    return glGetError() == GL_NO_ERROR;
}
} // namespace ImGuiTexInspect
