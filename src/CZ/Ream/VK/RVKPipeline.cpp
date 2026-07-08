#include <CZ/Ream/VK/RVKPipeline.h>
#include <CZ/Ream/VK/RVKDevice.h>
#include <CZ/Ream/RLog.h>
#include <cstdint>

using namespace CZ;

static const uint32_t painter_vert_spv[] = {
#include "painter_vert.spv.h"
};
static const uint32_t color_frag_spv[] = {
#include "color_frag.spv.h"
};
static const uint32_t image_frag_spv[] = {
#include "image_frag.spv.h"
};
static const uint32_t effect_frag_spv[] = {
#include "effect_frag.spv.h"
};

static UInt64 BlendHash(const RVKBlend &b) noexcept
{
    return (UInt64(b.enable ? 1 : 0) << 20) |
           (UInt64(b.srcColor) << 15) | (UInt64(b.dstColor) << 10) |
           (UInt64(b.srcAlpha) << 5)  |  UInt64(b.dstAlpha);
}

std::unique_ptr<RVKPipeline> RVKPipeline::Make(RVKDevice *device) noexcept
{
    auto p { std::unique_ptr<RVKPipeline>(new RVKPipeline(device)) };
    if (!p->init())
        return nullptr;
    return p;
}

VkShaderModule RVKPipeline::loadModule(const UInt32 *code, size_t bytes) noexcept
{
    VkShaderModuleCreateInfo ci {};
    ci.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    ci.codeSize = bytes;
    ci.pCode = code;
    VkShaderModule m { VK_NULL_HANDLE };
    if (vkCreateShaderModule(m_dev->device(), &ci, nullptr, &m) != VK_SUCCESS)
        RLog(CZError, CZLN, "RVKPipeline: vkCreateShaderModule failed");
    return m;
}

bool RVKPipeline::init() noexcept
{
    const VkDevice dev { m_dev->device() };

    m_vert = loadModule(painter_vert_spv, sizeof(painter_vert_spv));
    m_colorFrag = loadModule(color_frag_spv, sizeof(color_frag_spv));
    m_imageFrag = loadModule(image_frag_spv, sizeof(image_frag_spv));
    m_effectFrag = loadModule(effect_frag_spv, sizeof(effect_frag_spv));
    if (!m_vert || !m_colorFrag || !m_imageFrag || !m_effectFrag)
        return false;

    VkPipelineCacheCreateInfo cci {};
    cci.sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO;
    vkCreatePipelineCache(dev, &cci, nullptr, &m_cache);

    VkPushConstantRange pcr {};
    pcr.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    pcr.offset = 0;
    pcr.size = sizeof(RVKPushConstants);

    // Color pipeline layout: push constants only.
    {
        VkPipelineLayoutCreateInfo li {};
        li.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        li.pushConstantRangeCount = 1;
        li.pPushConstantRanges = &pcr;
        if (vkCreatePipelineLayout(dev, &li, nullptr, &m_colorLayout) != VK_SUCCESS)
            return false;
    }

    // Image descriptor set layout: image + mask combined image samplers.
    {
        VkDescriptorSetLayoutBinding bindings[2] {};
        for (UInt32 i = 0; i < 2; i++)
        {
            bindings[i].binding = i;
            bindings[i].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            bindings[i].descriptorCount = 1;
            bindings[i].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
        }
        VkDescriptorSetLayoutCreateInfo si {};
        si.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        si.bindingCount = 2;
        si.pBindings = bindings;
        if (vkCreateDescriptorSetLayout(dev, &si, nullptr, &m_imageSetLayout) != VK_SUCCESS)
            return false;

        VkPipelineLayoutCreateInfo li {};
        li.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        li.setLayoutCount = 1;
        li.pSetLayouts = &m_imageSetLayout;
        li.pushConstantRangeCount = 1;
        li.pPushConstantRanges = &pcr;
        if (vkCreatePipelineLayout(dev, &li, nullptr, &m_imageLayout) != VK_SUCCESS)
            return false;
    }

    return true;
}

