/*
  SDL3 GPU backend for OpenGL ES — internal header
*/
#include "SDL_internal.h"

#ifndef SDL_gpu_gles_h
#define SDL_gpu_gles_h

#ifdef SDL_GPU_GLES

#include "../SDL_sysgpu.h"

/* ======================================================================== */
/* GL type definitions (avoid pulling in GL headers)                        */
/* ======================================================================== */

typedef unsigned int GLenum;
typedef unsigned int GLuint;
typedef int GLint;
typedef int GLsizei;
typedef unsigned char GLboolean;
typedef float GLfloat;
typedef unsigned int GLbitfield;
typedef void GLvoid;
typedef signed long int GLsizeiptr;
typedef char GLchar;
typedef float GLclampf;
typedef signed long int GLintptr;

/* ======================================================================== */
/* GL constants                                                            */
/* ======================================================================== */

/* Clear bits */
#define GL_COLOR_BUFFER_BIT     0x00004000
#define GL_DEPTH_BUFFER_BIT     0x00000100
#define GL_STENCIL_BUFFER_BIT   0x00000400

/* Framebuffer */
#define GL_FRAMEBUFFER          0x8D40
#define GL_READ_FRAMEBUFFER     0x8CA8
#define GL_DRAW_FRAMEBUFFER     0x8CA9
#define GL_COLOR_ATTACHMENT0    0x8CE0
#define GL_DEPTH_ATTACHMENT     0x8D00
#define GL_STENCIL_ATTACHMENT   0x8D20
#define GL_DEPTH_STENCIL_ATTACHMENT 0x821A

/* Boolean / misc */
#define GL_TRUE                 1
#define GL_FALSE                0
#define GL_VERSION              0x1F02
#define GL_NO_ERROR             0

/* State enables */
#define GL_SCISSOR_TEST         0x0C11
#define GL_DEPTH_TEST           0x0B71
#define GL_BLEND                0x0BE2
#define GL_CULL_FACE            0x0B44
#define GL_STENCIL_TEST         0x0B90
#define GL_SAMPLE_ALPHA_TO_COVERAGE 0x809E
#define GL_POLYGON_OFFSET_FILL  0x8037

/* Shader types */
#define GL_VERTEX_SHADER        0x8B31
#define GL_FRAGMENT_SHADER      0x8B30
#define GL_COMPILE_STATUS       0x8B81
#define GL_LINK_STATUS          0x8B82
#define GL_INFO_LOG_LENGTH      0x8B84
#define GL_ACTIVE_UNIFORM_BLOCKS 0x8A36

/* Primitive types */
#define GL_POINTS               0x0000
#define GL_LINES                0x0001
#define GL_LINE_STRIP           0x0003
#define GL_TRIANGLES            0x0004
#define GL_TRIANGLE_STRIP       0x0005

/* Face state */
#define GL_FRONT                0x0404
#define GL_BACK                 0x0405
#define GL_FRONT_AND_BACK       0x0408
#define GL_CW                   0x0900
#define GL_CCW                  0x0901

/* Comparison functions */
#define GL_NEVER                0x0200
#define GL_LESS                 0x0201
#define GL_EQUAL                0x0202
#define GL_LEQUAL               0x0203
#define GL_GREATER              0x0204
#define GL_NOTEQUAL             0x0205
#define GL_GEQUAL               0x0206
#define GL_ALWAYS               0x0207

/* Stencil ops */
#define GL_KEEP                 0x1E00
#define GL_ZERO_OP              0x0000  /* GL_ZERO — renamed to avoid collision */
#define GL_REPLACE              0x1E01
#define GL_INCR                 0x1E02
#define GL_DECR                 0x1E03
#define GL_INVERT               0x150A
#define GL_INCR_WRAP            0x8507
#define GL_DECR_WRAP            0x8508

/* Blend factors */
#define GL_BF_ZERO              0x0000  /* renamed to avoid collision */
#define GL_ONE                  1
#define GL_SRC_COLOR            0x0300
#define GL_ONE_MINUS_SRC_COLOR  0x0301
#define GL_SRC_ALPHA            0x0302
#define GL_ONE_MINUS_SRC_ALPHA  0x0303
#define GL_DST_ALPHA            0x0304
#define GL_ONE_MINUS_DST_ALPHA  0x0305
#define GL_DST_COLOR            0x0306
#define GL_ONE_MINUS_DST_COLOR  0x0307
#define GL_SRC_ALPHA_SATURATE   0x0308
#define GL_CONSTANT_COLOR       0x8001
#define GL_ONE_MINUS_CONSTANT_COLOR 0x8002

