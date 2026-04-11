/*
  SDL3 GPU backend for OpenGL ES — SPIR-V to GLSL ES shader transpilation

  Uses SPIRV-Cross C API to convert SPIR-V bytecode to GLSL ES.
  Vertex/Fragment shaders → GLSL ES 3.10
  Compute shaders → GLSL 4.30 (desktop, for image format support)

  SPIR-V from HLSL (via DXC/shadercross) uses separate texture and sampler
  objects. GLSL ES requires combined sampler types (e.g. sampler2D).
  We call spvc_compiler_build_combined_image_samplers() to handle this.
*/

#include "SDL_internal.h"

#ifdef SDL_GPU_GLES

#include "../SDL_sysgpu.h"
#include "SDL_gpu_gles.h"

#include <spirv_cross_c.h>

char *GLES_TranspileSPIRV(const Uint8 *spirv_code, size_t spirv_size,
                           const char *entrypoint, SDL_GPUShaderStage stage)
{
    (void)entrypoint;

    if (!spirv_code || spirv_size < 4) {
        SDL_SetError("Invalid SPIR-V data");
        return NULL;
    }

    /* SPIR-V is a stream of 32-bit words */
    size_t word_count = spirv_size / sizeof(SpvId);
    const SpvId *spirv_words = (const SpvId *)spirv_code;

    spvc_context context = NULL;
    spvc_parsed_ir parsed_ir = NULL;
    spvc_compiler compiler = NULL;
    spvc_compiler_options options = NULL;
    const char *glsl_source = NULL;
    char *result = NULL;

    /* Determine target GLSL version based on shader stage.
     * Use GLSL ES 3.10 for vertex/fragment — many shaders from Vulkan/D3D12 apps
     * use storage buffers (SSBOs) which require ES 3.10+, and compute needs 3.10. */
    unsigned int glsl_version = 310;
    bool is_compute = (stage != SDL_GPU_SHADERSTAGE_VERTEX &&
                       stage != SDL_GPU_SHADERSTAGE_FRAGMENT);

    /* Create SPIRV-Cross context */
    if (spvc_context_create(&context) != SPVC_SUCCESS) {
        SDL_SetError("Failed to create SPIRV-Cross context");
        return NULL;
    }

    /* Parse SPIR-V */
    if (spvc_context_parse_spirv(context, spirv_words, word_count, &parsed_ir) != SPVC_SUCCESS) {
        SDL_SetError("Failed to parse SPIR-V: %s", spvc_context_get_last_error_string(context));
        goto cleanup;
    }

    /* Create GLSL compiler */
    if (spvc_context_create_compiler(context, SPVC_BACKEND_GLSL, parsed_ir,
                                      SPVC_CAPTURE_MODE_TAKE_OWNERSHIP, &compiler) != SPVC_SUCCESS) {
        SDL_SetError("Failed to create GLSL compiler: %s", spvc_context_get_last_error_string(context));
        goto cleanup;
    }

    /*
     * For compute shaders that use texelFetch (no sampler), build a dummy sampler
     * BEFORE build_combined_image_samplers, or SPIRV-Cross will error.
     */
    if (is_compute) {
        spvc_variable_id dummy_id;
        spvc_compiler_build_dummy_sampler_for_combined_images(compiler, &dummy_id);
    }

    /*
     * HLSL shaders use separate texture/sampler objects (Texture2D + SamplerState).
     * GLSL ES requires combined samplers (sampler2D). SPIRV-Cross can combine them,
     * but we must call build_combined_image_samplers() before compile().
     */
    if (spvc_compiler_build_combined_image_samplers(compiler) != SPVC_SUCCESS) {
        SDL_SetError("Failed to build combined image samplers: %s",
                     spvc_context_get_last_error_string(context));
        goto cleanup;
    }

    /*
     * Rename uniform block types AND instance names to include stage prefix.
     * HLSL cbuffers in different stages can have the same name but different
     * contents (e.g. both vertex and fragment have "UniformBuffer" slot 0).
     * In GLSL, linked programs require uniform blocks with the same name to
     * have identical definitions. We prefix with stage to avoid collisions.
     */
    {
        const char *stage_prefix = (stage == SDL_GPU_SHADERSTAGE_VERTEX) ? "vs_" :
                                   (stage == SDL_GPU_SHADERSTAGE_FRAGMENT) ? "fs_" : "cs_";
        spvc_resources resources = NULL;
        if (spvc_compiler_create_shader_resources(compiler, &resources) == SPVC_SUCCESS) {
            const spvc_reflected_resource *list = NULL;
            size_t count = 0;
            if (spvc_resources_get_resource_list_for_type(resources,
                    SPVC_RESOURCE_TYPE_UNIFORM_BUFFER, &list, &count) == SPVC_SUCCESS) {
                for (size_t i = 0; i < count; i++) {
                    char new_name[256];
                    /* Rename block type (controls the "uniform BlockTypeName { ... }" part) */
                    SDL_snprintf(new_name, sizeof(new_name), "%sUBO_%u",
                             stage_prefix, (unsigned)i);
                    spvc_compiler_set_name(compiler, list[i].base_type_id, new_name);
                    /* Rename variable/instance name (controls "} instanceName;") */
                    SDL_snprintf(new_name, sizeof(new_name), "%subo_%u",
                             stage_prefix, (unsigned)i);
                    spvc_compiler_set_name(compiler, list[i].id, new_name);
                }
            }
        }
    }

    /* Set compiler options */
    if (spvc_compiler_create_compiler_options(compiler, &options) != SPVC_SUCCESS) {
        SDL_SetError("Failed to create compiler options");
        goto cleanup;
    }

    if (is_compute) {
        /* Compute shaders may use image formats not available in ES (e.g. r8).
         * Use desktop GLSL 430 which supports all image formats.
         * Mesa on aarch64 supports both ES and desktop GL contexts. */
        spvc_compiler_options_set_uint(options, SPVC_COMPILER_OPTION_GLSL_VERSION, 430);
        spvc_compiler_options_set_bool(options, SPVC_COMPILER_OPTION_GLSL_ES, SPVC_FALSE);
    } else {
        spvc_compiler_options_set_uint(options, SPVC_COMPILER_OPTION_GLSL_VERSION, glsl_version);
        spvc_compiler_options_set_bool(options, SPVC_COMPILER_OPTION_GLSL_ES, SPVC_TRUE);
        spvc_compiler_options_set_bool(options, SPVC_COMPILER_OPTION_GLSL_ES_DEFAULT_FLOAT_PRECISION_HIGHP, SPVC_TRUE);
        spvc_compiler_options_set_bool(options, SPVC_COMPILER_OPTION_GLSL_ES_DEFAULT_INT_PRECISION_HIGHP, SPVC_TRUE);
    }
    spvc_compiler_options_set_bool(options, SPVC_COMPILER_OPTION_GLSL_VULKAN_SEMANTICS, SPVC_FALSE);
    spvc_compiler_install_compiler_options(compiler, options);

    /* Compile to GLSL */
    if (spvc_compiler_compile(compiler, &glsl_source) != SPVC_SUCCESS) {
        SDL_SetError("SPIR-V to GLSL ES %u compilation failed: %s",
                     glsl_version, spvc_context_get_last_error_string(context));
        goto cleanup;
    }

    /* Duplicate the result (spvc owns the original) */
    result = SDL_strdup(glsl_source);
    if (!result) {
        SDL_SetError("Out of memory copying GLSL source");
    }

cleanup:
    if (context) spvc_context_destroy(context);
    return result;
}

#endif /* SDL_GPU_GLES */
