#include "pipeline.h"
#include "utils.h"
#include "vertex.h"

VkViewport make_viewport(VkExtent2D extent) {
    VkViewport viewport = {
        .x = 1.0f, .y = 1.0f, .width = extent.width, .height = extent.height, .minDepth = 0.0f, .maxDepth = 1.0f};
    return viewport;
}

VkRect2D make_scissor(VkExtent2D extent, uint32_t left, uint32_t right, uint32_t up, uint32_t down) {
    left = MIN(left, extent.width);
    right = MIN(right, extent.width);
    up = MIN(up, extent.height);
    down = MIN(down, extent.height);
    VkOffset2D offset = {.x = left, .y = up};
    VkExtent2D newExtent = {.width = extent.width - left - right, .height = extent.height - up - down};
    VkRect2D scissor = {.offset = offset, .extent = newExtent};
    return scissor;
}

VkPipeline create_graphics_pipeline(
    VkDevice device, VkPipelineLayoutCreateInfo cInfo, VkShaderModule *shaderModules,
    VkShaderStageFlagBits *shaderStageFlags, uint32_t shaderModuleCount, VkRenderPass renderPass,
    VkPipelineLayout *out_pipelineLayout) {
    VkPipelineLayout pipelineLayout;

    vkCreatePipelineLayout(device, &cInfo, VK_NULL_HANDLE, &pipelineLayout);

    char const entryName[] = "main"; // maybe pipeline name for debug
    ARR_ALLOC(VkPipelineShaderStageCreateInfo, stages, shaderModuleCount);
    for (uint32_t i = 0; i < shaderModuleCount; i++) {
        stages[i] = (VkPipelineShaderStageCreateInfo){
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .pNext = VK_NULL_HANDLE,
            .flags = 0,
            .stage = shaderStageFlags[i],
            .module = shaderModules[i],
            .pName = entryName,
            .pSpecializationInfo = VK_NULL_HANDLE};
    }

    // todo: extract to arguments

    VkVertexInputBindingDescription bindDesc = {
        .binding = 0, .stride = sizeof(Vertex), .inputRate = VK_VERTEX_INPUT_RATE_VERTEX};

    VkVertexInputAttributeDescription attrDesc[2] = {
        {.binding = 0, .location = 0,    .format = VK_FORMAT_R32G32_SFLOAT, .offset = offsetof(Vertex,   pos)},
        {.binding = 0, .location = 1, .format = VK_FORMAT_R32G32B32_SFLOAT, .offset = offsetof(Vertex, color)},
    };

    VkPipelineVertexInputStateCreateInfo plVISCInfo = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
        .pNext = VK_NULL_HANDLE,
        .flags = 0,
        .vertexBindingDescriptionCount = 1,
        .pVertexBindingDescriptions = &bindDesc,
        .vertexAttributeDescriptionCount = 2,
        .pVertexAttributeDescriptions = attrDesc};
    VkPipelineInputAssemblyStateCreateInfo plIASCInfo = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
        .pNext = VK_NULL_HANDLE,
        .flags = 0,
        .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
        .primitiveRestartEnable = VK_FALSE};

    VkPipelineViewportStateCreateInfo plVSCInfo = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
        .pNext = VK_NULL_HANDLE,
        .flags = 0,
        .viewportCount = 1,
        .pViewports = VK_NULL_HANDLE, // dynamic
        .scissorCount = 1,
        .pScissors = VK_NULL_HANDLE // dynamic
    };
    VkPipelineRasterizationStateCreateInfo plRSCInfo = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
        .pNext = VK_NULL_HANDLE,
        .flags = 0,
        .depthClampEnable = VK_FALSE,
        .rasterizerDiscardEnable = VK_FALSE,
        .polygonMode = VK_POLYGON_MODE_FILL,
        .cullMode = VK_CULL_MODE_BACK_BIT,
        .frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE,
        .depthBiasEnable = VK_FALSE,
        .depthBiasConstantFactor = 0.0f,
        .depthBiasClamp = 0.0f,
        .depthBiasSlopeFactor = 0.0f,
        .lineWidth = 1.0f};

    VkPipelineMultisampleStateCreateInfo plMSCInfo = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
        .pNext = VK_NULL_HANDLE,
        .flags = 0,
        .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
        .sampleShadingEnable = VK_FALSE,
        .minSampleShading = 1.0f,
        .pSampleMask = VK_NULL_HANDLE,
        .alphaToCoverageEnable = VK_FALSE,
        .alphaToOneEnable = VK_FALSE};
    VkPipelineColorBlendAttachmentState plCBAS = {
        .blendEnable = VK_FALSE,
        .srcColorBlendFactor = VK_BLEND_FACTOR_ONE,
        .dstColorBlendFactor = VK_BLEND_FACTOR_ZERO,
        .colorBlendOp = VK_BLEND_OP_ADD,
        .srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE,
        .dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO,
        .alphaBlendOp = VK_BLEND_OP_ADD,
        .colorWriteMask =
            VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT};
    VkPipelineColorBlendStateCreateInfo plCBSCInfo = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
        .pNext = VK_NULL_HANDLE,
        .flags = 0,
        .logicOpEnable = VK_FALSE,
        .logicOp = VK_LOGIC_OP_COPY,
        .attachmentCount = 1,
        .pAttachments = &plCBAS,
        .blendConstants = {0.0f, 0.0f, 0.0f, 0.0f}
    };
    // dynamic state for viewport & scissor resize
    VkPipelineDynamicStateCreateInfo plDSCInfo = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
        .pNext = VK_NULL_HANDLE,
        .flags = 0,
        .dynamicStateCount = 2,
        .pDynamicStates = (VkDynamicState[]){VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR}
    };
    VkGraphicsPipelineCreateInfo gpCInfo = {
        .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
        .pNext = VK_NULL_HANDLE,
        .flags = 0,
        .stageCount = shaderModuleCount,
        .pStages = stages,
        .pVertexInputState = &plVISCInfo,
        .pInputAssemblyState = &plIASCInfo,
        .pTessellationState = VK_NULL_HANDLE,
        .pViewportState = &plVSCInfo,
        .pRasterizationState = &plRSCInfo,
        .pMultisampleState = &plMSCInfo,
        .pDepthStencilState = VK_NULL_HANDLE,
        .pColorBlendState = &plCBSCInfo,
        .pDynamicState = &plDSCInfo,
        .layout = pipelineLayout,
        .renderPass = renderPass,
        .subpass = 0,
        .basePipelineHandle = VK_NULL_HANDLE,
        .basePipelineIndex = -1,
    };

    VkPipeline pipeline;
    vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &gpCInfo, VK_NULL_HANDLE, &pipeline);

    *out_pipelineLayout = pipelineLayout;
    return pipeline;
}