/* Blend equations */
#define GL_FUNC_ADD             0x8006
#define GL_FUNC_SUBTRACT        0x800A
#define GL_FUNC_REVERSE_SUBTRACT 0x800B
#define GL_MIN                  0x8007
#define GL_MAX                  0x8008

/* Data types (also used as vertex format types) */
#define GL_BYTE                 0x1400
#define GL_UNSIGNED_BYTE        0x1401
#define GL_SHORT                0x1402
#define GL_UNSIGNED_SHORT       0x1403
#define GL_INT                  0x1404
#define GL_UNSIGNED_INT         0x1405
#define GL_HALF_FLOAT           0x140B
#define GL_FLOAT                0x1406
#define GL_UNSIGNED_INT_24_8    0x84FA
#define GL_FLOAT_32_UNSIGNED_INT_24_8_REV 0x8DAD
#define GL_UNSIGNED_SHORT_5_6_5         0x8363
#define GL_UNSIGNED_SHORT_4_4_4_4       0x8033
#define GL_UNSIGNED_INT_2_10_10_10_REV  0x8368
#define GL_UNSIGNED_INT_10F_11F_11F_REV 0x8C3B

/* Buffer targets */
#define GL_ARRAY_BUFFER         0x8892
#define GL_ELEMENT_ARRAY_BUFFER 0x8893
#define GL_UNIFORM_BUFFER       0x8A11
#define GL_COPY_WRITE_BUFFER    0x8F37

/* Buffer usage */
#define GL_DYNAMIC_DRAW         0x88E8

/* Texture targets */
#define GL_TEXTURE_2D           0x0DE1
#define GL_TEXTURE_3D           0x806F
#define GL_TEXTURE_CUBE_MAP     0x8513
#define GL_TEXTURE_2D_ARRAY     0x8C1A
#define GL_TEXTURE_2D_MULTISAMPLE 0x9100
#define GL_TEXTURE_CUBE_MAP_POSITIVE_X 0x8515
#define GL_TEXTURE0             0x84C0

/* Texture parameters */
#define GL_TEXTURE_MIN_FILTER   0x2801
#define GL_TEXTURE_MAG_FILTER   0x2800
#define GL_TEXTURE_WRAP_S       0x2802
#define GL_TEXTURE_WRAP_T       0x2803
#define GL_TEXTURE_WRAP_R       0x8072
#define GL_TEXTURE_MAX_LEVEL    0x813D
#define GL_TEXTURE_MIN_LOD      0x813A
#define GL_TEXTURE_MAX_LOD      0x813B
#define GL_TEXTURE_COMPARE_MODE 0x884C
#define GL_TEXTURE_COMPARE_FUNC 0x884D
#define GL_COMPARE_REF_TO_TEXTURE 0x884E
#define GL_TEXTURE_MAX_ANISOTROPY_EXT 0x84FE

/* Sampler filter modes */
#define GL_NEAREST              0x2600
#define GL_LINEAR               0x2601
#define GL_NEAREST_MIPMAP_NEAREST 0x2700
#define GL_LINEAR_MIPMAP_NEAREST 0x2701
#define GL_NEAREST_MIPMAP_LINEAR 0x2702
#define GL_LINEAR_MIPMAP_LINEAR 0x2703

/* Sampler wrap modes */
#define GL_REPEAT               0x2901
#define GL_CLAMP_TO_EDGE        0x812F
#define GL_MIRRORED_REPEAT      0x8370

