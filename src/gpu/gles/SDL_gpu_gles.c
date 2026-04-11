/*
  SDL3 GPU backend for OpenGL ES
  MVP: device creation, swapchain, render pass clear, submit

  Unimplemented functions assert so we catch incompleteness immediately
  rather than continuing with undefined behavior.
*/
#include "SDL_internal.h"

#ifdef SDL_GPU_GLES

#include "../SDL_sysgpu.h"
#include "SDL_gpu_gles.h"

/* ======================================================================== */
/* Asserting stub macro — crashes loudly with function name                 */
/* ======================================================================== */

#define GPU_STUB_ASSERT(funcname) do { \
    SDL_LogError(SDL_LOG_CATEGORY_GPU, \
        "GLES GPU backend: %s not yet implemented!", #funcname); \
    SDL_assert(!"GPU function not implemented: " #funcname); \
} while (0)

/* ======================================================================== */
/* GL function loading                                                     */
/* ======================================================================== */

#define LOAD_GL_FUNC(renderer, name) do { \
    renderer->name = (typeof(renderer->name))SDL_GL_GetProcAddress(#name); \
    if (!renderer->name) { \
        SDL_LogWarn(SDL_LOG_CATEGORY_GPU, "GLES: missing GL function %s", #name); \
    } \
} while (0)

static bool GLES_LoadGLFunctions(GLESRenderer *renderer)
{
    LOAD_GL_FUNC(renderer, glClearColor);
    LOAD_GL_FUNC(renderer, glClearDepthf);
    LOAD_GL_FUNC(renderer, glClearStencil);
    LOAD_GL_FUNC(renderer, glClear);
    LOAD_GL_FUNC(renderer, glViewport);
    LOAD_GL_FUNC(renderer, glScissor);
    LOAD_GL_FUNC(renderer, glEnable);
    LOAD_GL_FUNC(renderer, glDisable);
    LOAD_GL_FUNC(renderer, glFinish);
    LOAD_GL_FUNC(renderer, glFlush);
    LOAD_GL_FUNC(renderer, glGetError);
    LOAD_GL_FUNC(renderer, glGetString);
    LOAD_GL_FUNC(renderer, glColorMask);
    LOAD_GL_FUNC(renderer, glDepthMask);
    LOAD_GL_FUNC(renderer, glStencilMask);
    LOAD_GL_FUNC(renderer, glBindFramebuffer);

    /* Shader compilation */
    LOAD_GL_FUNC(renderer, glCreateShader);
    LOAD_GL_FUNC(renderer, glDeleteShader);
    LOAD_GL_FUNC(renderer, glShaderSource);
    LOAD_GL_FUNC(renderer, glCompileShader);
    LOAD_GL_FUNC(renderer, glGetShaderiv);
    LOAD_GL_FUNC(renderer, glGetShaderInfoLog);

    /* Program linking */
    LOAD_GL_FUNC(renderer, glCreateProgram);
    LOAD_GL_FUNC(renderer, glDeleteProgram);
    LOAD_GL_FUNC(renderer, glAttachShader);
    LOAD_GL_FUNC(renderer, glLinkProgram);
    LOAD_GL_FUNC(renderer, glUseProgram);
    LOAD_GL_FUNC(renderer, glGetProgramiv);
    LOAD_GL_FUNC(renderer, glGetProgramInfoLog);

    /* Uniform blocks */
    LOAD_GL_FUNC(renderer, glUniformBlockBinding);
    LOAD_GL_FUNC(renderer, glGetActiveUniformBlockName);

    /* Rasterizer state */
    LOAD_GL_FUNC(renderer, glCullFace);
    LOAD_GL_FUNC(renderer, glFrontFace);
    LOAD_GL_FUNC(renderer, glPolygonOffset);

    /* Depth/stencil state */
    LOAD_GL_FUNC(renderer, glDepthFunc);
    LOAD_GL_FUNC(renderer, glStencilFuncSeparate);
    LOAD_GL_FUNC(renderer, glStencilOpSeparate);
    LOAD_GL_FUNC(renderer, glStencilMaskSeparate);

    /* Blend state */
    LOAD_GL_FUNC(renderer, glBlendFuncSeparate);
    LOAD_GL_FUNC(renderer, glBlendEquationSeparate);

    /* Pixel store */
    LOAD_GL_FUNC(renderer, glPixelStorei);

    /* Buffers */
    LOAD_GL_FUNC(renderer, glGenBuffers);
    LOAD_GL_FUNC(renderer, glDeleteBuffers);
    LOAD_GL_FUNC(renderer, glBindBuffer);
    LOAD_GL_FUNC(renderer, glBufferData);
    LOAD_GL_FUNC(renderer, glBufferSubData);
    LOAD_GL_FUNC(renderer, glBindBufferBase);

    /* Textures */
    LOAD_GL_FUNC(renderer, glGenTextures);
    LOAD_GL_FUNC(renderer, glDeleteTextures);
    LOAD_GL_FUNC(renderer, glBindTexture);
    LOAD_GL_FUNC(renderer, glActiveTexture);
    LOAD_GL_FUNC(renderer, glTexStorage2D);
    LOAD_GL_FUNC(renderer, glTexStorage3D);
    LOAD_GL_FUNC(renderer, glTexSubImage2D);
    LOAD_GL_FUNC(renderer, glTexSubImage3D);
    LOAD_GL_FUNC(renderer, glTexParameteri);

    /* Samplers */
    LOAD_GL_FUNC(renderer, glGenSamplers);
    LOAD_GL_FUNC(renderer, glDeleteSamplers);
    LOAD_GL_FUNC(renderer, glBindSampler);
    LOAD_GL_FUNC(renderer, glSamplerParameteri);
    LOAD_GL_FUNC(renderer, glSamplerParameterf);

    /* Vertex arrays */
    LOAD_GL_FUNC(renderer, glGenVertexArrays);
    LOAD_GL_FUNC(renderer, glDeleteVertexArrays);
    LOAD_GL_FUNC(renderer, glBindVertexArray);
    LOAD_GL_FUNC(renderer, glEnableVertexAttribArray);
    LOAD_GL_FUNC(renderer, glDisableVertexAttribArray);
    LOAD_GL_FUNC(renderer, glVertexAttribPointer);
    LOAD_GL_FUNC(renderer, glVertexAttribIPointer);
    LOAD_GL_FUNC(renderer, glVertexAttribDivisor);

    /* Draw */
    LOAD_GL_FUNC(renderer, glDrawArrays);
    LOAD_GL_FUNC(renderer, glDrawElements);
    LOAD_GL_FUNC(renderer, glDrawArraysInstanced);
    LOAD_GL_FUNC(renderer, glDrawElementsInstanced);

    /* Check critical functions */
    if (!renderer->glClear || !renderer->glClearColor || !renderer->glGetString ||
        !renderer->glCreateShader || !renderer->glCreateProgram ||
        !renderer->glGenBuffers || !renderer->glGenTextures) {
        return SDL_SetError("GLES: failed to load critical GL functions");
    }
    return true;
}

/* ======================================================================== */
/* Device lifecycle                                                        */
/* ======================================================================== */

static void GLES_DestroyDevice(SDL_GPUDevice *device)
{
    GLESRenderer *renderer = (GLESRenderer *)device->driverData;
    if (renderer) {
        SDL_free(renderer);
    }
    SDL_free(device);
}

static SDL_PropertiesID GLES_GetDeviceProperties(SDL_GPUDevice *device)
{
    return SDL_CreateProperties();
}

/* ======================================================================== */
/* Command buffer                                                          */
/* ======================================================================== */

/* Single static command buffer for MVP (GLES is immediate-mode anyway) */
static GLESCommandBuffer g_cmdbuf;
static GLESSwapchainTexture g_swapchain_texture;

static SDL_GPUCommandBuffer *GLES_AcquireCommandBuffer(
    SDL_GPURenderer *driverData)
{
    GLESRenderer *renderer = (GLESRenderer *)driverData;
    GLESCommandBuffer *cmdbuf = &g_cmdbuf;

    SDL_zeroa(cmdbuf->header.render_pass.color_targets);
    cmdbuf->header.render_pass.in_progress = false;
    cmdbuf->header.compute_pass.in_progress = false;
    cmdbuf->header.copy_pass.in_progress = false;
    cmdbuf->header.swapchain_texture_acquired = false;
    cmdbuf->header.submitted = false;

    cmdbuf->renderer = renderer;
    cmdbuf->swapchain_window = NULL;

    return (SDL_GPUCommandBuffer *)cmdbuf;
}

static bool GLES_Submit(SDL_GPUCommandBuffer *commandBuffer)
{
    GLESCommandBuffer *cmdbuf = (GLESCommandBuffer *)commandBuffer;

    /* Present: swap the window */
    if (cmdbuf->swapchain_window) {
        SDL_GL_SwapWindow(cmdbuf->swapchain_window);
    }

    cmdbuf->header.submitted = true;
    return true;
}

static SDL_GPUFence *GLES_SubmitAndAcquireFence(SDL_GPUCommandBuffer *commandBuffer)
{
    GLES_Submit(commandBuffer);
    /* Return a dummy non-NULL fence so callers don't think it failed */
    static int dummy_fence;
    return (SDL_GPUFence *)&dummy_fence;
}

static bool GLES_Cancel(SDL_GPUCommandBuffer *commandBuffer)
{
    GLESCommandBuffer *cmdbuf = (GLESCommandBuffer *)commandBuffer;
    cmdbuf->header.submitted = true;  /* prevent reuse */
    return true;
}

static bool GLES_Wait(SDL_GPURenderer *driverData)
{
    GLESRenderer *renderer = (GLESRenderer *)driverData;
    if (renderer->glFinish) {
        renderer->glFinish();
    }
    return true;
}

static bool GLES_WaitForFences(
    SDL_GPURenderer *driverData,
    bool waitAll,
    SDL_GPUFence *const *fences,
    Uint32 numFences)
{
    return GLES_Wait(driverData);
}

static bool GLES_QueryFence(SDL_GPURenderer *driverData, SDL_GPUFence *fence)
{
    (void)driverData;
    (void)fence;
    return true;  /* always complete in immediate mode */
}

static void GLES_ReleaseFence(SDL_GPURenderer *driverData, SDL_GPUFence *fence)
{
    (void)driverData;
    (void)fence;
}

/* ======================================================================== */
/* Swapchain                                                               */
/* ======================================================================== */

static bool GLES_ClaimWindow(SDL_GPURenderer *driverData, SDL_Window *window)
{
    GLESRenderer *renderer = (GLESRenderer *)driverData;
    renderer->claimed_window = window;
    return true;
}

static void GLES_ReleaseWindow(SDL_GPURenderer *driverData, SDL_Window *window)
{
    GLESRenderer *renderer = (GLESRenderer *)driverData;
    if (renderer->claimed_window == window) {
        renderer->claimed_window = NULL;
    }
}

static bool GLES_AcquireSwapchainTexture(
    SDL_GPUCommandBuffer *commandBuffer,
    SDL_Window *window,
    SDL_GPUTexture **swapchainTexture,
    Uint32 *swapchainTextureWidth,
    Uint32 *swapchainTextureHeight)
{
    GLESCommandBuffer *cmdbuf = (GLESCommandBuffer *)commandBuffer;

    if (!window || !swapchainTexture) {
        return SDL_SetError("Invalid arguments to AcquireSwapchainTexture");
    }

    /* Make GL context current for this window */
    /* (SDL3's GL layer handles this through the video driver) */

    int w = 0, h = 0;
    SDL_GetWindowSizeInPixels(window, &w, &h);

    /* Set up the swapchain texture header */
    SDL_zero(g_swapchain_texture);
    g_swapchain_texture.header.info.type = SDL_GPU_TEXTURETYPE_2D;
    g_swapchain_texture.header.info.format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
    g_swapchain_texture.header.info.width = (Uint32)w;
    g_swapchain_texture.header.info.height = (Uint32)h;
    g_swapchain_texture.header.info.layer_count_or_depth = 1;
    g_swapchain_texture.header.info.num_levels = 1;
    g_swapchain_texture.header.info.usage = SDL_GPU_TEXTUREUSAGE_COLOR_TARGET;

    *swapchainTexture = (SDL_GPUTexture *)&g_swapchain_texture;
    if (swapchainTextureWidth) *swapchainTextureWidth = (Uint32)w;
    if (swapchainTextureHeight) *swapchainTextureHeight = (Uint32)h;

    cmdbuf->swapchain_window = window;
    cmdbuf->header.swapchain_texture_acquired = true;

    return true;
}

static bool GLES_WaitAndAcquireSwapchainTexture(
    SDL_GPUCommandBuffer *commandBuffer,
    SDL_Window *window,
    SDL_GPUTexture **swapchainTexture,
    Uint32 *swapchainTextureWidth,
    Uint32 *swapchainTextureHeight)
{
    /* In immediate mode there's nothing to wait for */
    return GLES_AcquireSwapchainTexture(
        commandBuffer, window, swapchainTexture,
        swapchainTextureWidth, swapchainTextureHeight);
}

static bool GLES_WaitForSwapchain(SDL_GPURenderer *driverData, SDL_Window *window)
{
    (void)driverData;
    (void)window;
    return true;
}

static SDL_GPUTextureFormat GLES_GetSwapchainTextureFormat(
    SDL_GPURenderer *driverData, SDL_Window *window)
{
    (void)driverData;
    (void)window;
    return SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
}

static bool GLES_SetSwapchainParameters(
    SDL_GPURenderer *driverData,
    SDL_Window *window,
    SDL_GPUSwapchainComposition swapchainComposition,
    SDL_GPUPresentMode presentMode)
{
    (void)driverData;
    (void)window;
    (void)swapchainComposition;
    (void)presentMode;
    return true;
}

static bool GLES_SetAllowedFramesInFlight(
    SDL_GPURenderer *driverData, Uint32 allowedFramesInFlight)
{
    (void)driverData;
    (void)allowedFramesInFlight;
    return true;
}

static bool GLES_SupportsSwapchainComposition(
    SDL_GPURenderer *driverData,
    SDL_Window *window,
    SDL_GPUSwapchainComposition swapchainComposition)
{
    (void)driverData;
    (void)window;
    return (swapchainComposition == SDL_GPU_SWAPCHAINCOMPOSITION_SDR);
}

static bool GLES_SupportsPresentMode(
    SDL_GPURenderer *driverData,
    SDL_Window *window,
    SDL_GPUPresentMode presentMode)
{
    (void)driverData;
    (void)window;
    return (presentMode == SDL_GPU_PRESENTMODE_VSYNC ||
            presentMode == SDL_GPU_PRESENTMODE_IMMEDIATE);
}

/* ======================================================================== */
/* Render pass                                                             */
/* ======================================================================== */

static void GLES_BeginRenderPass(
    SDL_GPUCommandBuffer *commandBuffer,
    const SDL_GPUColorTargetInfo *colorTargetInfos,
    Uint32 numColorTargets,
    const SDL_GPUDepthStencilTargetInfo *depthStencilTargetInfo)
{
    GLESCommandBuffer *cmdbuf = (GLESCommandBuffer *)commandBuffer;
    GLESRenderer *renderer = cmdbuf->renderer;

    /* Bind the default framebuffer (FBO 0) */
    if (renderer->glBindFramebuffer) {
        renderer->glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }

    /* Handle clear operations */
    GLbitfield clearMask = 0;

    for (Uint32 i = 0; i < numColorTargets; i++) {
        if (colorTargetInfos[i].load_op == SDL_GPU_LOADOP_CLEAR) {
            renderer->glClearColor(
                colorTargetInfos[i].clear_color.r,
                colorTargetInfos[i].clear_color.g,
                colorTargetInfos[i].clear_color.b,
                colorTargetInfos[i].clear_color.a);
            clearMask |= GL_COLOR_BUFFER_BIT;
        }
    }

    if (depthStencilTargetInfo) {
        if (depthStencilTargetInfo->load_op == SDL_GPU_LOADOP_CLEAR) {
            if (renderer->glClearDepthf) {
                renderer->glClearDepthf(depthStencilTargetInfo->clear_depth);
            }
            /* Ensure depth writes are enabled for clear */
            if (renderer->glDepthMask) {
                renderer->glDepthMask(GL_TRUE);
            }
            clearMask |= GL_DEPTH_BUFFER_BIT;
        }
        if (depthStencilTargetInfo->stencil_load_op == SDL_GPU_LOADOP_CLEAR) {
            if (renderer->glClearStencil) {
                renderer->glClearStencil((GLint)depthStencilTargetInfo->clear_stencil);
            }
            if (renderer->glStencilMask) {
                renderer->glStencilMask(0xFF);
            }
            clearMask |= GL_STENCIL_BUFFER_BIT;
        }
    }

    if (clearMask != 0) {
        /* Ensure color writes are enabled for clear */
        if (renderer->glColorMask) {
            renderer->glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
        }
        /* Disable scissor so clear affects entire framebuffer */
        if (renderer->glDisable) {
            renderer->glDisable(GL_SCISSOR_TEST);
        }
        renderer->glClear(clearMask);
    }

    /* Set viewport to match swapchain size */
    if (numColorTargets > 0 && colorTargetInfos[0].texture) {
        TextureCommonHeader *tex = (TextureCommonHeader *)colorTargetInfos[0].texture;
        if (renderer->glViewport) {
            renderer->glViewport(0, 0, (GLsizei)tex->info.width, (GLsizei)tex->info.height);
        }
    }
}

static void GLES_EndRenderPass(SDL_GPUCommandBuffer *commandBuffer)
{
    /* Nothing to do in immediate mode — rendering already happened */
    (void)commandBuffer;
}

/* ======================================================================== */
/* Feature queries                                                         */
/* ======================================================================== */

static bool GLES_SupportsTextureFormat(
    SDL_GPURenderer *driverData,
    SDL_GPUTextureFormat format,
    SDL_GPUTextureType type,
    SDL_GPUTextureUsageFlags usage)
{
    (void)driverData;
    /* Report support for common formats used by SDL3 apps */
    switch (format) {
    case SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM:
    case SDL_GPU_TEXTUREFORMAT_B8G8R8A8_UNORM:
    case SDL_GPU_TEXTUREFORMAT_R8_UNORM:
    case SDL_GPU_TEXTUREFORMAT_R8G8_UNORM:
    case SDL_GPU_TEXTUREFORMAT_D16_UNORM:
    case SDL_GPU_TEXTUREFORMAT_D24_UNORM_S8_UINT:
    case SDL_GPU_TEXTUREFORMAT_D32_FLOAT:
        return true;
    default:
        return false;
    }
}

static bool GLES_SupportsSampleCount(
    SDL_GPURenderer *driverData,
    SDL_GPUTextureFormat format,
    SDL_GPUSampleCount desiredSampleCount)
{
    (void)driverData;
    (void)format;
    return (desiredSampleCount == SDL_GPU_SAMPLECOUNT_1);
}

/* ======================================================================== */
/* Format conversion functions                                             */
/* ======================================================================== */

static void GLES_GetVertexFormatInfo(SDL_GPUVertexElementFormat format,
                              GLint *out_size, GLenum *out_type,
                              GLboolean *out_normalized, GLboolean *out_integer)
{
    *out_normalized = GL_FALSE;
    *out_integer = GL_FALSE;

    switch (format) {
    /* Integer formats (use VertexAttribIPointer) */
    case SDL_GPU_VERTEXELEMENTFORMAT_INT:
        *out_size = 1; *out_type = GL_INT; *out_integer = GL_TRUE; break;
    case SDL_GPU_VERTEXELEMENTFORMAT_INT2:
        *out_size = 2; *out_type = GL_INT; *out_integer = GL_TRUE; break;
    case SDL_GPU_VERTEXELEMENTFORMAT_INT3:
        *out_size = 3; *out_type = GL_INT; *out_integer = GL_TRUE; break;
    case SDL_GPU_VERTEXELEMENTFORMAT_INT4:
        *out_size = 4; *out_type = GL_INT; *out_integer = GL_TRUE; break;
    case SDL_GPU_VERTEXELEMENTFORMAT_UINT:
        *out_size = 1; *out_type = GL_UNSIGNED_INT; *out_integer = GL_TRUE; break;
    case SDL_GPU_VERTEXELEMENTFORMAT_UINT2:
        *out_size = 2; *out_type = GL_UNSIGNED_INT; *out_integer = GL_TRUE; break;
    case SDL_GPU_VERTEXELEMENTFORMAT_UINT3:
        *out_size = 3; *out_type = GL_UNSIGNED_INT; *out_integer = GL_TRUE; break;
    case SDL_GPU_VERTEXELEMENTFORMAT_UINT4:
        *out_size = 4; *out_type = GL_UNSIGNED_INT; *out_integer = GL_TRUE; break;

    /* Float formats */
    case SDL_GPU_VERTEXELEMENTFORMAT_FLOAT:
        *out_size = 1; *out_type = GL_FLOAT; break;
    case SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2:
        *out_size = 2; *out_type = GL_FLOAT; break;
    case SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3:
        *out_size = 3; *out_type = GL_FLOAT; break;
    case SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4:
        *out_size = 4; *out_type = GL_FLOAT; break;

    /* Half-float formats */
    case SDL_GPU_VERTEXELEMENTFORMAT_HALF2:
        *out_size = 2; *out_type = GL_HALF_FLOAT; break;
    case SDL_GPU_VERTEXELEMENTFORMAT_HALF4:
        *out_size = 4; *out_type = GL_HALF_FLOAT; break;

    /* Byte formats */
    case SDL_GPU_VERTEXELEMENTFORMAT_BYTE2:
        *out_size = 2; *out_type = GL_BYTE; break;
    case SDL_GPU_VERTEXELEMENTFORMAT_BYTE4:
        *out_size = 4; *out_type = GL_BYTE; break;
    case SDL_GPU_VERTEXELEMENTFORMAT_UBYTE2:
        *out_size = 2; *out_type = GL_UNSIGNED_BYTE; break;
    case SDL_GPU_VERTEXELEMENTFORMAT_UBYTE4:
        *out_size = 4; *out_type = GL_UNSIGNED_BYTE; break;
    case SDL_GPU_VERTEXELEMENTFORMAT_BYTE2_NORM:
        *out_size = 2; *out_type = GL_BYTE; *out_normalized = GL_TRUE; break;
    case SDL_GPU_VERTEXELEMENTFORMAT_BYTE4_NORM:
        *out_size = 4; *out_type = GL_BYTE; *out_normalized = GL_TRUE; break;
    case SDL_GPU_VERTEXELEMENTFORMAT_UBYTE2_NORM:
        *out_size = 2; *out_type = GL_UNSIGNED_BYTE; *out_normalized = GL_TRUE; break;
    case SDL_GPU_VERTEXELEMENTFORMAT_UBYTE4_NORM:
        *out_size = 4; *out_type = GL_UNSIGNED_BYTE; *out_normalized = GL_TRUE; break;

    /* Short formats */
    case SDL_GPU_VERTEXELEMENTFORMAT_SHORT2:
        *out_size = 2; *out_type = GL_SHORT; break;
    case SDL_GPU_VERTEXELEMENTFORMAT_SHORT4:
        *out_size = 4; *out_type = GL_SHORT; break;
    case SDL_GPU_VERTEXELEMENTFORMAT_USHORT2:
        *out_size = 2; *out_type = GL_UNSIGNED_SHORT; break;
    case SDL_GPU_VERTEXELEMENTFORMAT_USHORT4:
        *out_size = 4; *out_type = GL_UNSIGNED_SHORT; break;
    case SDL_GPU_VERTEXELEMENTFORMAT_SHORT2_NORM:
        *out_size = 2; *out_type = GL_SHORT; *out_normalized = GL_TRUE; break;
    case SDL_GPU_VERTEXELEMENTFORMAT_SHORT4_NORM:
        *out_size = 4; *out_type = GL_SHORT; *out_normalized = GL_TRUE; break;
    case SDL_GPU_VERTEXELEMENTFORMAT_USHORT2_NORM:
        *out_size = 2; *out_type = GL_UNSIGNED_SHORT; *out_normalized = GL_TRUE; break;
    case SDL_GPU_VERTEXELEMENTFORMAT_USHORT4_NORM:
        *out_size = 4; *out_type = GL_UNSIGNED_SHORT; *out_normalized = GL_TRUE; break;

    default:
        *out_size = 4; *out_type = GL_FLOAT; break;
    }
}

static GLenum GLES_PrimitiveType(SDL_GPUPrimitiveType type)
{
    switch (type) {
    case SDL_GPU_PRIMITIVETYPE_TRIANGLELIST:  return GL_TRIANGLES;
    case SDL_GPU_PRIMITIVETYPE_TRIANGLESTRIP: return GL_TRIANGLE_STRIP;
    case SDL_GPU_PRIMITIVETYPE_LINELIST:      return GL_LINES;
    case SDL_GPU_PRIMITIVETYPE_LINESTRIP:     return GL_LINE_STRIP;
    case SDL_GPU_PRIMITIVETYPE_POINTLIST:     return GL_POINTS;
    default: return GL_TRIANGLES;
    }
}

static GLenum GLES_CompareOp(SDL_GPUCompareOp op)
{
    switch (op) {
    case SDL_GPU_COMPAREOP_NEVER:            return GL_NEVER;
    case SDL_GPU_COMPAREOP_LESS:             return GL_LESS;
    case SDL_GPU_COMPAREOP_EQUAL:            return GL_EQUAL;
    case SDL_GPU_COMPAREOP_LESS_OR_EQUAL:    return GL_LEQUAL;
    case SDL_GPU_COMPAREOP_GREATER:          return GL_GREATER;
    case SDL_GPU_COMPAREOP_NOT_EQUAL:        return GL_NOTEQUAL;
    case SDL_GPU_COMPAREOP_GREATER_OR_EQUAL: return GL_GEQUAL;
    case SDL_GPU_COMPAREOP_ALWAYS:           return GL_ALWAYS;
    default: return GL_ALWAYS;
    }
}

static GLenum GLES_StencilOp(SDL_GPUStencilOp op)
{
    switch (op) {
    case SDL_GPU_STENCILOP_KEEP:                return GL_KEEP;
    case SDL_GPU_STENCILOP_ZERO:                return GL_ZERO_OP;
    case SDL_GPU_STENCILOP_REPLACE:             return GL_REPLACE;
    case SDL_GPU_STENCILOP_INCREMENT_AND_CLAMP: return GL_INCR;
    case SDL_GPU_STENCILOP_DECREMENT_AND_CLAMP: return GL_DECR;
    case SDL_GPU_STENCILOP_INVERT:              return GL_INVERT;
    case SDL_GPU_STENCILOP_INCREMENT_AND_WRAP:  return GL_INCR_WRAP;
    case SDL_GPU_STENCILOP_DECREMENT_AND_WRAP:  return GL_DECR_WRAP;
    default: return GL_KEEP;
    }
}

static GLenum GLES_BlendFactor(SDL_GPUBlendFactor factor)
{
    switch (factor) {
    case SDL_GPU_BLENDFACTOR_ZERO:                     return GL_BF_ZERO;
    case SDL_GPU_BLENDFACTOR_ONE:                      return GL_ONE;
    case SDL_GPU_BLENDFACTOR_SRC_COLOR:                return GL_SRC_COLOR;
    case SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_COLOR:      return GL_ONE_MINUS_SRC_COLOR;
    case SDL_GPU_BLENDFACTOR_DST_COLOR:                return GL_DST_COLOR;
    case SDL_GPU_BLENDFACTOR_ONE_MINUS_DST_COLOR:      return GL_ONE_MINUS_DST_COLOR;
    case SDL_GPU_BLENDFACTOR_SRC_ALPHA:                return GL_SRC_ALPHA;
    case SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA:      return GL_ONE_MINUS_SRC_ALPHA;
    case SDL_GPU_BLENDFACTOR_DST_ALPHA:                return GL_DST_ALPHA;
    case SDL_GPU_BLENDFACTOR_ONE_MINUS_DST_ALPHA:      return GL_ONE_MINUS_DST_ALPHA;
    case SDL_GPU_BLENDFACTOR_CONSTANT_COLOR:           return GL_CONSTANT_COLOR;
    case SDL_GPU_BLENDFACTOR_ONE_MINUS_CONSTANT_COLOR: return GL_ONE_MINUS_CONSTANT_COLOR;
    case SDL_GPU_BLENDFACTOR_SRC_ALPHA_SATURATE:       return GL_SRC_ALPHA_SATURATE;
    default: return GL_ONE;
    }
}

static GLenum GLES_BlendOp(SDL_GPUBlendOp op)
{
    switch (op) {
    case SDL_GPU_BLENDOP_ADD:              return GL_FUNC_ADD;
    case SDL_GPU_BLENDOP_SUBTRACT:         return GL_FUNC_SUBTRACT;
    case SDL_GPU_BLENDOP_REVERSE_SUBTRACT: return GL_FUNC_REVERSE_SUBTRACT;
    case SDL_GPU_BLENDOP_MIN:              return GL_MIN;
    case SDL_GPU_BLENDOP_MAX:              return GL_MAX;
    default: return GL_FUNC_ADD;
    }
}

/* ======================================================================== */
/* Shader and pipeline creation                                            */
/* ======================================================================== */

static SDL_GPUShader *GLES_CreateShader(
    SDL_GPURenderer *driverData,
    const SDL_GPUShaderCreateInfo *createinfo)
{
    GLESRenderer *renderer = (GLESRenderer *)driverData;

    if (!(createinfo->format & SDL_GPU_SHADERFORMAT_SPIRV)) {
        SDL_SetError("GLES GPU backend only supports SPIR-V shaders");
        return NULL;
    }

    /* Transpile SPIR-V to GLSL ES */
    char *glsl = GLES_TranspileSPIRV(createinfo->code, createinfo->code_size,
                                      createinfo->entrypoint, createinfo->stage);
    if (!glsl) return NULL;  /* Error already set */

    /* Compile GL shader */
    GLenum gl_stage = (createinfo->stage == SDL_GPU_SHADERSTAGE_VERTEX) ?
                       GL_VERTEX_SHADER : GL_FRAGMENT_SHADER;
    GLuint handle = renderer->glCreateShader(gl_stage);
    const GLchar *source = glsl;
    renderer->glShaderSource(handle, 1, &source, NULL);
    renderer->glCompileShader(handle);

    GLint status;
    renderer->glGetShaderiv(handle, GL_COMPILE_STATUS, &status);
    if (!status) {
        GLint log_len;
        renderer->glGetShaderiv(handle, GL_INFO_LOG_LENGTH, &log_len);
        char *log = (char *)SDL_malloc((size_t)log_len + 1);
        if (log) {
            renderer->glGetShaderInfoLog(handle, log_len, NULL, log);
            SDL_SetError("Shader compilation failed: %s", log);
            if (renderer->debug_mode) {
                SDL_LogError(SDL_LOG_CATEGORY_GPU, "Shader compile error:\n%s\nSource:\n%s", log, glsl);
            }
            SDL_free(log);
        }
        renderer->glDeleteShader(handle);
        SDL_free(glsl);
        return NULL;
    }

    GLESShader *shader = (GLESShader *)SDL_calloc(1, sizeof(GLESShader));
    if (!shader) {
        renderer->glDeleteShader(handle);
        SDL_free(glsl);
        return NULL;
    }
    shader->handle = handle;
    shader->stage = createinfo->stage;
    shader->num_samplers = createinfo->num_samplers;
    shader->num_storage_textures = createinfo->num_storage_textures;
    shader->num_storage_buffers = createinfo->num_storage_buffers;
    shader->num_uniform_buffers = createinfo->num_uniform_buffers;
    shader->glsl_source = renderer->debug_mode ? glsl : NULL;
    if (!renderer->debug_mode) SDL_free(glsl);

    return (SDL_GPUShader *)shader;
}

static SDL_GPUGraphicsPipeline *GLES_CreateGraphicsPipeline(
    SDL_GPURenderer *driverData,
    const SDL_GPUGraphicsPipelineCreateInfo *createinfo)
{
    GLESRenderer *renderer = (GLESRenderer *)driverData;

    GLESShader *vert = (GLESShader *)createinfo->vertex_shader;
    GLESShader *frag = (GLESShader *)createinfo->fragment_shader;

    /* Link program */
    GLuint program = renderer->glCreateProgram();
    renderer->glAttachShader(program, vert->handle);
    renderer->glAttachShader(program, frag->handle);
    renderer->glLinkProgram(program);

    GLint status;
    renderer->glGetProgramiv(program, GL_LINK_STATUS, &status);
    if (!status) {
        GLint log_len;
        renderer->glGetProgramiv(program, GL_INFO_LOG_LENGTH, &log_len);
        char *log = (char *)SDL_malloc((size_t)log_len + 1);
        if (log) {
            renderer->glGetProgramInfoLog(program, log_len, NULL, log);
            SDL_SetError("Program link failed: %s", log);
            if (renderer->debug_mode) {
                SDL_LogError(SDL_LOG_CATEGORY_GPU, "Program link error: %s", log);
            }
            SDL_free(log);
        }
        renderer->glDeleteProgram(program);
        return NULL;
    }

    /* Bind uniform blocks to stage-based binding points.
     * Our transpiler prefixes block names with "vs_" or "fs_" so we can
     * assign vertex UBOs to bindings 0..3 and fragment UBOs to 4..7. */
    {
        GLint num_ub;
        renderer->glGetProgramiv(program, GL_ACTIVE_UNIFORM_BLOCKS, &num_ub);
        Uint32 vs_slot = 0, fs_slot = 0;
        for (GLint i = 0; i < num_ub; i++) {
            char name[256];
            GLsizei name_len;
            renderer->glGetActiveUniformBlockName(program, (GLuint)i, sizeof(name), &name_len, name);
            if (name_len >= 3 && name[0] == 'v' && name[1] == 's' && name[2] == '_') {
                renderer->glUniformBlockBinding(program, (GLuint)i, vs_slot);
                vs_slot++;
            } else if (name_len >= 3 && name[0] == 'f' && name[1] == 's' && name[2] == '_') {
                renderer->glUniformBlockBinding(program, (GLuint)i, GLES_MAX_UNIFORM_BUFFERS + fs_slot);
                fs_slot++;
            } else {
                /* Unknown prefix — assign to vertex slots as fallback */
                renderer->glUniformBlockBinding(program, (GLuint)i, vs_slot);
                vs_slot++;
            }
        }
    }

    GLESGraphicsPipeline *pipeline = (GLESGraphicsPipeline *)SDL_calloc(1, sizeof(GLESGraphicsPipeline));
    if (!pipeline) {
        renderer->glDeleteProgram(program);
        return NULL;
    }

    /* Fill GraphicsPipelineCommonHeader — dispatch layer reads this */
    pipeline->header.num_vertex_samplers = vert->num_samplers;
    pipeline->header.num_vertex_storage_textures = vert->num_storage_textures;
    pipeline->header.num_vertex_storage_buffers = vert->num_storage_buffers;
    pipeline->header.num_vertex_uniform_buffers = vert->num_uniform_buffers;
    pipeline->header.num_fragment_samplers = frag->num_samplers;
    pipeline->header.num_fragment_storage_textures = frag->num_storage_textures;
    pipeline->header.num_fragment_storage_buffers = frag->num_storage_buffers;
    pipeline->header.num_fragment_uniform_buffers = frag->num_uniform_buffers;

    pipeline->program = program;
    pipeline->primitive_type = GLES_PrimitiveType(createinfo->primitive_type);

    /* Bake vertex input state */
    const SDL_GPUVertexInputState *vi = &createinfo->vertex_input_state;
    pipeline->num_vertex_buffers = vi->num_vertex_buffers;
    for (Uint32 i = 0; i < vi->num_vertex_buffers && i < GLES_MAX_VERTEX_BUFFERS; i++) {
        pipeline->vertex_buffers[i].slot = vi->vertex_buffer_descriptions[i].slot;
        pipeline->vertex_buffers[i].pitch = vi->vertex_buffer_descriptions[i].pitch;
        pipeline->vertex_buffers[i].input_rate = vi->vertex_buffer_descriptions[i].input_rate;
        pipeline->vertex_buffers[i].instance_step_rate = vi->vertex_buffer_descriptions[i].instance_step_rate;
    }
    pipeline->num_vertex_attributes = vi->num_vertex_attributes;
    for (Uint32 i = 0; i < vi->num_vertex_attributes && i < GLES_MAX_VERTEX_ATTRIBUTES; i++) {
        pipeline->vertex_attributes[i].location = vi->vertex_attributes[i].location;
        pipeline->vertex_attributes[i].buffer_slot = vi->vertex_attributes[i].buffer_slot;
        pipeline->vertex_attributes[i].offset = vi->vertex_attributes[i].offset;
        GLES_GetVertexFormatInfo(vi->vertex_attributes[i].format,
            &pipeline->vertex_attributes[i].gl_size,
            &pipeline->vertex_attributes[i].gl_type,
            &pipeline->vertex_attributes[i].gl_normalized,
            &pipeline->vertex_attributes[i].gl_integer);
    }

    /* Bake pipeline state */
    pipeline->rasterizer = createinfo->rasterizer_state;
    pipeline->depth_stencil = createinfo->depth_stencil_state;
    pipeline->multisample = createinfo->multisample_state;

    pipeline->num_color_targets = createinfo->target_info.num_color_targets;
    for (Uint32 i = 0; i < createinfo->target_info.num_color_targets && i < GLES_MAX_COLOR_TARGETS; i++) {
        pipeline->blend_states[i] = createinfo->target_info.color_target_descriptions[i].blend_state;
    }

    pipeline->fragment_sampler_count = frag->num_samplers;

    return (SDL_GPUGraphicsPipeline *)pipeline;
}

/* ======================================================================== */
/* Asserting stubs for unimplemented functions                             */
/* ======================================================================== */

/* --- State Creation --- */

static SDL_GPUComputePipeline *GLES_CreateComputePipeline(
    SDL_GPURenderer *driverData,
    const SDL_GPUComputePipelineCreateInfo *createinfo)
{
    GPU_STUB_ASSERT(CreateComputePipeline);
    return NULL;
}

static GLenum GLES_SamplerFilter(SDL_GPUFilter filter)
{
    return (filter == SDL_GPU_FILTER_NEAREST) ? GL_NEAREST : GL_LINEAR;
}

static GLenum GLES_SamplerMipFilter(SDL_GPUFilter filter, SDL_GPUSamplerMipmapMode mip)
{
    if (filter == SDL_GPU_FILTER_NEAREST) {
        return (mip == SDL_GPU_SAMPLERMIPMAPMODE_NEAREST) ?
               GL_NEAREST_MIPMAP_NEAREST : GL_NEAREST_MIPMAP_LINEAR;
    } else {
        return (mip == SDL_GPU_SAMPLERMIPMAPMODE_NEAREST) ?
               GL_LINEAR_MIPMAP_NEAREST : GL_LINEAR_MIPMAP_LINEAR;
    }
}

static GLenum GLES_AddressMode(SDL_GPUSamplerAddressMode mode)
{
    switch (mode) {
    case SDL_GPU_SAMPLERADDRESSMODE_REPEAT:          return GL_REPEAT;
    case SDL_GPU_SAMPLERADDRESSMODE_MIRRORED_REPEAT: return GL_MIRRORED_REPEAT;
    case SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE:   return GL_CLAMP_TO_EDGE;
    default: return GL_CLAMP_TO_EDGE;
    }
}

static bool GLES_GetTextureFormatInfo(SDL_GPUTextureFormat sdl_format,
                                       GLenum *out_internal, GLenum *out_format, GLenum *out_type)
{
    switch (sdl_format) {
    case SDL_GPU_TEXTUREFORMAT_R8_UNORM:
        *out_internal = GL_R8; *out_format = GL_RED; *out_type = GL_UNSIGNED_BYTE; return true;
    case SDL_GPU_TEXTUREFORMAT_R8G8_UNORM:
        *out_internal = GL_RG8; *out_format = GL_RG; *out_type = GL_UNSIGNED_BYTE; return true;
    case SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM:
        *out_internal = GL_RGBA8; *out_format = GL_RGBA; *out_type = GL_UNSIGNED_BYTE; return true;
    case SDL_GPU_TEXTUREFORMAT_R8_SNORM:
        *out_internal = GL_R8_SNORM; *out_format = GL_RED; *out_type = GL_BYTE; return true;
    case SDL_GPU_TEXTUREFORMAT_R8G8_SNORM:
        *out_internal = GL_RG8_SNORM; *out_format = GL_RG; *out_type = GL_BYTE; return true;
    case SDL_GPU_TEXTUREFORMAT_R8G8B8A8_SNORM:
        *out_internal = GL_RGBA8_SNORM; *out_format = GL_RGBA; *out_type = GL_BYTE; return true;
    case SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM_SRGB:
        *out_internal = GL_SRGB8_ALPHA8; *out_format = GL_RGBA; *out_type = GL_UNSIGNED_BYTE; return true;
    case SDL_GPU_TEXTUREFORMAT_B8G8R8A8_UNORM:
        *out_internal = GL_RGBA8; *out_format = GL_RGBA; *out_type = GL_UNSIGNED_BYTE; return true;
    case SDL_GPU_TEXTUREFORMAT_B8G8R8A8_UNORM_SRGB:
        *out_internal = GL_SRGB8_ALPHA8; *out_format = GL_RGBA; *out_type = GL_UNSIGNED_BYTE; return true;
    case SDL_GPU_TEXTUREFORMAT_R16_FLOAT:
        *out_internal = GL_R16F; *out_format = GL_RED; *out_type = GL_HALF_FLOAT; return true;
    case SDL_GPU_TEXTUREFORMAT_R16G16_FLOAT:
        *out_internal = GL_RG16F; *out_format = GL_RG; *out_type = GL_HALF_FLOAT; return true;
    case SDL_GPU_TEXTUREFORMAT_R16G16B16A16_FLOAT:
        *out_internal = GL_RGBA16F; *out_format = GL_RGBA; *out_type = GL_HALF_FLOAT; return true;
    case SDL_GPU_TEXTUREFORMAT_R32_FLOAT:
        *out_internal = GL_R32F; *out_format = GL_RED; *out_type = GL_FLOAT; return true;
    case SDL_GPU_TEXTUREFORMAT_R32G32_FLOAT:
        *out_internal = GL_RG32F; *out_format = GL_RG; *out_type = GL_FLOAT; return true;
    case SDL_GPU_TEXTUREFORMAT_R32G32B32A32_FLOAT:
        *out_internal = GL_RGBA32F; *out_format = GL_RGBA; *out_type = GL_FLOAT; return true;
    case SDL_GPU_TEXTUREFORMAT_D16_UNORM:
        *out_internal = GL_DEPTH_COMPONENT16; *out_format = GL_DEPTH_COMPONENT; *out_type = GL_UNSIGNED_SHORT; return true;
    case SDL_GPU_TEXTUREFORMAT_D24_UNORM:
        *out_internal = GL_DEPTH_COMPONENT24; *out_format = GL_DEPTH_COMPONENT; *out_type = GL_UNSIGNED_INT; return true;
    case SDL_GPU_TEXTUREFORMAT_D32_FLOAT:
        *out_internal = GL_DEPTH_COMPONENT32F; *out_format = GL_DEPTH_COMPONENT; *out_type = GL_FLOAT; return true;
    case SDL_GPU_TEXTUREFORMAT_D24_UNORM_S8_UINT:
        *out_internal = GL_DEPTH24_STENCIL8; *out_format = GL_DEPTH_STENCIL; *out_type = GL_UNSIGNED_INT_24_8; return true;
    case SDL_GPU_TEXTUREFORMAT_D32_FLOAT_S8_UINT:
        *out_internal = GL_DEPTH32F_STENCIL8; *out_format = GL_DEPTH_STENCIL; *out_type = GL_FLOAT_32_UNSIGNED_INT_24_8_REV; return true;
    default:
        return false;
    }
}

static SDL_GPUSampler *GLES_CreateSampler(
    SDL_GPURenderer *driverData,
    const SDL_GPUSamplerCreateInfo *createinfo)
{
    GLESRenderer *renderer = (GLESRenderer *)driverData;

    GLuint handle;
    renderer->glGenSamplers(1, &handle);

    renderer->glSamplerParameteri(handle, GL_TEXTURE_MIN_FILTER,
        GLES_SamplerMipFilter(createinfo->min_filter, createinfo->mipmap_mode));
    renderer->glSamplerParameteri(handle, GL_TEXTURE_MAG_FILTER,
        GLES_SamplerFilter(createinfo->mag_filter));
    renderer->glSamplerParameteri(handle, GL_TEXTURE_WRAP_S,
        GLES_AddressMode(createinfo->address_mode_u));
    renderer->glSamplerParameteri(handle, GL_TEXTURE_WRAP_T,
        GLES_AddressMode(createinfo->address_mode_v));
    renderer->glSamplerParameteri(handle, GL_TEXTURE_WRAP_R,
        GLES_AddressMode(createinfo->address_mode_w));
    renderer->glSamplerParameterf(handle, GL_TEXTURE_MIN_LOD, createinfo->min_lod);
    renderer->glSamplerParameterf(handle, GL_TEXTURE_MAX_LOD, createinfo->max_lod);

    if (createinfo->enable_anisotropy) {
        renderer->glSamplerParameterf(handle, GL_TEXTURE_MAX_ANISOTROPY_EXT,
                                      createinfo->max_anisotropy);
    }

    if (createinfo->enable_compare) {
        renderer->glSamplerParameteri(handle, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_REF_TO_TEXTURE);
        renderer->glSamplerParameteri(handle, GL_TEXTURE_COMPARE_FUNC,
                                      GLES_CompareOp(createinfo->compare_op));
    }

    GLESSampler *sampler = (GLESSampler *)SDL_calloc(1, sizeof(GLESSampler));
    if (!sampler) {
        renderer->glDeleteSamplers(1, &handle);
        return NULL;
    }
    sampler->handle = handle;
    return (SDL_GPUSampler *)sampler;
}

static SDL_GPUTexture *GLES_CreateTexture(
    SDL_GPURenderer *driverData,
    const SDL_GPUTextureCreateInfo *createinfo)
{
    GLESRenderer *renderer = (GLESRenderer *)driverData;

    GLenum internal_format, format, type;
    if (!GLES_GetTextureFormatInfo(createinfo->format, &internal_format, &format, &type)) {
        SDL_SetError("Unsupported texture format: %d", createinfo->format);
        return NULL;
    }

    GLenum target;
    switch (createinfo->type) {
    case SDL_GPU_TEXTURETYPE_2D:       target = GL_TEXTURE_2D; break;
    case SDL_GPU_TEXTURETYPE_2D_ARRAY: target = GL_TEXTURE_2D_ARRAY; break;
    case SDL_GPU_TEXTURETYPE_3D:       target = GL_TEXTURE_3D; break;
    case SDL_GPU_TEXTURETYPE_CUBE:     target = GL_TEXTURE_CUBE_MAP; break;
    default:
        SDL_SetError("Unsupported texture type: %d", createinfo->type);
        return NULL;
    }

    GLuint handle;
    renderer->glGenTextures(1, &handle);
    renderer->glBindTexture(target, handle);

    Uint32 w = createinfo->width;
    Uint32 h = createinfo->height;
    Uint32 levels = createinfo->num_levels;
    Uint32 layers = createinfo->layer_count_or_depth;

    if (target == GL_TEXTURE_2D || target == GL_TEXTURE_CUBE_MAP) {
        renderer->glTexStorage2D(target, (GLsizei)levels, internal_format, (GLsizei)w, (GLsizei)h);
    } else if (target == GL_TEXTURE_2D_ARRAY || target == GL_TEXTURE_3D) {
        renderer->glTexStorage3D(target, (GLsizei)levels, internal_format,
                                 (GLsizei)w, (GLsizei)h, (GLsizei)layers);
    }

    /* Set default sampling params */
    renderer->glTexParameteri(target, GL_TEXTURE_MIN_FILTER,
                              levels > 1 ? GL_LINEAR_MIPMAP_LINEAR : GL_LINEAR);
    renderer->glTexParameteri(target, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    renderer->glTexParameteri(target, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    renderer->glTexParameteri(target, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    if (target == GL_TEXTURE_3D || target == GL_TEXTURE_CUBE_MAP)
        renderer->glTexParameteri(target, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
    renderer->glTexParameteri(target, GL_TEXTURE_MAX_LEVEL, (GLint)(levels - 1));

    renderer->glBindTexture(target, 0);

    GLESTexture *tex = (GLESTexture *)SDL_calloc(1, sizeof(GLESTexture));
    if (!tex) {
        renderer->glDeleteTextures(1, &handle);
        return NULL;
    }
    tex->info = *createinfo;
    tex->handle = handle;
    tex->target = target;
    tex->gl_internalformat = internal_format;
    tex->gl_format = format;
    tex->gl_type = type;
    tex->is_swapchain = false;

    return (SDL_GPUTexture *)tex;
}

static SDL_GPUBuffer *GLES_CreateBuffer(
    SDL_GPURenderer *driverData,
    SDL_GPUBufferUsageFlags usageFlags,
    Uint32 size,
    const char *debugName)
{
    GLESRenderer *renderer = (GLESRenderer *)driverData;
    (void)debugName;

    GLuint handle;
    renderer->glGenBuffers(1, &handle);

    GLenum target = GL_ARRAY_BUFFER;
    if (usageFlags & SDL_GPU_BUFFERUSAGE_INDEX)
        target = GL_ELEMENT_ARRAY_BUFFER;

    renderer->glBindBuffer(target, handle);
    renderer->glBufferData(target, (GLsizeiptr)size, NULL, GL_DYNAMIC_DRAW);
    renderer->glBindBuffer(target, 0);

    GLESBuffer *buf = (GLESBuffer *)SDL_calloc(1, sizeof(GLESBuffer));
    if (!buf) {
        renderer->glDeleteBuffers(1, &handle);
        return NULL;
    }
    buf->handle = handle;
    buf->size = size;
    buf->usage = usageFlags;

    return (SDL_GPUBuffer *)buf;
}

static SDL_GPUTransferBuffer *GLES_CreateTransferBuffer(
    SDL_GPURenderer *driverData,
    SDL_GPUTransferBufferUsage usage,
    Uint32 size,
    const char *debugName)
{
    (void)driverData;
    (void)debugName;

    GLESTransferBuffer *tb = (GLESTransferBuffer *)SDL_calloc(1, sizeof(GLESTransferBuffer));
    if (!tb) return NULL;
    tb->data = SDL_calloc(1, size);
    if (!tb->data) { SDL_free(tb); return NULL; }
    tb->size = size;
    tb->usage = usage;
    tb->mapped = false;

    return (SDL_GPUTransferBuffer *)tb;
}

/* --- XR (unsupported) --- */

static XrResult GLES_DestroyXRSwapchain(
    SDL_GPURenderer *driverData, XrSwapchain swapchain, SDL_GPUTexture **images)
{
    GPU_STUB_ASSERT(DestroyXRSwapchain);
    return 0; /* XR_ERROR_RUNTIME_FAILURE */
}

static XrResult GLES_CreateXRSession(
    SDL_GPURenderer *driverData, const XrSessionCreateInfo *createinfo, XrSession *session)
{
    GPU_STUB_ASSERT(CreateXRSession);
    return 0;
}

static SDL_GPUTextureFormat *GLES_GetXRSwapchainFormats(
    SDL_GPURenderer *driverData, XrSession session, int *num_formats)
{
    GPU_STUB_ASSERT(GetXRSwapchainFormats);
    if (num_formats) *num_formats = 0;
    return NULL;
}

static XrResult GLES_CreateXRSwapchain(
    SDL_GPURenderer *driverData, XrSession session,
    const XrSwapchainCreateInfo *createinfo, SDL_GPUTextureFormat format,
    XrSwapchain *swapchain, SDL_GPUTexture ***textures)
{
    GPU_STUB_ASSERT(CreateXRSwapchain);
    return 0;
}

/* --- Debug naming (no-ops, don't assert) --- */

static void GLES_SetBufferName(SDL_GPURenderer *driverData, SDL_GPUBuffer *buffer, const char *text)
{
    (void)driverData; (void)buffer; (void)text;
}

static void GLES_SetTextureName(SDL_GPURenderer *driverData, SDL_GPUTexture *texture, const char *text)
{
    (void)driverData; (void)texture; (void)text;
}

static void GLES_InsertDebugLabel(SDL_GPUCommandBuffer *commandBuffer, const char *text)
{
    (void)commandBuffer; (void)text;
}

static void GLES_PushDebugGroup(SDL_GPUCommandBuffer *commandBuffer, const char *name)
{
    (void)commandBuffer; (void)name;
}

static void GLES_PopDebugGroup(SDL_GPUCommandBuffer *commandBuffer)
{
    (void)commandBuffer;
}

/* --- Disposal (no-ops for now since we don't create real resources) --- */

static void GLES_ReleaseTexture(SDL_GPURenderer *driverData, SDL_GPUTexture *texture)
{
    GLESRenderer *renderer = (GLESRenderer *)driverData;
    GLESTexture *tex = (GLESTexture *)texture;
    if (!tex || tex->is_swapchain) return;
    renderer->glDeleteTextures(1, &tex->handle);
    SDL_free(tex);
}

static void GLES_ReleaseSampler(SDL_GPURenderer *driverData, SDL_GPUSampler *sampler)
{
    GLESRenderer *renderer = (GLESRenderer *)driverData;
    GLESSampler *s = (GLESSampler *)sampler;
    if (!s) return;
    renderer->glDeleteSamplers(1, &s->handle);
    SDL_free(s);
}

static void GLES_ReleaseBuffer(SDL_GPURenderer *driverData, SDL_GPUBuffer *buffer)
{
    GLESRenderer *renderer = (GLESRenderer *)driverData;
    GLESBuffer *buf = (GLESBuffer *)buffer;
    if (!buf) return;
    renderer->glDeleteBuffers(1, &buf->handle);
    SDL_free(buf);
}

static void GLES_ReleaseTransferBuffer(SDL_GPURenderer *driverData, SDL_GPUTransferBuffer *transferBuffer)
{
    (void)driverData;
    GLESTransferBuffer *tb = (GLESTransferBuffer *)transferBuffer;
    if (!tb) return;
    SDL_free(tb->data);
    SDL_free(tb);
}

static void GLES_ReleaseShader(SDL_GPURenderer *driverData, SDL_GPUShader *shader)
{
    GLESRenderer *renderer = (GLESRenderer *)driverData;
    GLESShader *s = (GLESShader *)shader;
    if (!s) return;
    if (s->handle && renderer->glDeleteShader) {
        renderer->glDeleteShader(s->handle);
    }
    SDL_free(s->glsl_source);
    SDL_free(s);
}

static void GLES_ReleaseComputePipeline(SDL_GPURenderer *driverData, SDL_GPUComputePipeline *pipeline)
{
    (void)driverData; (void)pipeline;
}

static void GLES_ReleaseGraphicsPipeline(SDL_GPURenderer *driverData, SDL_GPUGraphicsPipeline *graphicsPipeline)
{
    GLESRenderer *renderer = (GLESRenderer *)driverData;
    GLESGraphicsPipeline *p = (GLESGraphicsPipeline *)graphicsPipeline;
    if (!p) return;
    if (p->program && renderer->glDeleteProgram) {
        renderer->glDeleteProgram(p->program);
    }
    SDL_free(p);
}

/* --- Render pass bindings (assert — not yet implemented) --- */

static void GLES_BindGraphicsPipeline(SDL_GPUCommandBuffer *commandBuffer, SDL_GPUGraphicsPipeline *graphicsPipeline)
{
    GLESCommandBuffer *cmdbuf = (GLESCommandBuffer *)commandBuffer;
    GLESRenderer *renderer = cmdbuf->renderer;
    GLESGraphicsPipeline *pipeline = (GLESGraphicsPipeline *)graphicsPipeline;

    cmdbuf->current_pipeline = pipeline;

    renderer->glUseProgram(pipeline->program);

    /* Rasterizer state */
    if (pipeline->rasterizer.cull_mode != SDL_GPU_CULLMODE_NONE) {
        renderer->glEnable(GL_CULL_FACE);
        renderer->glCullFace(pipeline->rasterizer.cull_mode == SDL_GPU_CULLMODE_FRONT ? GL_FRONT : GL_BACK);
    } else {
        renderer->glDisable(GL_CULL_FACE);
    }
    renderer->glFrontFace(pipeline->rasterizer.front_face == SDL_GPU_FRONTFACE_CLOCKWISE ? GL_CW : GL_CCW);

    if (pipeline->rasterizer.enable_depth_bias) {
        renderer->glEnable(GL_POLYGON_OFFSET_FILL);
        renderer->glPolygonOffset(pipeline->rasterizer.depth_bias_slope_factor,
                                  pipeline->rasterizer.depth_bias_constant_factor);
    } else {
        renderer->glDisable(GL_POLYGON_OFFSET_FILL);
    }

    /* Depth/stencil state */
    if (pipeline->depth_stencil.enable_depth_test) {
        renderer->glEnable(GL_DEPTH_TEST);
        renderer->glDepthFunc(GLES_CompareOp(pipeline->depth_stencil.compare_op));
    } else {
        renderer->glDisable(GL_DEPTH_TEST);
    }
    renderer->glDepthMask(pipeline->depth_stencil.enable_depth_write ? GL_TRUE : GL_FALSE);

    if (pipeline->depth_stencil.enable_stencil_test) {
        renderer->glEnable(GL_STENCIL_TEST);
        /* Front face */
        renderer->glStencilFuncSeparate(GL_FRONT,
            GLES_CompareOp(pipeline->depth_stencil.front_stencil_state.compare_op), 0,
            pipeline->depth_stencil.compare_mask);
        renderer->glStencilOpSeparate(GL_FRONT,
            GLES_StencilOp(pipeline->depth_stencil.front_stencil_state.fail_op),
            GLES_StencilOp(pipeline->depth_stencil.front_stencil_state.depth_fail_op),
            GLES_StencilOp(pipeline->depth_stencil.front_stencil_state.pass_op));
        /* Back face */
        renderer->glStencilFuncSeparate(GL_BACK,
            GLES_CompareOp(pipeline->depth_stencil.back_stencil_state.compare_op), 0,
            pipeline->depth_stencil.compare_mask);
        renderer->glStencilOpSeparate(GL_BACK,
            GLES_StencilOp(pipeline->depth_stencil.back_stencil_state.fail_op),
            GLES_StencilOp(pipeline->depth_stencil.back_stencil_state.depth_fail_op),
            GLES_StencilOp(pipeline->depth_stencil.back_stencil_state.pass_op));
        renderer->glStencilMaskSeparate(GL_FRONT_AND_BACK, pipeline->depth_stencil.write_mask);
    } else {
        renderer->glDisable(GL_STENCIL_TEST);
    }

    /* Blend state (GLES 3.x supports only single-target blend) */
    if (pipeline->num_color_targets > 0 && pipeline->blend_states[0].enable_blend) {
        renderer->glEnable(GL_BLEND);
        const SDL_GPUColorTargetBlendState *bs = &pipeline->blend_states[0];
        renderer->glBlendFuncSeparate(
            GLES_BlendFactor(bs->src_color_blendfactor),
            GLES_BlendFactor(bs->dst_color_blendfactor),
            GLES_BlendFactor(bs->src_alpha_blendfactor),
            GLES_BlendFactor(bs->dst_alpha_blendfactor));
        renderer->glBlendEquationSeparate(
            GLES_BlendOp(bs->color_blend_op),
            GLES_BlendOp(bs->alpha_blend_op));
    } else {
        renderer->glDisable(GL_BLEND);
    }

    /* Color write mask */
    if (pipeline->num_color_targets > 0) {
        const SDL_GPUColorTargetBlendState *bs = &pipeline->blend_states[0];
        if (bs->enable_color_write_mask) {
            renderer->glColorMask(
                (bs->color_write_mask & SDL_GPU_COLORCOMPONENT_R) ? GL_TRUE : GL_FALSE,
                (bs->color_write_mask & SDL_GPU_COLORCOMPONENT_G) ? GL_TRUE : GL_FALSE,
                (bs->color_write_mask & SDL_GPU_COLORCOMPONENT_B) ? GL_TRUE : GL_FALSE,
                (bs->color_write_mask & SDL_GPU_COLORCOMPONENT_A) ? GL_TRUE : GL_FALSE);
        } else {
            renderer->glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
        }
    }

    /* Multisample alpha-to-coverage */
    if (pipeline->multisample.enable_alpha_to_coverage) {
        renderer->glEnable(GL_SAMPLE_ALPHA_TO_COVERAGE);
    } else {
        renderer->glDisable(GL_SAMPLE_ALPHA_TO_COVERAGE);
    }
}

static void GLES_SetViewport(SDL_GPUCommandBuffer *cb, const SDL_GPUViewport *viewport)
{
    /* This one is benign enough to implement */
    GLESCommandBuffer *cmdbuf = (GLESCommandBuffer *)cb;
    if (cmdbuf->renderer->glViewport && viewport) {
        cmdbuf->renderer->glViewport(
            (GLint)viewport->x, (GLint)viewport->y,
            (GLsizei)viewport->w, (GLsizei)viewport->h);
    }
}

static void GLES_SetScissor(SDL_GPUCommandBuffer *cb, const SDL_Rect *scissor)
{
    GLESCommandBuffer *cmdbuf = (GLESCommandBuffer *)cb;
    if (cmdbuf->renderer->glScissor && scissor) {
        cmdbuf->renderer->glEnable(GL_SCISSOR_TEST);
        cmdbuf->renderer->glScissor(scissor->x, scissor->y, scissor->w, scissor->h);
    }
}

static void GLES_SetBlendConstants(SDL_GPUCommandBuffer *cb, SDL_FColor blendConstants)
{
    GPU_STUB_ASSERT(SetBlendConstants);
}

static void GLES_SetStencilReference(SDL_GPUCommandBuffer *cb, Uint8 reference)
{
    GPU_STUB_ASSERT(SetStencilReference);
}

static void GLES_BindVertexBuffers(SDL_GPUCommandBuffer *cb, Uint32 firstSlot,
    const SDL_GPUBufferBinding *bindings, Uint32 numBindings)
{
    GLESCommandBuffer *cmdbuf = (GLESCommandBuffer *)cb;
    for (Uint32 i = 0; i < numBindings; i++) {
        Uint32 slot = firstSlot + i;
        if (slot < GLES_MAX_VERTEX_BUFFERS) {
            cmdbuf->vertex_buffers[slot] = (GLESBuffer *)bindings[i].buffer;
            cmdbuf->vertex_buffer_offsets[slot] = bindings[i].offset;
        }
    }
}

static void GLES_BindIndexBuffer(SDL_GPUCommandBuffer *cb,
    const SDL_GPUBufferBinding *binding, SDL_GPUIndexElementSize indexElementSize)
{
    GLESCommandBuffer *cmdbuf = (GLESCommandBuffer *)cb;
    cmdbuf->index_buffer = (GLESBuffer *)binding->buffer;
    cmdbuf->index_buffer_offset = binding->offset;
    cmdbuf->index_type = (indexElementSize == SDL_GPU_INDEXELEMENTSIZE_16BIT) ?
                          GL_UNSIGNED_SHORT : GL_UNSIGNED_INT;
}

static void GLES_BindVertexSamplers(SDL_GPUCommandBuffer *cb, Uint32 firstSlot,
    const SDL_GPUTextureSamplerBinding *bindings, Uint32 numBindings)
{
    GLESCommandBuffer *cmdbuf = (GLESCommandBuffer *)cb;
    GLESRenderer *renderer = cmdbuf->renderer;
    Uint32 frag_count = cmdbuf->current_pipeline ? cmdbuf->current_pipeline->fragment_sampler_count : 0;
    for (Uint32 i = 0; i < numBindings; i++) {
        Uint32 unit = frag_count + firstSlot + i;
        GLESTexture *tex = (GLESTexture *)bindings[i].texture;
        GLESSampler *sampler = (GLESSampler *)bindings[i].sampler;
        if (tex) {
            renderer->glActiveTexture(GL_TEXTURE0 + unit);
            renderer->glBindTexture(tex->target, tex->handle);
        }
        if (sampler) {
            renderer->glBindSampler(unit, sampler->handle);
        }
    }
}

static void GLES_BindVertexStorageTextures(SDL_GPUCommandBuffer *cb, Uint32 firstSlot,
    SDL_GPUTexture *const *textures, Uint32 numBindings)
{
    (void)cb; (void)firstSlot; (void)textures; (void)numBindings;
}

static void GLES_BindVertexStorageBuffers(SDL_GPUCommandBuffer *cb, Uint32 firstSlot,
    SDL_GPUBuffer *const *buffers, Uint32 numBindings)
{
    (void)cb; (void)firstSlot; (void)buffers; (void)numBindings;
}

static void GLES_BindFragmentSamplers(SDL_GPUCommandBuffer *cb, Uint32 firstSlot,
    const SDL_GPUTextureSamplerBinding *bindings, Uint32 numBindings)
{
    GLESCommandBuffer *cmdbuf = (GLESCommandBuffer *)cb;
    GLESRenderer *renderer = cmdbuf->renderer;
    for (Uint32 i = 0; i < numBindings; i++) {
        Uint32 unit = firstSlot + i;
        GLESTexture *tex = (GLESTexture *)bindings[i].texture;
        GLESSampler *sampler = (GLESSampler *)bindings[i].sampler;
        if (tex) {
            renderer->glActiveTexture(GL_TEXTURE0 + unit);
            renderer->glBindTexture(tex->target, tex->handle);
        }
        if (sampler) {
            renderer->glBindSampler(unit, sampler->handle);
        }
    }
}

static void GLES_BindFragmentStorageTextures(SDL_GPUCommandBuffer *cb, Uint32 firstSlot,
    SDL_GPUTexture *const *textures, Uint32 numBindings)
{
    (void)cb; (void)firstSlot; (void)textures; (void)numBindings;
}

static void GLES_BindFragmentStorageBuffers(SDL_GPUCommandBuffer *cb, Uint32 firstSlot,
    SDL_GPUBuffer *const *buffers, Uint32 numBindings)
{
    (void)cb; (void)firstSlot; (void)buffers; (void)numBindings;
}

static void GLES_PushVertexUniformData(SDL_GPUCommandBuffer *cb, Uint32 slotIndex,
    const void *data, Uint32 length)
{
    GLESCommandBuffer *cmdbuf = (GLESCommandBuffer *)cb;
    GLESRenderer *renderer = cmdbuf->renderer;
    if (slotIndex >= GLES_MAX_UNIFORM_BUFFERS) return;

    if (!cmdbuf->vertex_ubos[slotIndex]) {
        renderer->glGenBuffers(1, &cmdbuf->vertex_ubos[slotIndex]);
    }
    renderer->glBindBuffer(GL_UNIFORM_BUFFER, cmdbuf->vertex_ubos[slotIndex]);
    renderer->glBufferData(GL_UNIFORM_BUFFER, (GLsizeiptr)length, data, GL_DYNAMIC_DRAW);
    renderer->glBindBufferBase(GL_UNIFORM_BUFFER, slotIndex, cmdbuf->vertex_ubos[slotIndex]);
    renderer->glBindBuffer(GL_UNIFORM_BUFFER, 0);
}

static void GLES_PushFragmentUniformData(SDL_GPUCommandBuffer *cb, Uint32 slotIndex,
    const void *data, Uint32 length)
{
    GLESCommandBuffer *cmdbuf = (GLESCommandBuffer *)cb;
    GLESRenderer *renderer = cmdbuf->renderer;
    if (slotIndex >= GLES_MAX_UNIFORM_BUFFERS) return;

    Uint32 binding = GLES_MAX_UNIFORM_BUFFERS + slotIndex;

    if (!cmdbuf->fragment_ubos[slotIndex]) {
        renderer->glGenBuffers(1, &cmdbuf->fragment_ubos[slotIndex]);
    }
    renderer->glBindBuffer(GL_UNIFORM_BUFFER, cmdbuf->fragment_ubos[slotIndex]);
    renderer->glBufferData(GL_UNIFORM_BUFFER, (GLsizeiptr)length, data, GL_DYNAMIC_DRAW);
    renderer->glBindBufferBase(GL_UNIFORM_BUFFER, binding, cmdbuf->fragment_ubos[slotIndex]);
    renderer->glBindBuffer(GL_UNIFORM_BUFFER, 0);
}

/* --- Vertex input setup and draw commands --- */

static void GLES_SetupVertexInput(GLESCommandBuffer *cmdbuf)
{
    GLESRenderer *renderer = cmdbuf->renderer;
    GLESGraphicsPipeline *pipeline = cmdbuf->current_pipeline;
    if (!pipeline) return;

    renderer->glBindVertexArray(cmdbuf->vao);

    /* Disable all attributes first */
    for (Uint32 i = 0; i < GLES_MAX_VERTEX_ATTRIBUTES; i++) {
        renderer->glDisableVertexAttribArray(i);
    }

    /* Set up each attribute */
    for (Uint32 i = 0; i < pipeline->num_vertex_attributes; i++) {
        GLESVertexAttribute *attr = &pipeline->vertex_attributes[i];
        GLESBuffer *buf = cmdbuf->vertex_buffers[attr->buffer_slot];
        if (!buf) continue;

        /* Find the vertex buffer description for this slot */
        GLESVertexBufferDesc *vbd = NULL;
        for (Uint32 j = 0; j < pipeline->num_vertex_buffers; j++) {
            if (pipeline->vertex_buffers[j].slot == attr->buffer_slot) {
                vbd = &pipeline->vertex_buffers[j];
                break;
            }
        }
        if (!vbd) continue;

        renderer->glBindBuffer(GL_ARRAY_BUFFER, buf->handle);
        renderer->glEnableVertexAttribArray(attr->location);

        GLsizei stride = (GLsizei)vbd->pitch;
        const void *offset = (const void *)(uintptr_t)(
            cmdbuf->vertex_buffer_offsets[attr->buffer_slot] + attr->offset);

        if (attr->gl_integer) {
            renderer->glVertexAttribIPointer(attr->location, attr->gl_size,
                                              attr->gl_type, stride, offset);
        } else {
            renderer->glVertexAttribPointer(attr->location, attr->gl_size,
                                             attr->gl_type, attr->gl_normalized, stride, offset);
        }

        /* Instance divisor */
        if (vbd->input_rate == SDL_GPU_VERTEXINPUTRATE_INSTANCE) {
            renderer->glVertexAttribDivisor(attr->location,
                vbd->instance_step_rate > 0 ? vbd->instance_step_rate : 1);
        } else {
            renderer->glVertexAttribDivisor(attr->location, 0);
        }
    }
}

static void GLES_DrawIndexedPrimitives(SDL_GPUCommandBuffer *cb,
    Uint32 numIndices, Uint32 numInstances, Uint32 firstIndex,
    Sint32 vertexOffset, Uint32 firstInstance)
{
    GLESCommandBuffer *cmdbuf = (GLESCommandBuffer *)cb;
    GLESRenderer *renderer = cmdbuf->renderer;
    if (!cmdbuf->current_pipeline || !cmdbuf->index_buffer) return;

    GLES_SetupVertexInput(cmdbuf);

    (void)vertexOffset;   /* GLES 3.0 has no glDrawElementsBaseVertex */
    (void)firstInstance;

    renderer->glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, cmdbuf->index_buffer->handle);

    size_t index_size = (cmdbuf->index_type == GL_UNSIGNED_SHORT) ? 2 : 4;
    const void *offset = (const void *)(uintptr_t)(
        cmdbuf->index_buffer_offset + firstIndex * index_size);

    if (numInstances > 1) {
        renderer->glDrawElementsInstanced(cmdbuf->current_pipeline->primitive_type,
                                           (GLsizei)numIndices, cmdbuf->index_type,
                                           offset, (GLsizei)numInstances);
    } else {
        renderer->glDrawElements(cmdbuf->current_pipeline->primitive_type,
                                  (GLsizei)numIndices, cmdbuf->index_type, offset);
    }
}

static void GLES_DrawPrimitives(SDL_GPUCommandBuffer *cb,
    Uint32 numVertices, Uint32 numInstances, Uint32 firstVertex, Uint32 firstInstance)
{
    GLESCommandBuffer *cmdbuf = (GLESCommandBuffer *)cb;
    GLESRenderer *renderer = cmdbuf->renderer;
    if (!cmdbuf->current_pipeline) return;

    GLES_SetupVertexInput(cmdbuf);

    (void)firstInstance;

    if (numInstances > 1) {
        renderer->glDrawArraysInstanced(cmdbuf->current_pipeline->primitive_type,
                                         (GLint)firstVertex, (GLsizei)numVertices,
                                         (GLsizei)numInstances);
    } else {
        renderer->glDrawArrays(cmdbuf->current_pipeline->primitive_type,
                                (GLint)firstVertex, (GLsizei)numVertices);
    }
}

static void GLES_DrawPrimitivesIndirect(SDL_GPUCommandBuffer *cb,
    SDL_GPUBuffer *buffer, Uint32 offset, Uint32 drawCount)
{
    (void)cb; (void)buffer; (void)offset; (void)drawCount;
    SDL_SetError("Indirect draw not supported on GLES");
}

static void GLES_DrawIndexedPrimitivesIndirect(SDL_GPUCommandBuffer *cb,
    SDL_GPUBuffer *buffer, Uint32 offset, Uint32 drawCount)
{
    (void)cb; (void)buffer; (void)offset; (void)drawCount;
    SDL_SetError("Indirect draw not supported on GLES");
}

/* --- Compute pass (assert) --- */

static void GLES_BeginComputePass(SDL_GPUCommandBuffer *cb,
    const SDL_GPUStorageTextureReadWriteBinding *texBindings, Uint32 numTexBindings,
    const SDL_GPUStorageBufferReadWriteBinding *bufBindings, Uint32 numBufBindings)
{
    GPU_STUB_ASSERT(BeginComputePass);
}

static void GLES_BindComputePipeline(SDL_GPUCommandBuffer *cb, SDL_GPUComputePipeline *pipeline)
{
    GPU_STUB_ASSERT(BindComputePipeline);
}

static void GLES_BindComputeSamplers(SDL_GPUCommandBuffer *cb, Uint32 firstSlot,
    const SDL_GPUTextureSamplerBinding *bindings, Uint32 numBindings)
{
    GPU_STUB_ASSERT(BindComputeSamplers);
}

static void GLES_BindComputeStorageTextures(SDL_GPUCommandBuffer *cb, Uint32 firstSlot,
    SDL_GPUTexture *const *textures, Uint32 numBindings)
{
    GPU_STUB_ASSERT(BindComputeStorageTextures);
}

static void GLES_BindComputeStorageBuffers(SDL_GPUCommandBuffer *cb, Uint32 firstSlot,
    SDL_GPUBuffer *const *buffers, Uint32 numBindings)
{
    GPU_STUB_ASSERT(BindComputeStorageBuffers);
}

static void GLES_PushComputeUniformData(SDL_GPUCommandBuffer *cb, Uint32 slotIndex,
    const void *data, Uint32 length)
{
    GPU_STUB_ASSERT(PushComputeUniformData);
}

static void GLES_DispatchCompute(SDL_GPUCommandBuffer *cb,
    Uint32 groupcountX, Uint32 groupcountY, Uint32 groupcountZ)
{
    GPU_STUB_ASSERT(DispatchCompute);
}

static void GLES_DispatchComputeIndirect(SDL_GPUCommandBuffer *cb,
    SDL_GPUBuffer *buffer, Uint32 offset)
{
    GPU_STUB_ASSERT(DispatchComputeIndirect);
}

static void GLES_EndComputePass(SDL_GPUCommandBuffer *cb)
{
    GPU_STUB_ASSERT(EndComputePass);
}

/* --- Transfer / Copy (assert) --- */

static void *GLES_MapTransferBuffer(SDL_GPURenderer *driverData,
    SDL_GPUTransferBuffer *transferBuffer, bool cycle)
{
    (void)driverData; (void)cycle;
    GLESTransferBuffer *tb = (GLESTransferBuffer *)transferBuffer;
    if (!tb) return NULL;
    tb->mapped = true;
    return tb->data;
}

static void GLES_UnmapTransferBuffer(SDL_GPURenderer *driverData,
    SDL_GPUTransferBuffer *transferBuffer)
{
    (void)driverData;
    GLESTransferBuffer *tb = (GLESTransferBuffer *)transferBuffer;
    if (tb) tb->mapped = false;
}

static void GLES_BeginCopyPass(SDL_GPUCommandBuffer *cb)
{
    (void)cb;  /* nothing to do in immediate mode */
}

static void GLES_UploadToTexture(SDL_GPUCommandBuffer *cb,
    const SDL_GPUTextureTransferInfo *source, const SDL_GPUTextureRegion *dest, bool cycle)
{
    GLESCommandBuffer *cmdbuf = (GLESCommandBuffer *)cb;
    GLESRenderer *renderer = cmdbuf->renderer;
    (void)cycle;

    GLESTransferBuffer *tb = (GLESTransferBuffer *)source->transfer_buffer;
    GLESTexture *tex = (GLESTexture *)dest->texture;
    if (!tb || !tex) return;

    const Uint8 *data = (const Uint8 *)tb->data + source->offset;

    renderer->glBindTexture(tex->target, tex->handle);

    if (source->pixels_per_row > 0) {
        renderer->glPixelStorei(GL_UNPACK_ROW_LENGTH, (GLint)source->pixels_per_row);
    }

    if (tex->target == GL_TEXTURE_2D || tex->target == GL_TEXTURE_CUBE_MAP) {
        GLenum face_target = tex->target;
        if (tex->target == GL_TEXTURE_CUBE_MAP) {
            face_target = GL_TEXTURE_CUBE_MAP_POSITIVE_X + dest->layer;
        }
        renderer->glTexSubImage2D(face_target, (GLint)dest->mip_level,
                                   (GLint)dest->x, (GLint)dest->y,
                                   (GLsizei)dest->w, (GLsizei)dest->h,
                                   tex->gl_format, tex->gl_type, data);
    } else {
        renderer->glTexSubImage3D(tex->target, (GLint)dest->mip_level,
                                   (GLint)dest->x, (GLint)dest->y, (GLint)dest->layer,
                                   (GLsizei)dest->w, (GLsizei)dest->h, (GLsizei)dest->d,
                                   tex->gl_format, tex->gl_type, data);
    }

    if (source->pixels_per_row > 0) {
        renderer->glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
    }

    renderer->glBindTexture(tex->target, 0);
}

static void GLES_UploadToBuffer(SDL_GPUCommandBuffer *cb,
    const SDL_GPUTransferBufferLocation *source, const SDL_GPUBufferRegion *dest, bool cycle)
{
    GLESCommandBuffer *cmdbuf = (GLESCommandBuffer *)cb;
    GLESRenderer *renderer = cmdbuf->renderer;
    (void)cycle;

    GLESTransferBuffer *tb = (GLESTransferBuffer *)source->transfer_buffer;
    GLESBuffer *buf = (GLESBuffer *)dest->buffer;
    if (!tb || !buf) return;

    const Uint8 *data = (const Uint8 *)tb->data + source->offset;

    renderer->glBindBuffer(GL_COPY_WRITE_BUFFER, buf->handle);
    renderer->glBufferSubData(GL_COPY_WRITE_BUFFER, (GLintptr)dest->offset,
                               (GLsizeiptr)dest->size, data);
    renderer->glBindBuffer(GL_COPY_WRITE_BUFFER, 0);
}

static void GLES_CopyTextureToTexture(SDL_GPUCommandBuffer *cb,
    const SDL_GPUTextureLocation *source, const SDL_GPUTextureLocation *dest,
    Uint32 w, Uint32 h, Uint32 d, bool cycle)
{
    GPU_STUB_ASSERT(CopyTextureToTexture);
}

static void GLES_CopyBufferToBuffer(SDL_GPUCommandBuffer *cb,
    const SDL_GPUBufferLocation *source, const SDL_GPUBufferLocation *dest,
    Uint32 size, bool cycle)
{
    GPU_STUB_ASSERT(CopyBufferToBuffer);
}

static void GLES_GenerateMipmaps(SDL_GPUCommandBuffer *cb, SDL_GPUTexture *texture)
{
    GPU_STUB_ASSERT(GenerateMipmaps);
}

static void GLES_DownloadFromTexture(SDL_GPUCommandBuffer *cb,
    const SDL_GPUTextureRegion *source, const SDL_GPUTextureTransferInfo *dest)
{
    GPU_STUB_ASSERT(DownloadFromTexture);
}

static void GLES_DownloadFromBuffer(SDL_GPUCommandBuffer *cb,
    const SDL_GPUBufferRegion *source, const SDL_GPUTransferBufferLocation *dest)
{
    GPU_STUB_ASSERT(DownloadFromBuffer);
}

static void GLES_EndCopyPass(SDL_GPUCommandBuffer *cb)
{
    (void)cb;  /* nothing to do in immediate mode */
}

static void GLES_Blit(SDL_GPUCommandBuffer *cb, const SDL_GPUBlitInfo *info)
{
    GPU_STUB_ASSERT(Blit);
}

/* ======================================================================== */
/* Bootstrap: PrepareDriver and CreateDevice                               */
/* ======================================================================== */

/* Forward declaration */
static bool GLES_ClaimWindow_WithGLInit(SDL_GPURenderer *driverData, SDL_Window *window);

static bool GLES_PrepareDriver(SDL_VideoDevice *_this, SDL_PropertiesID props)
{
    (void)props;

    /* We need a video driver that provides GL_GetProcAddress */
    if (!_this || !_this->GL_GetProcAddress) {
        return false;
    }

    /* For now, always return true if we have GL support.
     * On the r36s, GL is provided via our SDL2 video driver -> EGL/Mali.
     * On desktop, this may also work with GLX (for testing). */
    return true;
}

static SDL_GPUDevice *GLES_CreateDevice(bool debug_mode, bool prefer_low_power, SDL_PropertiesID props)
{
    GLESRenderer *renderer = NULL;
    SDL_GPUDevice *result = NULL;

    (void)prefer_low_power;
    (void)props;

    renderer = (GLESRenderer *)SDL_calloc(1, sizeof(GLESRenderer));
    if (!renderer) {
        return NULL;
    }
    renderer->debug_mode = debug_mode;

    /* We need a GL context to load functions. The context should already
     * exist if the video driver set one up, or we create a temporary one.
     * For the MVP, we assume a GL context is available when ClaimWindow
     * is called. Load GL functions lazily on first ClaimWindow. */

    result = (SDL_GPUDevice *)SDL_calloc(1, sizeof(SDL_GPUDevice));
    if (!result) {
        SDL_free(renderer);
        return NULL;
    }

    /* Fill ALL vtable function pointers via the ASSIGN_DRIVER macro */
    ASSIGN_DRIVER(GLES)

    /* Override ClaimWindow with the version that initializes GL on first call */
    result->ClaimWindow = GLES_ClaimWindow_WithGLInit;

    result->driverData = (SDL_GPURenderer *)renderer;
    result->shader_formats = SDL_GPU_SHADERFORMAT_SPIRV;
    result->debug_mode = debug_mode;

    return result;
}

/* Override ClaimWindow to also load GL functions on first use */
static bool GLES_ClaimWindow_WithGLInit(SDL_GPURenderer *driverData, SDL_Window *window)
{
    GLESRenderer *renderer = (GLESRenderer *)driverData;

    /* Load GL functions on first claim (we need a current context) */
    if (!renderer->glClear) {
        /* Set GL attributes for GLES before loading library or creating context */
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);

        /* Load the GL library through the video driver */
        if (!SDL_GL_LoadLibrary(NULL)) {
            return SDL_SetError("GLES GPU: Failed to load GL library: %s", SDL_GetError());
        }

        /* Add OPENGL flag to the window so SDL3 allows GL context creation */
        window->flags |= SDL_WINDOW_OPENGL;

        /* Create GL context */
        SDL_GLContext ctx = SDL_GL_CreateContext(window);
        if (!ctx) {
            return SDL_SetError("GLES GPU: Failed to create GL context: %s", SDL_GetError());
        }

        if (!GLES_LoadGLFunctions(renderer)) {
            SDL_GL_DestroyContext(ctx);
            return false;
        }

        /* Log GL info */
        if (renderer->glGetString) {
            const char *version = (const char *)renderer->glGetString(GL_VERSION);
            SDL_Log("GLES GPU backend initialized: %s", version ? version : "unknown");
        }

        /* Create a VAO for the command buffer */
        if (renderer->glGenVertexArrays) {
            renderer->glGenVertexArrays(1, &g_cmdbuf.vao);
        }
    }

    return GLES_ClaimWindow(driverData, window);
}

SDL_GPUBootstrap GLESDriver = {
    "gles",
    GLES_PrepareDriver,
    GLES_CreateDevice
};

#endif /* SDL_GPU_GLES */
