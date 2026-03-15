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

/// Camera / input encoding.  This is the only thing the user picks.
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
 * The user only selects the camera / input encoding (IDT).  The output
 * transform is derived automatically: after grading the plugin converts
 * back to the same color space the input arrived in, so the host's own
 * display pipeline (viewer LUT, monitor profile, etc.) stays in control.
 *
 * Typical usage:
 * \code
 *   // One-time setup — user picks camera format only.
 *   auto pipeline = MetalColorPipeline::Create(mtlDevice,
 *                                              WORKING_SPACE_ACESCCT,
 *                                              INPUT_SLOG3_SGAMUT3CINE);
 *
 *   // Build GPU shaders (cache the results, rebuild only when IDT changes):
 *   auto idtShader     = pipeline->getIDTShader();      // input → working
 *   auto inverseShader = pipeline->getInverseIDTShader(); // working → input
 *
 *   // For CIFilters that need linear input, sandwich them:
 *   auto toLinShader   = pipeline->getToLinearShader();   // working → ACEScg
 *   auto fromLinShader = pipeline->getFromLinearShader(); // ACEScg → working
 *
 *   // Feed each GpuShaderDescRcPtr into MetalBuilder::Create().
 * \endcode
 *
 * Pipeline for a plugin render pass:
 *   input pixels (camera space)
 *     → IDT shader (camera → working space)
 *       → [grade / CIFilter chain in working space]
 *     → inverse IDT shader (working space → camera space)
 *   output pixels (same space the host sent — host handles display)
 *
 * The "to/from linear" shaders are only needed when the working space is
 * non-linear (ACEScct).  For ACEScg they compile to identity (no-op).
 */
class MetalColorPipeline
{
public:
    MetalColorPipeline() = delete;
    MetalColorPipeline(const MetalColorPipeline &) = delete;
    MetalColorPipeline & operator=(const MetalColorPipeline &) = delete;

    /// Create a pipeline.  User picks working space and camera format.
    static MetalColorPipelineRcPtr Create(id<MTLDevice> device,
                                          WorkingSpace workingSpace,
                                          InputEncoding input);

    /// Change the camera / input encoding (rebuilds IDT processors).
    void setInputEncoding(InputEncoding input);
    InputEncoding getInputEncoding() const;

    /// Change working space (rebuilds all processors).
    void setWorkingSpace(WorkingSpace ws);
    WorkingSpace getWorkingSpace() const;

    // -----------------------------------------------------------------
    //  GPU shader descriptors — feed these into MetalBuilder::Create()
    // -----------------------------------------------------------------

    /// Input (camera) → working space.
    GpuShaderDescRcPtr getIDTShader() const;

    /// Working space → input (camera).  Returns pixels in the same
    /// color space the host originally sent, so the host's display
    /// pipeline sees the correct data.
    GpuShaderDescRcPtr getInverseIDTShader() const;

    /// Working space → ACEScg (linearize for filters that need it).
    /// Compiles to identity when working space is already ACEScg.
    GpuShaderDescRcPtr getToLinearShader() const;

    /// ACEScg → working space (de-linearize after linear filters).
    /// Compiles to identity when working space is already ACEScg.
    GpuShaderDescRcPtr getFromLinearShader() const;

    /// Direct access to the underlying OCIO config (escape hatch).
    ConstConfigRcPtr getConfig() const;

    ~MetalColorPipeline();

private:
    MetalColorPipeline(id<MTLDevice> device, WorkingSpace ws, InputEncoding input);

    GpuShaderDescRcPtr buildShader(ConstProcessorRcPtr proc) const;

    static const char * workingSpaceName(WorkingSpace ws);
    static const char * inputEncodingName(InputEncoding input);

    struct Impl;
    Impl * m_impl;
};

} // namespace OCIO_NAMESPACE

#endif // INCLUDED_OCIO_METAL_COLOR_PIPELINE_H