/* Texture internal formats */
#define GL_R8                   0x8229
#define GL_R8_SNORM             0x8F94
#define GL_R16F                 0x822D
#define GL_R32F                 0x822E
#define GL_R8UI                 0x8232
#define GL_R8I                  0x8231
#define GL_R16UI                0x8234
#define GL_R16I                 0x8233
#define GL_R32UI                0x8236
#define GL_R32I                 0x8235
#define GL_RG8                  0x822B
#define GL_RG8_SNORM            0x8F95
#define GL_RG16F                0x822F
#define GL_RG32F                0x8230
#define GL_RG8UI                0x8238
#define GL_RG8I                 0x8237
#define GL_RG16UI               0x823A
#define GL_RG16I                0x8239
#define GL_RG32UI               0x823C
#define GL_RG32I                0x823B
#define GL_RGB8                 0x8051
#define GL_SRGB8                0x8C41
#define GL_RGB565               0x8D62
#define GL_R11F_G11F_B10F       0x8C3A
#define GL_RGBA8                0x8058
#define GL_RGBA8_SNORM          0x8F97
#define GL_SRGB8_ALPHA8         0x8C43
#define GL_RGBA4                0x8056
#define GL_RGB10_A2             0x8059
#define GL_RGBA16F              0x881A
#define GL_RGBA32F              0x8814
#define GL_RGBA8UI              0x8D7C
#define GL_RGBA8I               0x8D8E
#define GL_RGBA16UI             0x8D76
#define GL_RGBA16I              0x8D88
#define GL_RGBA32UI             0x8D70
#define GL_RGBA32I              0x8D82
#define GL_DEPTH_COMPONENT16    0x81A5
#define GL_DEPTH_COMPONENT24    0x81A6
#define GL_DEPTH_COMPONENT32F   0x8CAC
#define GL_DEPTH24_STENCIL8     0x88F0
#define GL_DEPTH32F_STENCIL8    0x8CAD

/* Pixel base formats */
#define GL_RED                  0x1903
#define GL_RG                   0x8227
#define GL_RGB                  0x1907
#define GL_RGBA                 0x1908
#define GL_RED_INTEGER          0x8D94
#define GL_RG_INTEGER           0x8228
#define GL_RGB_INTEGER          0x8D98
#define GL_RGBA_INTEGER         0x8D99
#define GL_DEPTH_COMPONENT      0x1902
#define GL_DEPTH_STENCIL        0x84F9

/* Pixel store */
#define GL_UNPACK_ROW_LENGTH    0x0CF2

/* ======================================================================== */
/* GL function pointer types                                               */
/* ======================================================================== */

/* Basic state */
typedef void (*PFNGLCLEARCOLORPROC)(GLclampf r, GLclampf g, GLclampf b, GLclampf a);
typedef void (*PFNGLCLEARDEPTHFPROC)(GLclampf d);
typedef void (*PFNGLCLEARSTENCILPROC)(GLint s);
typedef void (*PFNGLCLEARPROC)(GLbitfield mask);
typedef void (*PFNGLVIEWPORTPROC)(GLint x, GLint y, GLsizei w, GLsizei h);
typedef void (*PFNGLSCISSORPROC)(GLint x, GLint y, GLsizei w, GLsizei h);
typedef void (*PFNGLENABLEPROC)(GLenum cap);
typedef void (*PFNGLDISABLEPROC)(GLenum cap);
typedef void (*PFNGLFINISHPROC)(void);
typedef void (*PFNGLFLUSHPROC)(void);
typedef GLenum (*PFNGLGETERRORPROC)(void);
typedef const GLchar *(*PFNGLGETSTRINGPROC)(GLenum name);
typedef void (*PFNGLCOLORMASKPROC)(GLboolean r, GLboolean g, GLboolean b, GLboolean a);
typedef void (*PFNGLDEPTHMASKPROC)(GLboolean flag);
typedef void (*PFNGLSTENCILMASKPROC)(GLuint mask);
typedef void (*PFNGLBINDFRAMEBUFFERPROC)(GLenum target, GLuint fbo);
typedef void (*PFNGLPIXELSTOREIPROC)(GLenum pname, GLint param);

/* Shader compilation */
typedef GLuint (*PFNGLCREATESHADERPROC)(GLenum type);
typedef void (*PFNGLDELETESHADERPROC)(GLuint shader);
typedef void (*PFNGLSHADERSOURCEPROC)(GLuint shader, GLsizei count, const GLchar *const*string, const GLint *length);
typedef void (*PFNGLCOMPILESHADERPROC)(GLuint shader);
typedef void (*PFNGLGETSHADERIVPROC)(GLuint shader, GLenum pname, GLint *params);
typedef void (*PFNGLGETSHADERINFOLOGPROC)(GLuint shader, GLsizei bufSize, GLsizei *length, GLchar *infoLog);

