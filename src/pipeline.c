#include "pipeline.h"
#include "shader.h"
#include "utils.h"
#include "vertex.h"

VkViewport make_viewport(VkExtent2D extent) {
    VkViewport viewport = {
        .x = 1.0f,
        .y = 1.0f,
        .width = extent.width,
        .height = extent.height,
        .minDepth = 0.0f,
        .maxDepth = 1.0f};
    return viewport;
}

VkRect2D make_scissor(
    VkExtent2D extent, uint32_t left, uint32_t right, uint32_t up, uint32_t down) {
    left = MIN(left, extent.width);
    right = MIN(right, extent.width);
    up = MIN(up, extent.height);
    down = MIN(down, extent.height);
    VkOffset2D offset = {.x = left, .y = up};
    VkExtent2D newExtent = {
        .width = extent.width - left - right, .height = extent.height - up - down};
    VkRect2D scissor = {.offset = offset, .extent = newExtent};
    return scissor;
}

VkPipelineRasterizationStateCreateInfo APipeline_default_rasterizer() {
    return (VkPipelineRasterizationStateCreateInfo){
        .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
        .depthClampEnable = VK_TRUE,
        .rasterizerDiscardEnable = VK_TRUE,
        .polygonMode = VK_POLYGON_MODE_FILL,
        .cullMode = VK_CULL_MODE_BACK_BIT,
        .frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE,
        .depthBiasEnable = VK_FALSE,
        .lineWidth = 1.0f};
}

VkPipelineMultisampleStateCreateInfo APipeline_default_multisampler() {
    return (VkPipelineMultisampleStateCreateInfo){
        .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
        .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
        .sampleShadingEnable = VK_FALSE,
        .minSampleShading = 1.0f,
        .alphaToCoverageEnable = VK_FALSE,
        .alphaToOneEnable = VK_FALSE};
}

VkPipelineColorBlendAttachmentState *APipeline_default_attachments(uint32_t *out_count) {
    *out_count = 1;
    ARR_ALLOC(VkPipelineColorBlendAttachmentState, result, 1);
    result[0] = (VkPipelineColorBlendAttachmentState){
        .blendEnable = VK_FALSE,
        .srcColorBlendFactor = VK_BLEND_FACTOR_ONE,
        .dstColorBlendFactor = VK_BLEND_FACTOR_ZERO,
        .colorBlendOp = VK_BLEND_OP_ADD,
        .srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE,
        .dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO,
        .alphaBlendOp = VK_BLEND_OP_ADD,
        .colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                          VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT};
    return result;
}

APipelineParams APipeline_default() {
    ARR_ALLOC(VkVertexInputBindingDescription, bindings, 1);
    bindings[0] = Vertex_binding(0);
    APipelineParams result = {
        .bindingCount = 1,
        .bindings = bindings,
        .rasterizationParams = APipeline_default_rasterizer(),
        .multisampleParams = APipeline_default_multisampler(),
        .colorBlendLogicOpEnable = VK_FALSE,
        .colorBlendLogicOp = VK_LOGIC_OP_COPY,
        .colorBlendConstants = {0.f, 0.f, 0.f, 0.f}
    };
    result.attributes = Vertex_attributes(0, &result.attributeCount);
    result.colorBlendAttachments = APipeline_default_attachments(&result.colorBlendAttachmentCount);
    return result;
}

void APipelineParams_free(APipelineParams args) {
    free(args.bindings);
    free(args.attributes);
    free(args.colorBlendAttachments);
}

VkPipelineLayout create_pipeline_layout(
    VkDevice device, uint32_t setLayoutCount, VkDescriptorSetLayout const *setLayouts,
    uint32_t pushConstantRangeCount, VkPushConstantRange const *pushConstantRanges) {
    VkPipelineLayoutCreateInfo createInfo = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .setLayoutCount = setLayoutCount,
        .pSetLayouts = setLayouts,
        .pushConstantRangeCount = pushConstantRangeCount,
        .pPushConstantRanges = pushConstantRanges};
    VkPipelineLayout pipelineLayout;
    vkCreatePipelineLayout(device, &createInfo, NULL, &pipelineLayout);
    return pipelineLayout;
}

VkPipeline create_pipeline(
    VkDevice device, VkPipelineLayout pipelineLayout, VkRenderPass renderPass,
    char const *entryPointGroup, uint32_t shaderCount, AShader const *shaders,
    APipelineParams args) {
    ARR_ALLOC(VkPipelineShaderStageCreateInfo, stages, shaderCount);
    for (uint32_t i = 0; i < shaderCount; i++) {
        stages[i] = (VkPipelineShaderStageCreateInfo){
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .stage = shaders[i].stage,
            .module = shaders[i].module,
            .pName = entryPointGroup};
    }
    VkPipelineVertexInputStateCreateInfo vertexInputState = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
        .vertexBindingDescriptionCount = args.bindingCount,
        .pVertexBindingDescriptions = args.bindings,
        .vertexAttributeDescriptionCount = args.attributeCount,
        .pVertexAttributeDescriptions = args.attributes};
    VkPipelineInputAssemblyStateCreateInfo inputAssemblyState = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
        .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
        .primitiveRestartEnable = VK_FALSE};

    VkPipelineViewportStateCreateInfo viewportState = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
        .viewportCount = 1,
        .scissorCount = 1,
    };
    VkPipelineRasterizationStateCreateInfo rasterizationState = args.rasterizationParams;

    VkPipelineMultisampleStateCreateInfo multisapleState = args.multisampleParams;
    VkPipelineColorBlendAttachmentState const *colorBlendAttachmentState =
        args.colorBlendAttachments;
    VkPipelineColorBlendStateCreateInfo colorBlendState = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
        .logicOpEnable = args.colorBlendLogicOpEnable,
        .logicOp = args.colorBlendLogicOp,
        .attachmentCount = args.colorBlendAttachmentCount,
        .pAttachments = colorBlendAttachmentState,
    };
    colorBlendState.blendConstants[0] = args.colorBlendConstants[0];
    colorBlendState.blendConstants[1] = args.colorBlendConstants[1];
    colorBlendState.blendConstants[2] = args.colorBlendConstants[2];
    colorBlendState.blendConstants[3] = args.colorBlendConstants[3];
    // dynamic state for viewport & scissor resize
    VkPipelineDynamicStateCreateInfo dynamicState = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
        .dynamicStateCount = 2,
        .pDynamicStates = (VkDynamicState[]){VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR}
    };
    VkGraphicsPipelineCreateInfo pipelineCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
        .stageCount = shaderCount,
        .pStages = stages,
        .pVertexInputState = &vertexInputState,
        .pInputAssemblyState = &inputAssemblyState,
        .pTessellationState = NULL,
        .pViewportState = &viewportState,
        .pRasterizationState = &rasterizationState,
        .pMultisampleState = &multisapleState,
        .pDepthStencilState = NULL,
        .pColorBlendState = &colorBlendState,
        .pDynamicState = &dynamicState,
        .layout = pipelineLayout,
        .renderPass = renderPass,
        .subpass = 0,
        .basePipelineHandle = NULL,
        .basePipelineIndex = -1,
    };

    VkPipeline pipeline;
    vkCreateGraphicsPipelines(device, NULL, 1, &pipelineCreateInfo, NULL, &pipeline);

    return pipeline;
}