VkRenderPass RVKPipeline::renderPass(VkFormat format) noexcept
{
    const auto it { m_renderPasses.find(format) };
    if (it != m_renderPasses.end())
        return it->second;

    VkAttachmentDescription color {};
    color.format = format;
    color.samples = VK_SAMPLE_COUNT_1_BIT;
    color.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;   // preserve existing content
    color.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    color.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    color.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    color.initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    color.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkAttachmentReference ref {};
    ref.attachment = 0;
    ref.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkSubpassDescription sub {};
    sub.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    sub.colorAttachmentCount = 1;
    sub.pColorAttachments = &ref;

    VkSubpassDependency deps[2] {};
    deps[0].srcSubpass = VK_SUBPASS_EXTERNAL;
    deps[0].dstSubpass = 0;
    deps[0].srcStageMask = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
    deps[0].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    deps[0].srcAccessMask = 0;
    deps[0].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_COLOR_ATTACHMENT_READ_BIT;
    deps[1].srcSubpass = 0;
    deps[1].dstSubpass = VK_SUBPASS_EXTERNAL;
    deps[1].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    deps[1].dstStageMask = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
    deps[1].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    deps[1].dstAccessMask = 0;

    VkRenderPassCreateInfo rp {};
    rp.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    rp.attachmentCount = 1;
    rp.pAttachments = &color;
    rp.subpassCount = 1;
    rp.pSubpasses = &sub;
    rp.dependencyCount = 2;
    rp.pDependencies = deps;

    VkRenderPass out { VK_NULL_HANDLE };
    if (vkCreateRenderPass(m_dev->device(), &rp, nullptr, &out) != VK_SUCCESS)
    {
        RLog(CZError, CZLN, "RVKPipeline: vkCreateRenderPass failed");
        return VK_NULL_HANDLE;
    }
    m_renderPasses.emplace(format, out);
    return out;
}