/* Program linking */
typedef GLuint (*PFNGLCREATEPROGRAMPROC)(void);
typedef void (*PFNGLDELETEPROGRAMPROC)(GLuint program);
typedef void (*PFNGLATTACHSHADERPROC)(GLuint program, GLuint shader);
typedef void (*PFNGLLINKPROGRAMPROC)(GLuint program);
typedef void (*PFNGLUSEPROGRAMPROC)(GLuint program);
typedef void (*PFNGLGETPROGRAMIVPROC)(GLuint program, GLenum pname, GLint *params);
typedef void (*PFNGLGETPROGRAMINFOLOGPROC)(GLuint program, GLsizei bufSize, GLsizei *length, GLchar *infoLog);

/* Uniform blocks */
typedef void (*PFNGLUNIFORMBLOCKBINDINGPROC)(GLuint program, GLuint uniformBlockIndex, GLuint uniformBlockBinding);
typedef void (*PFNGLGETACTIVEUNIFORMBLOCKNAMEPROC)(GLuint program, GLuint uniformBlockIndex, GLsizei bufSize, GLsizei *length, GLchar *uniformBlockName);

/* Rasterizer state */
typedef void (*PFNGLCULLFACEPROC)(GLenum mode);
typedef void (*PFNGLFRONTFACEPROC)(GLenum mode);
typedef void (*PFNGLPOLYGONOFFSETPROC)(GLfloat factor, GLfloat units);

/* Depth/Stencil state */
typedef void (*PFNGLDEPTHFUNCPROC)(GLenum func);
typedef void (*PFNGLSTENCILFUNCSEPARATEPROC)(GLenum face, GLenum func, GLint ref, GLuint mask);
typedef void (*PFNGLSTENCILOPSEPARATEPROC)(GLenum face, GLenum sfail, GLenum dpfail, GLenum dppass);
typedef void (*PFNGLSTENCILMASKSEPARATEPROC)(GLenum face, GLuint mask);

/* Blend state */
typedef void (*PFNGLBLENDFUNCSEPARATEPROC)(GLenum srcRGB, GLenum dstRGB, GLenum srcA, GLenum dstA);
typedef void (*PFNGLBLENDEQUATIONSEPARATEPROC)(GLenum modeRGB, GLenum modeA);

/* Buffers */
typedef void (*PFNGLGENBUFFERSPROC)(GLsizei n, GLuint *buffers);
typedef void (*PFNGLDELETEBUFFERSPROC)(GLsizei n, const GLuint *buffers);
typedef void (*PFNGLBINDBUFFERPROC)(GLenum target, GLuint buffer);
typedef void (*PFNGLBUFFERDATAPROC)(GLenum target, GLsizeiptr size, const void *data, GLenum usage);
typedef void (*PFNGLBUFFERSUBDATAPROC)(GLenum target, GLintptr offset, GLsizeiptr size, const void *data);
typedef void (*PFNGLBINDBUFFERBASEPROC)(GLenum target, GLuint index, GLuint buffer);

/* Textures */
typedef void (*PFNGLGENTEXTURESPROC)(GLsizei n, GLuint *textures);
typedef void (*PFNGLDELETETEXTURESPROC)(GLsizei n, const GLuint *textures);
typedef void (*PFNGLBINDTEXTUREPROC)(GLenum target, GLuint texture);
typedef void (*PFNGLACTIVETEXTUREPROC)(GLenum texture);
typedef void (*PFNGLTEXSTORAGE2DPROC)(GLenum target, GLsizei levels, GLenum internalformat, GLsizei w, GLsizei h);
typedef void (*PFNGLTEXSTORAGE3DPROC)(GLenum target, GLsizei levels, GLenum internalformat, GLsizei w, GLsizei h, GLsizei d);
typedef void (*PFNGLTEXSUBIMAGE2DPROC)(GLenum target, GLint level, GLint x, GLint y, GLsizei w, GLsizei h, GLenum format, GLenum type, const void *pixels);
typedef void (*PFNGLTEXSUBIMAGE3DPROC)(GLenum target, GLint level, GLint x, GLint y, GLint z, GLsizei w, GLsizei h, GLsizei d, GLenum format, GLenum type, const void *pixels);
typedef void (*PFNGLTEXPARAMETERIPROC)(GLenum target, GLenum pname, GLint param);

