#include <CZ/Ream/SK/RSKContext.h>

using namespace CZ;

struct SKGrOptions : public GrContextOptions
{
    SKGrOptions() noexcept
    {
        fShaderCacheStrategy = GrContextOptions::ShaderCacheStrategy::kBackendBinary;
        fAvoidStencilBuffers = true;
        fPreferExternalImagesOverES3 = true;
        fDisableGpuYUVConversion = true;
        fReducedShaderVariations = false;
        fSuppressPrints = true;
        fSuppressMipmapSupport = true;
        fSkipGLErrorChecks = GrContextOptions::Enable::kYes;
        fBufferMapThreshold = -1;
        fDisableDistanceFieldPaths = true;
        fAllowPathMaskCaching = true;
        fGlyphCacheTextureMaximumBytes = 2048 * 1024 * 4;
        fUseDrawInsteadOfClear = GrContextOptions::Enable::kYes;
        fReduceOpsTaskSplitting = GrContextOptions::Enable::kYes;
        fDisableDriverCorrectnessWorkarounds = true;
        fRuntimeProgramCacheSize = 1024;
        fInternalMultisampleCount = 4;
        fDisableTessellationPathRenderer = false;
        fAllowMSAAOnNewIntel = true;
        fAlwaysUseTexStorageWhenAvailable = true;
    }
};

GrContextOptions &CZ::GetSKContextOptions() noexcept
{
    static SKGrOptions s_options {};
    return s_options;
}
