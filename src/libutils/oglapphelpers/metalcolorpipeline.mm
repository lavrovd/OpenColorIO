// SPDX-License-Identifier: BSD-3-Clause
// Copyright Contributors to the OpenColorIO Project.

#include <stdexcept>
#include <string>

#include "metalcolorpipeline.h"

namespace OCIO_NAMESPACE
{

// Use the studio config — it has camera IDTs (Sony, ARRI, RED, Panasonic, etc.)
// The CG config only has output color spaces.
static constexpr const char * kBuiltinConfig = "ocio://studio-config-latest";

struct MetalColorPipeline::Impl
{
    id<MTLDevice>    device;
    WorkingSpace     workingSpace;
    ConstConfigRcPtr config;
};

// ---------------------------------------------------------------------------
//  Name mapping
// ---------------------------------------------------------------------------

const char * MetalColorPipeline::workingSpaceName(WorkingSpace ws)
{
    switch (ws)
    {
        case WORKING_SPACE_ACESCG:  return "ACEScg";
        case WORKING_SPACE_ACESCCT: return "ACEScct";
    }
    throw Exception("Unknown WorkingSpace value.");
}

const char * MetalColorPipeline::inputEncodingName(InputEncoding input)
{
    switch (input)
    {
        case INPUT_SLOG3_SGAMUT3CINE:   return "S-Log3 S-Gamut3.Cine";
        case INPUT_LOGC4_WIDE_GAMUT_4:  return "ARRI LogC4";
        case INPUT_LOGC3_WIDE_GAMUT_3:  return "ARRI LogC3 (EI800)";
        case INPUT_VLOG_VGAMUT:         return "V-Log V-Gamut";
        case INPUT_LOG3G10_RWGRGB:      return "Log3G10 REDWideGamutRGB";
        case INPUT_LINEAR_ACESCG:       return "ACEScg";
        case INPUT_LINEAR_SRGB:         return "sRGB Encoded Rec.709 (sRGB)";
    }
    throw Exception("Unknown InputEncoding value.");
}

void MetalColorPipeline::displayTargetNames(DisplayTarget dt,
                                             const char *& displayName,
                                             const char *& viewName)
{
    switch (dt)
    {
        case DISPLAY_SDR_sRGB:
            displayName = "sRGB - Display";
            viewName    = "ACES 2.0 - SDR 100 nits (Rec.709)";
            return;
        case DISPLAY_SDR_P3:
            displayName = "Display P3 - Display";
            viewName    = "ACES 2.0 - SDR 100 nits (P3 D65)";
            return;
        case DISPLAY_HDR_1000_P3:
            displayName = "Display P3 HDR - Display";
            viewName    = "ACES 2.0 - HDR 1000 nits (P3 D65)";
            return;
        case DISPLAY_HDR_1000_REC2020:
            displayName = "Rec.2100-PQ - Display";
            viewName    = "ACES 2.0 - HDR 1000 nits (Rec.2020)";
            return;
    }
    throw Exception("Unknown DisplayTarget value.");
}

// ---------------------------------------------------------------------------
//  Construction
// ---------------------------------------------------------------------------

MetalColorPipelineRcPtr MetalColorPipeline::Create(id<MTLDevice> device,
                                                    WorkingSpace workingSpace)
{
    return MetalColorPipelineRcPtr(new MetalColorPipeline(device, workingSpace));
}

MetalColorPipeline::MetalColorPipeline(id<MTLDevice> device, WorkingSpace ws)
    : m_impl(new Impl)
{
    m_impl->device       = device;
    m_impl->workingSpace = ws;
    m_impl->config       = Config::CreateFromBuiltinConfig(kBuiltinConfig);
}

MetalColorPipeline::~MetalColorPipeline()
{
    delete m_impl;
}

// ---------------------------------------------------------------------------
//  Accessors
// ---------------------------------------------------------------------------

void MetalColorPipeline::setWorkingSpace(WorkingSpace ws)
{
    m_impl->workingSpace = ws;
}

WorkingSpace MetalColorPipeline::getWorkingSpace() const
{
    return m_impl->workingSpace;
}

ConstConfigRcPtr MetalColorPipeline::getConfig() const
{
    return m_impl->config;
}

// ---------------------------------------------------------------------------
//  Shader helpers
// ---------------------------------------------------------------------------

GpuShaderDescRcPtr MetalColorPipeline::buildShader(ConstProcessorRcPtr proc) const
{
    auto gpuProc = proc->getOptimizedGPUProcessor(OPTIMIZATION_DEFAULT);

    auto shaderDesc = GpuShaderDesc::CreateShaderDesc();
    shaderDesc->setLanguage(GPU_LANGUAGE_MSL_2_0);
    gpuProc->extractGpuShaderInfo(shaderDesc);

    return shaderDesc;
}

// ---------------------------------------------------------------------------
//  IDT:  input encoding → working space
// ---------------------------------------------------------------------------

GpuShaderDescRcPtr MetalColorPipeline::getIDTShader(InputEncoding input) const
{
    const char * src = inputEncodingName(input);
    const char * dst = workingSpaceName(m_impl->workingSpace);

    auto proc = m_impl->config->getProcessor(src, dst);
    return buildShader(proc);
}

// ---------------------------------------------------------------------------
//  ODT:  working space → display
// ---------------------------------------------------------------------------

GpuShaderDescRcPtr MetalColorPipeline::getODTShader(DisplayTarget display) const
{
    const char * displayName = nullptr;
    const char * viewName    = nullptr;
    displayTargetNames(display, displayName, viewName);

    const char * ws = workingSpaceName(m_impl->workingSpace);

    // DisplayViewHelpers adds exposure/contrast dynamic properties automatically.
    auto proc = DisplayViewHelpers::GetProcessor(
        m_impl->config,
        ws,
        displayName,
        viewName,
        MatrixTransform::Create(),  // identity channel view
        TRANSFORM_DIR_FORWARD);

    return buildShader(proc);
}

// ---------------------------------------------------------------------------
//  Linearize / de-linearize (ACEScct ↔ ACEScg)
// ---------------------------------------------------------------------------

GpuShaderDescRcPtr MetalColorPipeline::getToLinearShader() const
{
    const char * ws = workingSpaceName(m_impl->workingSpace);

    // working space → ACEScg  (identity when already ACEScg)
    auto proc = m_impl->config->getProcessor(ws, "ACEScg");
    return buildShader(proc);
}

GpuShaderDescRcPtr MetalColorPipeline::getFromLinearShader() const
{
    const char * ws = workingSpaceName(m_impl->workingSpace);

    // ACEScg → working space  (identity when already ACEScg)
    auto proc = m_impl->config->getProcessor("ACEScg", ws);
    return buildShader(proc);
}

} // namespace OCIO_NAMESPACE