/* Samplers */
typedef void (*PFNGLGENSAMPLERSPROC)(GLsizei n, GLuint *samplers);
typedef void (*PFNGLDELETESAMPLERSPROC)(GLsizei n, const GLuint *samplers);
typedef void (*PFNGLBINDSAMPLERPROC)(GLuint unit, GLuint sampler);
typedef void (*PFNGLSAMPLERPARAMETERIPROC)(GLuint sampler, GLenum pname, GLint param);
typedef void (*PFNGLSAMPLERPARAMETERFPROC)(GLuint sampler, GLenum pname, GLfloat param);

/* Vertex arrays */
typedef void (*PFNGLGENVERTEXARRAYSPROC)(GLsizei n, GLuint *arrays);
typedef void (*PFNGLDELETEVERTEXARRAYSPROC)(GLsizei n, const GLuint *arrays);
typedef void (*PFNGLBINDVERTEXARRAYPROC)(GLuint array);
typedef void (*PFNGLENABLEVERTEXATTRIBARRAYPROC)(GLuint index);
typedef void (*PFNGLDISABLEVERTEXATTRIBARRAYPROC)(GLuint index);
typedef void (*PFNGLVERTEXATTRIBPOINTERPROC)(GLuint index, GLint size, GLenum type, GLboolean normalized, GLsizei stride, const void *pointer);
typedef void (*PFNGLVERTEXATTRIBIPOINTERPROC)(GLuint index, GLint size, GLenum type, GLsizei stride, const void *pointer);
typedef void (*PFNGLVERTEXATTRIBDIVISORPROC)(GLuint index, GLuint divisor);

/* Draw */
typedef void (*PFNGLDRAWARRAYSPROC)(GLenum mode, GLint first, GLsizei count);
typedef void (*PFNGLDRAWELEMENTSPROC)(GLenum mode, GLsizei count, GLenum type, const void *indices);
typedef void (*PFNGLDRAWARRAYSINSTANCEDPROC)(GLenum mode, GLint first, GLsizei count, GLsizei instancecount);
typedef void (*PFNGLDRAWELEMENTSINSTANCEDPROC)(GLenum mode, GLsizei count, GLenum type, const void *indices, GLsizei instancecount);

/* ======================================================================== */
/* Backend renderer state                                                  */
/* ======================================================================== */

