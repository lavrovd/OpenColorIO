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

/// Camera / input encoding.  The only thing the user selects.
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

/// Color space of the pixels that FCP delivers to the plugin.
/// Derived from the host — not user-selectable.
enum HostColorSpace
{
    HOST_REC709 = 0,               ///< Rec.709 / sRGB (SDR timeline)
    HOST_REC2020                   ///< Rec.2020 (Wide Gamut / HDR timeline)
};

/**
 * \brief High-level Metal color pipeline for FxPlug4 plugins.
 *
 * FCP decodes the media and delivers pixels to the plugin in Rec.709
 * or Rec.2020 (non-linearized).  The user's only choice is the camera
 * IDT.  The output transform converts back to the host color space
 * so FCP's display pipeline stays in control.
 *
 * Typical usage in a FxPlug4 plugin:
 * \code
 *   // Setup — user picked camera, host tells us Rec.709 or Rec.2020.
 *   auto pipeline = MetalColorPipeline::Create(mtlDevice,
 *                                              WORKING_SPACE_ACESCCT,
 *                                              INPUT_SLOG3_SGAMUT3CINE,
 *                                              HOST_REC709);
 *
 *   // Build GPU shaders (cache; rebuild when IDT or host space changes):
 *   auto idtShader = pipeline->getIDTShader();       // camera → working
 *   auto outShader = pipeline->getOutputShader();     // working → host
 *
 *   // For CIFilters that need linear light:
 *   auto toLinShader   = pipeline->getToLinearShader();
 *   auto fromLinShader = pipeline->getFromLinearShader();
 *
 *   // Feed each GpuShaderDescRcPtr into MetalBuilder::Create().
 * \endcode
 *
 * Full render-pass pipeline:
 *   FCP pixels (Rec.709 or Rec.2020)
 *     → IDT shader (camera → working space)
 *       → [grade / CIFilter chain in working space]
 *     → output shader (working space → Rec.709 or Rec.2020)
 *   return to FCP (host handles display)
 *
 * The "to/from linear" shaders are only needed when the working space
 * is non-linear (ACEScct).  For ACEScg they compile to identity.
 */
class MetalColorPipeline
{
public:
    MetalColorPipeline() = delete;
    MetalColorPipeline(const MetalColorPipeline &) = delete;
    MetalColorPipeline & operator=(const MetalColorPipeline &) = delete;

    /// Create a pipeline.
    /// @param device        Metal device.
    /// @param workingSpace  Internal grading space (ACEScg or ACEScct).
    /// @param input         Camera encoding — the user picks this.
    /// @param hostSpace     What FCP delivered — derived from the host.
    static MetalColorPipelineRcPtr Create(id<MTLDevice> device,
                                          WorkingSpace workingSpace,
                                          InputEncoding input,
                                          HostColorSpace hostSpace);

    /// Change the camera / input encoding (rebuilds IDT shader).
    void setInputEncoding(InputEncoding input);
    InputEncoding getInputEncoding() const;

    /// Change the host color space (rebuilds output shader).
    void setHostColorSpace(HostColorSpace hs);
    HostColorSpace getHostColorSpace() const;

    /// Change working space (rebuilds all shaders).
    void setWorkingSpace(WorkingSpace ws);
    WorkingSpace getWorkingSpace() const;

    // -----------------------------------------------------------------
    //  GPU shader descriptors — feed these into MetalBuilder::Create()
    // -----------------------------------------------------------------

    /// Camera → working space (IDT).
    /// User-selected camera encoding to internal grading space.
    GpuShaderDescRcPtr getIDTShader() const;

    /// Working space → host color space (output).
    /// Converts graded pixels back to whatever FCP expects (Rec.709
    /// or Rec.2020).  Derived automatically — not user-selectable.
    GpuShaderDescRcPtr getOutputShader() const;

    /// Working space → ACEScg (linearize for CIFilters).
    /// Identity when working space is already ACEScg.
    GpuShaderDescRcPtr getToLinearShader() const;

    /// ACEScg → working space (de-linearize after CIFilters).
    /// Identity when working space is already ACEScg.
    GpuShaderDescRcPtr getFromLinearShader() const;

    /// Direct access to the underlying OCIO config.
    ConstConfigRcPtr getConfig() const;

    ~MetalColorPipeline();

private:
    MetalColorPipeline(id<MTLDevice> device, WorkingSpace ws,
                       InputEncoding input, HostColorSpace hostSpace);

    GpuShaderDescRcPtr buildShader(ConstProcessorRcPtr proc) const;

    static const char * workingSpaceName(WorkingSpace ws);
    static const char * inputEncodingName(InputEncoding input);
    static const char * hostColorSpaceName(HostColorSpace hs);

    struct Impl;
    Impl * m_impl;
};

} // namespace OCIO_NAMESPACE

#endif // INCLUDED_OCIO_METAL_COLOR_PIPELINE_H
