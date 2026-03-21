#pragma once

#include "bodoq_engine/input.h"
#include "bodoq_engine/window.h"
#include "bodoq_engine/gpu.h"
#include "bodoq_engine/function.h"
#include "bodoq_engine/render_graph.h"
#include "imgui/imgui.h"

struct ImGuiRenderer
{
	static ImGuiKey mapKeyCode(KeyCode keyCode);

	ImGuiRenderer(const Window* window);
	~ImGuiRenderer();

	void beginFrame();
	void endFrame();
	void draw(TFunction<void()> drawFunc);
	void render(ID3D12GraphicsCommandList* commandList, GpuDescriptorAllocator* descriptorAllocator, GpuResourceBarrierBatcher& resourceBarriers);
	void render(RenderGraph& graph);
	void setRenderEnable(bool value);
	void toggleRenderEnable();
	bool isDrawing() const { return shouldRender; }

private:
	const Window* window = nullptr;
	GpuPipelineState* pipelineState = nullptr;
	GpuBuffer* vertexBuffer[GPU_FRAME_NUM] = {};
	GpuBuffer* indexBuffer[GPU_FRAME_NUM] = {};
	GpuBuffer* uploadIndexBuffer[GPU_FRAME_NUM] = {};
	GpuBuffer* uploadVertexBuffer[GPU_FRAME_NUM] = {};
	GpuTexture* fontTexture = nullptr;
	bool shouldRender = false;
};