typedef struct GLESRenderer
{
    /* GL function pointers — basic state */
    PFNGLCLEARCOLORPROC glClearColor;
    PFNGLCLEARDEPTHFPROC glClearDepthf;
    PFNGLCLEARSTENCILPROC glClearStencil;
    PFNGLCLEARPROC glClear;
    PFNGLVIEWPORTPROC glViewport;
    PFNGLSCISSORPROC glScissor;
    PFNGLENABLEPROC glEnable;
    PFNGLDISABLEPROC glDisable;
    PFNGLFINISHPROC glFinish;
    PFNGLFLUSHPROC glFlush;
    PFNGLGETERRORPROC glGetError;
    PFNGLGETSTRINGPROC glGetString;
    PFNGLCOLORMASKPROC glColorMask;
    PFNGLDEPTHMASKPROC glDepthMask;
    PFNGLSTENCILMASKPROC glStencilMask;
    PFNGLBINDFRAMEBUFFERPROC glBindFramebuffer;
    PFNGLPIXELSTOREIPROC glPixelStorei;

    /* Shader compilation */
    PFNGLCREATESHADERPROC glCreateShader;
    PFNGLDELETESHADERPROC glDeleteShader;
    PFNGLSHADERSOURCEPROC glShaderSource;
    PFNGLCOMPILESHADERPROC glCompileShader;
    PFNGLGETSHADERIVPROC glGetShaderiv;
    PFNGLGETSHADERINFOLOGPROC glGetShaderInfoLog;

    /* Program linking */
    PFNGLCREATEPROGRAMPROC glCreateProgram;
    PFNGLDELETEPROGRAMPROC glDeleteProgram;
    PFNGLATTACHSHADERPROC glAttachShader;
    PFNGLLINKPROGRAMPROC glLinkProgram;
    PFNGLUSEPROGRAMPROC glUseProgram;
    PFNGLGETPROGRAMIVPROC glGetProgramiv;
    PFNGLGETPROGRAMINFOLOGPROC glGetProgramInfoLog;

    /* Uniform blocks */
    PFNGLUNIFORMBLOCKBINDINGPROC glUniformBlockBinding;
    PFNGLGETACTIVEUNIFORMBLOCKNAMEPROC glGetActiveUniformBlockName;

    /* Rasterizer state */
    PFNGLCULLFACEPROC glCullFace;
    PFNGLFRONTFACEPROC glFrontFace;
    PFNGLPOLYGONOFFSETPROC glPolygonOffset;

    /* Depth/stencil state */
    PFNGLDEPTHFUNCPROC glDepthFunc;
    PFNGLSTENCILFUNCSEPARATEPROC glStencilFuncSeparate;
    PFNGLSTENCILOPSEPARATEPROC glStencilOpSeparate;
    PFNGLSTENCILMASKSEPARATEPROC glStencilMaskSeparate;

    /* Blend state */
    PFNGLBLENDFUNCSEPARATEPROC glBlendFuncSeparate;
    PFNGLBLENDEQUATIONSEPARATEPROC glBlendEquationSeparate;

    /* Buffers */
    PFNGLGENBUFFERSPROC glGenBuffers;
    PFNGLDELETEBUFFERSPROC glDeleteBuffers;
    PFNGLBINDBUFFERPROC glBindBuffer;
    PFNGLBUFFERDATAPROC glBufferData;
    PFNGLBUFFERSUBDATAPROC glBufferSubData;
    PFNGLBINDBUFFERBASEPROC glBindBufferBase;

    /* Textures */
    PFNGLGENTEXTURESPROC glGenTextures;
    PFNGLDELETETEXTURESPROC glDeleteTextures;
    PFNGLBINDTEXTUREPROC glBindTexture;
    PFNGLACTIVETEXTUREPROC glActiveTexture;
    PFNGLTEXSTORAGE2DPROC glTexStorage2D;
    PFNGLTEXSTORAGE3DPROC glTexStorage3D;
    PFNGLTEXSUBIMAGE2DPROC glTexSubImage2D;
    PFNGLTEXSUBIMAGE3DPROC glTexSubImage3D;
    PFNGLTEXPARAMETERIPROC glTexParameteri;

    /* Samplers */
    PFNGLGENSAMPLERSPROC glGenSamplers;
    PFNGLDELETESAMPLERSPROC glDeleteSamplers;
    PFNGLBINDSAMPLERPROC glBindSampler;
    PFNGLSAMPLERPARAMETERIPROC glSamplerParameteri;
    PFNGLSAMPLERPARAMETERFPROC glSamplerParameterf;

    /* Vertex arrays */
    PFNGLGENVERTEXARRAYSPROC glGenVertexArrays;
    PFNGLDELETEVERTEXARRAYSPROC glDeleteVertexArrays;
    PFNGLBINDVERTEXARRAYPROC glBindVertexArray;
    PFNGLENABLEVERTEXATTRIBARRAYPROC glEnableVertexAttribArray;
    PFNGLDISABLEVERTEXATTRIBARRAYPROC glDisableVertexAttribArray;
    PFNGLVERTEXATTRIBPOINTERPROC glVertexAttribPointer;
    PFNGLVERTEXATTRIBIPOINTERPROC glVertexAttribIPointer;
    PFNGLVERTEXATTRIBDIVISORPROC glVertexAttribDivisor;

    /* Draw */
    PFNGLDRAWARRAYSPROC glDrawArrays;
    PFNGLDRAWELEMENTSPROC glDrawElements;
    PFNGLDRAWARRAYSINSTANCEDPROC glDrawArraysInstanced;
    PFNGLDRAWELEMENTSINSTANCEDPROC glDrawElementsInstanced;

    /* Claimed windows (simple: one window for MVP) */
    SDL_Window *claimed_window;

    /* Debug mode */
    bool debug_mode;
} GLESRenderer;

