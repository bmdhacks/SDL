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

    /* Check critical functions */
    if (!renderer->glClear || !renderer->glClearColor || !renderer->glGetString) {
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

static SDL_GPUGraphicsPipeline *GLES_CreateGraphicsPipeline(
    SDL_GPURenderer *driverData,
    const SDL_GPUGraphicsPipelineCreateInfo *createinfo)
{
    GPU_STUB_ASSERT(CreateGraphicsPipeline);
    return NULL;
}

static SDL_GPUSampler *GLES_CreateSampler(
    SDL_GPURenderer *driverData,
    const SDL_GPUSamplerCreateInfo *createinfo)
{
    GPU_STUB_ASSERT(CreateSampler);
    return NULL;
}

static SDL_GPUShader *GLES_CreateShader(
    SDL_GPURenderer *driverData,
    const SDL_GPUShaderCreateInfo *createinfo)
{
    GPU_STUB_ASSERT(CreateShader);
    return NULL;
}

static SDL_GPUTexture *GLES_CreateTexture(
    SDL_GPURenderer *driverData,
    const SDL_GPUTextureCreateInfo *createinfo)
{
    GPU_STUB_ASSERT(CreateTexture);
    return NULL;
}

static SDL_GPUBuffer *GLES_CreateBuffer(
    SDL_GPURenderer *driverData,
    SDL_GPUBufferUsageFlags usageFlags,
    Uint32 size,
    const char *debugName)
{
    GPU_STUB_ASSERT(CreateBuffer);
    return NULL;
}

static SDL_GPUTransferBuffer *GLES_CreateTransferBuffer(
    SDL_GPURenderer *driverData,
    SDL_GPUTransferBufferUsage usage,
    Uint32 size,
    const char *debugName)
{
    GPU_STUB_ASSERT(CreateTransferBuffer);
    return NULL;
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
    (void)driverData; (void)texture;
}

static void GLES_ReleaseSampler(SDL_GPURenderer *driverData, SDL_GPUSampler *sampler)
{
    (void)driverData; (void)sampler;
}

static void GLES_ReleaseBuffer(SDL_GPURenderer *driverData, SDL_GPUBuffer *buffer)
{
    (void)driverData; (void)buffer;
}

static void GLES_ReleaseTransferBuffer(SDL_GPURenderer *driverData, SDL_GPUTransferBuffer *tb)
{
    (void)driverData; (void)tb;
}

static void GLES_ReleaseShader(SDL_GPURenderer *driverData, SDL_GPUShader *shader)
{
    (void)driverData; (void)shader;
}

static void GLES_ReleaseComputePipeline(SDL_GPURenderer *driverData, SDL_GPUComputePipeline *pipeline)
{
    (void)driverData; (void)pipeline;
}

static void GLES_ReleaseGraphicsPipeline(SDL_GPURenderer *driverData, SDL_GPUGraphicsPipeline *pipeline)
{
    (void)driverData; (void)pipeline;
}

/* --- Render pass bindings (assert — not yet implemented) --- */

static void GLES_BindGraphicsPipeline(SDL_GPUCommandBuffer *cb, SDL_GPUGraphicsPipeline *pipeline)
{
    GPU_STUB_ASSERT(BindGraphicsPipeline);
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
    GPU_STUB_ASSERT(BindVertexBuffers);
}

static void GLES_BindIndexBuffer(SDL_GPUCommandBuffer *cb,
    const SDL_GPUBufferBinding *binding, SDL_GPUIndexElementSize indexElementSize)
{
    GPU_STUB_ASSERT(BindIndexBuffer);
}

static void GLES_BindVertexSamplers(SDL_GPUCommandBuffer *cb, Uint32 firstSlot,
    const SDL_GPUTextureSamplerBinding *bindings, Uint32 numBindings)
{
    GPU_STUB_ASSERT(BindVertexSamplers);
}

static void GLES_BindVertexStorageTextures(SDL_GPUCommandBuffer *cb, Uint32 firstSlot,
    SDL_GPUTexture *const *textures, Uint32 numBindings)
{
    GPU_STUB_ASSERT(BindVertexStorageTextures);
}

static void GLES_BindVertexStorageBuffers(SDL_GPUCommandBuffer *cb, Uint32 firstSlot,
    SDL_GPUBuffer *const *buffers, Uint32 numBindings)
{
    GPU_STUB_ASSERT(BindVertexStorageBuffers);
}

static void GLES_BindFragmentSamplers(SDL_GPUCommandBuffer *cb, Uint32 firstSlot,
    const SDL_GPUTextureSamplerBinding *bindings, Uint32 numBindings)
{
    GPU_STUB_ASSERT(BindFragmentSamplers);
}

static void GLES_BindFragmentStorageTextures(SDL_GPUCommandBuffer *cb, Uint32 firstSlot,
    SDL_GPUTexture *const *textures, Uint32 numBindings)
{
    GPU_STUB_ASSERT(BindFragmentStorageTextures);
}

static void GLES_BindFragmentStorageBuffers(SDL_GPUCommandBuffer *cb, Uint32 firstSlot,
    SDL_GPUBuffer *const *buffers, Uint32 numBindings)
{
    GPU_STUB_ASSERT(BindFragmentStorageBuffers);
}

static void GLES_PushVertexUniformData(SDL_GPUCommandBuffer *cb, Uint32 slotIndex,
    const void *data, Uint32 length)
{
    GPU_STUB_ASSERT(PushVertexUniformData);
}

static void GLES_PushFragmentUniformData(SDL_GPUCommandBuffer *cb, Uint32 slotIndex,
    const void *data, Uint32 length)
{
    GPU_STUB_ASSERT(PushFragmentUniformData);
}

/* --- Draw commands (assert) --- */

static void GLES_DrawIndexedPrimitives(SDL_GPUCommandBuffer *cb,
    Uint32 numIndices, Uint32 numInstances, Uint32 firstIndex,
    Sint32 vertexOffset, Uint32 firstInstance)
{
    GPU_STUB_ASSERT(DrawIndexedPrimitives);
}

static void GLES_DrawPrimitives(SDL_GPUCommandBuffer *cb,
    Uint32 numVertices, Uint32 numInstances, Uint32 firstVertex, Uint32 firstInstance)
{
    GPU_STUB_ASSERT(DrawPrimitives);
}

static void GLES_DrawPrimitivesIndirect(SDL_GPUCommandBuffer *cb,
    SDL_GPUBuffer *buffer, Uint32 offset, Uint32 drawCount)
{
    GPU_STUB_ASSERT(DrawPrimitivesIndirect);
}

static void GLES_DrawIndexedPrimitivesIndirect(SDL_GPUCommandBuffer *cb,
    SDL_GPUBuffer *buffer, Uint32 offset, Uint32 drawCount)
{
    GPU_STUB_ASSERT(DrawIndexedPrimitivesIndirect);
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
    GPU_STUB_ASSERT(MapTransferBuffer);
    return NULL;
}

static void GLES_UnmapTransferBuffer(SDL_GPURenderer *driverData,
    SDL_GPUTransferBuffer *transferBuffer)
{
    GPU_STUB_ASSERT(UnmapTransferBuffer);
}

static void GLES_BeginCopyPass(SDL_GPUCommandBuffer *cb)
{
    GPU_STUB_ASSERT(BeginCopyPass);
}

static void GLES_UploadToTexture(SDL_GPUCommandBuffer *cb,
    const SDL_GPUTextureTransferInfo *source, const SDL_GPUTextureRegion *dest, bool cycle)
{
    GPU_STUB_ASSERT(UploadToTexture);
}

static void GLES_UploadToBuffer(SDL_GPUCommandBuffer *cb,
    const SDL_GPUTransferBufferLocation *source, const SDL_GPUBufferRegion *dest, bool cycle)
{
    GPU_STUB_ASSERT(UploadToBuffer);
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
    GPU_STUB_ASSERT(EndCopyPass);
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
    }

    return GLES_ClaimWindow(driverData, window);
}

SDL_GPUBootstrap GLESDriver = {
    "gles",
    GLES_PrepareDriver,
    GLES_CreateDevice
};

#endif /* SDL_GPU_GLES */
