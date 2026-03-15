// SPDX-License-Identifier: BSD-3-Clause
// Copyright Contributors to the OpenColorIO Project.

#ifndef INCLUDED_OCIO_METAL_COLOR_PIPELINE_H
#define INCLUDED_OCIO_METAL_COLOR_PIPELINE_H

#include <OpenColorIO/OpenColorIO.h>

#import <Metal/Metal.h>

namespace OCIO_NAMESPACE
{

class MetalColorPipeline;
typedef OCIO_SHARED_PTR<MetalColorPipeline> MetalColorPipelineRcPtr;

/// Working color space for the grading pipeline.
enum WorkingSpace
{
    /// AP1 primaries, scene-linear. Good for compositing, physical light math.
    WORKING_SPACE_ACESCG = 0,
    /// AP1 primaries, log-like curve. Good for grading (lift/gamma/gain).
    WORKING_SPACE_ACESCCT
};

/// Display / output target.
enum DisplayTarget
{
    DISPLAY_SDR_sRGB = 0,          ///< SDR 100 nits, sRGB / Rec.709
    DISPLAY_SDR_P3,                ///< SDR 100 nits, P3 D65
    DISPLAY_HDR_1000_P3,           ///< HDR 1000 nits, P3 D65
    DISPLAY_HDR_1000_REC2020       ///< HDR 1000 nits, Rec.2020
};

/// Camera / input encoding.
enum InputEncoding
{
    INPUT_SLOG3_SGAMUT3CINE = 0,   ///< Sony S-Log3 / S-Gamut3.Cine
    INPUT_LOGC4_WIDE_GAMUT_4,      ///< ARRI LogC4 / AWG4
    INPUT_LOGC3_WIDE_GAMUT_3,      ///< ARRI LogC3 / AWG3
    INPUT_VLOG_VGAMUT,             ///< Panasonic V-Log / V-Gamut
    INPUT_LOG3G10_RWGRGB,          ///< RED Log3G10 / REDWideGamutRGB
    INPUT_LINEAR_ACESCG,           ///< Already in ACEScg (e.g. EXR)
    INPUT_LINEAR_SRGB              ///< sRGB / Rec.709 linear (e.g. PNG assets)
};

/**
 * \brief High-level Metal color pipeline that hides OCIO internals.
 *
 * Typical usage:
 * \code
 *   auto pipeline = MetalColorPipeline::Create(mtlDevice, WORKING_SPACE_ACESCCT);
 *
 *   // Build GPU shaders for each stage (call once, cache the results):
 *   auto idtShader  = pipeline->getIDTShader(INPUT_SLOG3_SGAMUT3CINE);
 *   auto odtShader  = pipeline->getODTShader(DISPLAY_SDR_sRGB);
 *   auto toLinShader = pipeline->getToLinearShader();   // working → ACEScg
 *   auto fromLinShader = pipeline->getFromLinearShader(); // ACEScg → working
 *
 *   // Feed each GpuShaderDescRcPtr into MetalBuilder::Create() as usual.
 * \endcode
 *
 * The "to/from linear" shaders are only needed when the working space is
 * non-linear (ACEScct).  For ACEScg they return identity processors.
 */
class MetalColorPipeline
{
public:
    MetalColorPipeline() = delete;
    MetalColorPipeline(const MetalColorPipeline &) = delete;
    MetalColorPipeline & operator=(const MetalColorPipeline &) = delete;

    /// Create a pipeline with the given Metal device and working space.
    static MetalColorPipelineRcPtr Create(id<MTLDevice> device,
                                          WorkingSpace workingSpace);

    /// Change working space (rebuilds internal processors).
    void setWorkingSpace(WorkingSpace ws);
    WorkingSpace getWorkingSpace() const;

    // -----------------------------------------------------------------
    //  GPU shader descriptors — feed these into MetalBuilder::Create()
    // -----------------------------------------------------------------

    /// Input → working space (IDT).
    GpuShaderDescRcPtr getIDTShader(InputEncoding input) const;

    /// Working space → display (ODT).
    GpuShaderDescRcPtr getODTShader(DisplayTarget display) const;

    /// Working space → ACEScg (linearize).
    /// Returns an identity shader when working space is already ACEScg.
    GpuShaderDescRcPtr getToLinearShader() const;

    /// ACEScg → working space (de-linearize).
    /// Returns an identity shader when working space is already ACEScg.
    GpuShaderDescRcPtr getFromLinearShader() const;

    /// Direct access to the underlying OCIO config (for advanced use).
    ConstConfigRcPtr getConfig() const;

    ~MetalColorPipeline();

private:
    MetalColorPipeline(id<MTLDevice> device, WorkingSpace ws);

    GpuShaderDescRcPtr buildShader(ConstProcessorRcPtr proc) const;

    static const char * workingSpaceName(WorkingSpace ws);
    static const char * inputEncodingName(InputEncoding input);
    static void displayTargetNames(DisplayTarget dt,
                                   const char *& displayName,
                                   const char *& viewName);

    struct Impl;
    Impl * m_impl;
};

} // namespace OCIO_NAMESPACE

#endif // INCLUDED_OCIO_METAL_COLOR_PIPELINE_H