/* ======================================================================== */
/* Resource objects                                                        */
/* ======================================================================== */

#define GLES_MAX_VERTEX_BUFFERS     16
#define GLES_MAX_VERTEX_ATTRIBUTES  16
#define GLES_MAX_COLOR_TARGETS       4
#define GLES_MAX_UNIFORM_BUFFERS     4

typedef struct GLESBuffer {
    GLuint handle;
    Uint32 size;
    SDL_GPUBufferUsageFlags usage;
} GLESBuffer;

typedef struct GLESTexture {
    SDL_GPUTextureCreateInfo info;
    GLuint handle;
    GLenum target;
    GLenum gl_internalformat;
    GLenum gl_format;
    GLenum gl_type;
    bool is_swapchain;
} GLESTexture;

typedef struct GLESSampler {
    GLuint handle;
} GLESSampler;

typedef struct GLESTransferBuffer {
    void *data;
    Uint32 size;
    SDL_GPUTransferBufferUsage usage;
    bool mapped;
} GLESTransferBuffer;

typedef struct GLESVertexAttribute {
    Uint32 location;
    Uint32 buffer_slot;
    Uint32 offset;
    GLint gl_size;
    GLenum gl_type;
    GLboolean gl_normalized;
    GLboolean gl_integer;
} GLESVertexAttribute;

typedef struct GLESVertexBufferDesc {
    Uint32 slot;
    Uint32 pitch;
    SDL_GPUVertexInputRate input_rate;
    Uint32 instance_step_rate;
} GLESVertexBufferDesc;

typedef struct GLESShader {
    GLuint handle;
    SDL_GPUShaderStage stage;
    Uint32 num_samplers;
    Uint32 num_storage_textures;
    Uint32 num_storage_buffers;
    Uint32 num_uniform_buffers;
    char *glsl_source;
} GLESShader;

typedef struct GLESGraphicsPipeline {
    GraphicsPipelineCommonHeader header;  /* MUST be first field */
    GLuint program;
    GLenum primitive_type;
    GLESVertexBufferDesc vertex_buffers[GLES_MAX_VERTEX_BUFFERS];
    Uint32 num_vertex_buffers;
    GLESVertexAttribute vertex_attributes[GLES_MAX_VERTEX_ATTRIBUTES];
    Uint32 num_vertex_attributes;
    SDL_GPURasterizerState rasterizer;
    SDL_GPUDepthStencilState depth_stencil;
    SDL_GPUMultisampleState multisample;
    SDL_GPUColorTargetBlendState blend_states[GLES_MAX_COLOR_TARGETS];
    Uint32 num_color_targets;
    Uint32 fragment_sampler_count;
} GLESGraphicsPipeline;

/* ======================================================================== */
/* Command buffer                                                          */
/* ======================================================================== */

typedef struct GLESCommandBuffer
{
    CommandBufferCommonHeader header;
    GLESRenderer *renderer;
    SDL_Window *swapchain_window;

    /* Render pass state (deferred bindings) */
    GLESGraphicsPipeline *current_pipeline;
    GLESBuffer *vertex_buffers[GLES_MAX_VERTEX_BUFFERS];
    Uint32 vertex_buffer_offsets[GLES_MAX_VERTEX_BUFFERS];
    GLESBuffer *index_buffer;
    Uint32 index_buffer_offset;
    GLenum index_type;

    /* VAO for vertex input */
    GLuint vao;

    /* UBO ring for push uniforms */
    GLuint vertex_ubos[GLES_MAX_UNIFORM_BUFFERS];
    GLuint fragment_ubos[GLES_MAX_UNIFORM_BUFFERS];
} GLESCommandBuffer;

/* ======================================================================== */
/* Swapchain "texture" — represents the default framebuffer                */
/* ======================================================================== */

typedef struct GLESSwapchainTexture
{
    TextureCommonHeader header;
} GLESSwapchainTexture;

/* ======================================================================== */
/* SPIRV-Cross transpilation                                               */
/* ======================================================================== */

char *GLES_TranspileSPIRV(const Uint8 *spirv_code, size_t spirv_size,
                           const char *entrypoint, SDL_GPUShaderStage stage);

#endif /* SDL_GPU_GLES */
#endif /* SDL_gpu_gles_h */
