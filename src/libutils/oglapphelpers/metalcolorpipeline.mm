// SPDX-License-Identifier: BSD-3-Clause
// Copyright Contributors to the OpenColorIO Project.

#include <stdexcept>
#include <string>

#include "metalcolorpipeline.h"

namespace OCIO_NAMESPACE
{

// Studio config has camera IDTs (Sony, ARRI, RED, Panasonic, etc.)
static constexpr const char * kBuiltinConfig = "ocio://studio-config-latest";

struct MetalColorPipeline::Impl
{
    id<MTLDevice>    device;
    WorkingSpace     workingSpace;
    InputEncoding    inputEncoding;
    HostColorSpace   hostColorSpace;
    ConstConfigRcPtr config;
};

// ---------------------------------------------------------------------------
//  Name mapping — all OCIO string knowledge lives here
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

const char * MetalColorPipeline::hostColorSpaceName(HostColorSpace hs)
{
    switch (hs)
    {
        case HOST_REC709:   return "Rec.1886 Rec.709 - Display";
        case HOST_REC2020:  return "Rec.1886 Rec.2020 - Display";
    }
    throw Exception("Unknown HostColorSpace value.");
}

// ---------------------------------------------------------------------------
//  Construction
// ---------------------------------------------------------------------------

MetalColorPipelineRcPtr MetalColorPipeline::Create(id<MTLDevice> device,
                                                    WorkingSpace workingSpace,
                                                    InputEncoding input,
                                                    HostColorSpace hostSpace)
{
    return MetalColorPipelineRcPtr(
        new MetalColorPipeline(device, workingSpace, input, hostSpace));
}

MetalColorPipeline::MetalColorPipeline(id<MTLDevice> device,
                                       WorkingSpace ws,
                                       InputEncoding input,
                                       HostColorSpace hostSpace)
    : m_impl(new Impl)
{
    m_impl->device         = device;
    m_impl->workingSpace   = ws;
    m_impl->inputEncoding  = input;
    m_impl->hostColorSpace = hostSpace;
    m_impl->config         = Config::CreateFromBuiltinConfig(kBuiltinConfig);
}

MetalColorPipeline::~MetalColorPipeline()
{
    delete m_impl;
}

// ---------------------------------------------------------------------------
//  Accessors
// ---------------------------------------------------------------------------

void MetalColorPipeline::setInputEncoding(InputEncoding input)
{
    m_impl->inputEncoding = input;
}

InputEncoding MetalColorPipeline::getInputEncoding() const
{
    return m_impl->inputEncoding;
}

void MetalColorPipeline::setHostColorSpace(HostColorSpace hs)
{
    m_impl->hostColorSpace = hs;
}

HostColorSpace MetalColorPipeline::getHostColorSpace() const
{
    return m_impl->hostColorSpace;
}

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
//  IDT:  camera → working space
// ---------------------------------------------------------------------------

GpuShaderDescRcPtr MetalColorPipeline::getIDTShader() const
{
    const char * src = inputEncodingName(m_impl->inputEncoding);
    const char * dst = workingSpaceName(m_impl->workingSpace);

    auto proc = m_impl->config->getProcessor(src, dst);
    return buildShader(proc);
}

// ---------------------------------------------------------------------------
//  Output:  working space → host color space (Rec.709 or Rec.2020)
// ---------------------------------------------------------------------------

GpuShaderDescRcPtr MetalColorPipeline::getOutputShader() const
{
    const char * src = workingSpaceName(m_impl->workingSpace);
    const char * dst = hostColorSpaceName(m_impl->hostColorSpace);

    auto proc = m_impl->config->getProcessor(src, dst);
    return buildShader(proc);
}

// ---------------------------------------------------------------------------
//  Linearize / de-linearize  (working space ↔ ACEScg)
// ---------------------------------------------------------------------------

GpuShaderDescRcPtr MetalColorPipeline::getToLinearShader() const
{
    const char * ws = workingSpaceName(m_impl->workingSpace);

    auto proc = m_impl->config->getProcessor(ws, "ACEScg");
    return buildShader(proc);
}

GpuShaderDescRcPtr MetalColorPipeline::getFromLinearShader() const
{
    const char * ws = workingSpaceName(m_impl->workingSpace);

    auto proc = m_impl->config->getProcessor("ACEScg", ws);
    return buildShader(proc);
}

} // namespace OCIO_NAMESPACE