VkPipeline RVKPipeline::buildPipeline(VkRenderPass rp, int frag, const RVKBlend &blend, const void *specData, UInt32 specCount) noexcept
{
    VkSpecializationMapEntry specEntries[4] {};
    VkSpecializationInfo specInfo {};
    if (specData && specCount > 0)
    {
        for (UInt32 i = 0; i < specCount; i++)
        {
            specEntries[i].constantID = i;
            specEntries[i].offset = i * sizeof(UInt32);
            specEntries[i].size = sizeof(UInt32);
        }
        specInfo.mapEntryCount = specCount;
        specInfo.pMapEntries = specEntries;
        specInfo.dataSize = specCount * sizeof(UInt32);
        specInfo.pData = specData;
    }

    const bool usesSet { frag != 0 }; // image + effect use the sampler descriptor set

    VkPipelineShaderStageCreateInfo stages[2] {};
    stages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
    stages[0].module = m_vert;
    stages[0].pName = "main";
    stages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    stages[1].module = (frag == 2) ? m_effectFrag : (frag == 1) ? m_imageFrag : m_colorFrag;
    stages[1].pName = "main";
    if (specData && specCount > 0)
        stages[1].pSpecializationInfo = &specInfo;

    VkVertexInputBindingDescription bind {};
    bind.binding = 0;
    bind.stride = 6 * sizeof(float);
    bind.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    VkVertexInputAttributeDescription attrs[3] {};
    attrs[0] = { 0, 0, VK_FORMAT_R32G32_SFLOAT, 0 };
    attrs[1] = { 1, 0, VK_FORMAT_R32G32_SFLOAT, 2 * sizeof(float) };
    attrs[2] = { 2, 0, VK_FORMAT_R32G32_SFLOAT, 4 * sizeof(float) };

    VkPipelineVertexInputStateCreateInfo vi {};
    vi.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vi.vertexBindingDescriptionCount = 1;
    vi.pVertexBindingDescriptions = &bind;
    vi.vertexAttributeDescriptionCount = 3;
    vi.pVertexAttributeDescriptions = attrs;

    VkPipelineInputAssemblyStateCreateInfo ia {};
    ia.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    ia.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

    VkPipelineViewportStateCreateInfo vp {};
    vp.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    vp.viewportCount = 1;
    vp.scissorCount = 1;

    VkPipelineRasterizationStateCreateInfo rs {};
    rs.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rs.polygonMode = VK_POLYGON_MODE_FILL;
    rs.cullMode = VK_CULL_MODE_NONE;
    rs.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rs.lineWidth = 1.f;

    VkPipelineMultisampleStateCreateInfo ms {};
    ms.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    ms.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    VkPipelineColorBlendAttachmentState cb {};
    cb.blendEnable = blend.enable ? VK_TRUE : VK_FALSE;
    cb.srcColorBlendFactor = blend.srcColor;
    cb.dstColorBlendFactor = blend.dstColor;
    cb.colorBlendOp = VK_BLEND_OP_ADD;
    cb.srcAlphaBlendFactor = blend.srcAlpha;
    cb.dstAlphaBlendFactor = blend.dstAlpha;
    cb.alphaBlendOp = VK_BLEND_OP_ADD;
    cb.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

    VkPipelineColorBlendStateCreateInfo bs {};
    bs.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    bs.attachmentCount = 1;
    bs.pAttachments = &cb;

    VkDynamicState dyn[2] { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
    VkPipelineDynamicStateCreateInfo ds {};
    ds.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    ds.dynamicStateCount = 2;
    ds.pDynamicStates = dyn;

    VkGraphicsPipelineCreateInfo pi {};
    pi.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pi.stageCount = 2;
    pi.pStages = stages;
    pi.pVertexInputState = &vi;
    pi.pInputAssemblyState = &ia;
    pi.pViewportState = &vp;
    pi.pRasterizationState = &rs;
    pi.pMultisampleState = &ms;
    pi.pColorBlendState = &bs;
    pi.pDynamicState = &ds;
    pi.layout = usesSet ? m_imageLayout : m_colorLayout;
    pi.renderPass = rp;
    pi.subpass = 0;

    VkPipeline out { VK_NULL_HANDLE };
    if (vkCreateGraphicsPipelines(m_dev->device(), m_cache, 1, &pi, nullptr, &out) != VK_SUCCESS)
        RLog(CZError, CZLN, "RVKPipeline: vkCreateGraphicsPipelines failed");
    return out;
}

VkPipeline RVKPipeline::colorPipeline(VkRenderPass rp, VkFormat format, const RVKBlend &blend) noexcept
{
    // key layout: [0..31]=format  [32..33]=mode  [34..39]=payload  [40..]=blendHash
    const UInt64 key { UInt64(format) | (UInt64(0) << 32) | (BlendHash(blend) << 40) };
    const auto it { m_pipelines.find(key) };
    if (it != m_pipelines.end())
        return it->second;

    VkPipeline p { buildPipeline(rp, 0, blend, nullptr, 0) };
    if (p != VK_NULL_HANDLE)
        m_pipelines.emplace(key, p);
    return p;
}

VkPipeline RVKPipeline::imagePipeline(VkRenderPass rp, VkFormat format, const RVKBlend &blend, const RVKImageSpec &spec) noexcept
{
    const UInt64 key { UInt64(format) | (UInt64(1) << 32) | (UInt64(spec.pack()) << 34) | (BlendHash(blend) << 40) };
    const auto it { m_pipelines.find(key) };
    if (it != m_pipelines.end())
        return it->second;

    const UInt32 specData[4] { spec.hasMask, spec.replaceImageColor, spec.premultSrc, spec.blendDstIn };
    VkPipeline p { buildPipeline(rp, 1, blend, specData, 4) };
    if (p != VK_NULL_HANDLE)
        m_pipelines.emplace(key, p);
    return p;
}

VkPipeline RVKPipeline::effectPipeline(VkRenderPass rp, VkFormat format, UInt32 fx) noexcept
{
    const UInt64 key { UInt64(format) | (UInt64(2) << 32) | (UInt64(fx) << 34) };
    const auto it { m_pipelines.find(key) };
    if (it != m_pipelines.end())
        return it->second;

    const RVKBlend disabled { false, VK_BLEND_FACTOR_ONE, VK_BLEND_FACTOR_ZERO, VK_BLEND_FACTOR_ONE, VK_BLEND_FACTOR_ZERO };
    const UInt32 specData[1] { fx };
    VkPipeline p { buildPipeline(rp, 2, disabled, specData, 1) };
    if (p != VK_NULL_HANDLE)
        m_pipelines.emplace(key, p);
    return p;
}

VkSampler RVKPipeline::sampler(RImageFilter min, RImageFilter mag, RImageWrap wrapS, RImageWrap wrapT) noexcept
{
    const auto toAddr = [](RImageWrap w) -> VkSamplerAddressMode
    {
        switch (w)
        {
        case RImageWrap::Repeat:         return VK_SAMPLER_ADDRESS_MODE_REPEAT;
        case RImageWrap::MirroredRepeat: return VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT;
        case RImageWrap::ClampToBorder:  return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
        default:                         return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        }
    };

    const UInt32 key { UInt32(min) | (UInt32(mag) << 1) | (UInt32(wrapS) << 2) | (UInt32(wrapT) << 4) };
    const auto it { m_samplers.find(key) };
    if (it != m_samplers.end())
        return it->second;

    VkSamplerCreateInfo si {};
    si.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    si.minFilter = (min == RImageFilter::Linear) ? VK_FILTER_LINEAR : VK_FILTER_NEAREST;
    si.magFilter = (mag == RImageFilter::Linear) ? VK_FILTER_LINEAR : VK_FILTER_NEAREST;
    si.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
    si.addressModeU = toAddr(wrapS);
    si.addressModeV = toAddr(wrapT);
    si.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    si.borderColor = VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK;
    si.maxLod = 0.f;

    VkSampler s { VK_NULL_HANDLE };
    if (vkCreateSampler(m_dev->device(), &si, nullptr, &s) != VK_SUCCESS)
    {
        RLog(CZError, CZLN, "RVKPipeline: vkCreateSampler failed");
        return VK_NULL_HANDLE;
    }
    m_samplers.emplace(key, s);
    return s;
}

RVKPipeline::~RVKPipeline() noexcept
{
    const VkDevice dev { m_dev->device() };
    if (dev == VK_NULL_HANDLE)
        return;

    for (auto &[k, p] : m_pipelines) vkDestroyPipeline(dev, p, nullptr);
    for (auto &[k, r] : m_renderPasses) vkDestroyRenderPass(dev, r, nullptr);
    for (auto &[k, s] : m_samplers) vkDestroySampler(dev, s, nullptr);
    if (m_imageLayout) vkDestroyPipelineLayout(dev, m_imageLayout, nullptr);
    if (m_colorLayout) vkDestroyPipelineLayout(dev, m_colorLayout, nullptr);
    if (m_imageSetLayout) vkDestroyDescriptorSetLayout(dev, m_imageSetLayout, nullptr);
    if (m_cache) vkDestroyPipelineCache(dev, m_cache, nullptr);
    if (m_vert) vkDestroyShaderModule(dev, m_vert, nullptr);
    if (m_colorFrag) vkDestroyShaderModule(dev, m_colorFrag, nullptr);
    if (m_imageFrag) vkDestroyShaderModule(dev, m_imageFrag, nullptr);
    if (m_effectFrag) vkDestroyShaderModule(dev, m_effectFrag, nullptr);
}
