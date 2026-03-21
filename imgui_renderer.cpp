#include "imgui_renderer.h"
#include "bodoq_engine/math.h"
#include "bodoq_engine/input.h"
#include "imgui/imgui_vs.h"
#include "imgui/imgui_ps.h"
#include "bodoq_engine/win32.h"

ImGuiRenderer::ImGuiRenderer(const Window* window) :
	window(window)
{
	GpuGraphicsPipelineDesc psoDesc = {};
	psoDesc.pixel.shader = { ImGui_PS, sizeof(ImGui_PS) };
	psoDesc.pixel.renderTargets.add(DXGI_FORMAT_R8G8B8A8_UNORM);
	psoDesc.vertex.shader = { ImGui_VS, sizeof(ImGui_VS) };
	psoDesc.vertex.addVertexBuffer(sizeof(ImDrawVert))
		.addVertexAttribute(DXGI_FORMAT_R32G32_FLOAT, "POSITION", 0, offsetof(ImDrawVert, pos))
		.addVertexAttribute(DXGI_FORMAT_R32G32_FLOAT, "TEXCOORD", 0, offsetof(ImDrawVert, uv))
		.addVertexAttribute(DXGI_FORMAT_R8G8B8A8_UNORM, "COLOR", 0, offsetof(ImDrawVert, col));
	psoDesc.rasterizer.cullMode = D3D12_CULL_MODE_NONE;
	psoDesc.layout.add32BitConstant(0, 0, gpu::calcNumUint32FromSize<Float2>(), D3D12_SHADER_VISIBILITY_VERTEX);
	psoDesc.layout.addDescriptorTable(GpuDescriptorRange(
		GpuDescriptorRangeEntry(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0, 0)
	), D3D12_SHADER_VISIBILITY_PIXEL);
	psoDesc.layout.addStaticSampler(D3D12_FILTER_MIN_MAG_MIP_LINEAR, D3D12_TEXTURE_ADDRESS_MODE_WRAP, 0, 0, D3D12_SHADER_VISIBILITY_PIXEL);
	pipelineState = gpu::buildGraphicsPipelineState(psoDesc);

	ImGui::CreateContext();
	ImGui::StyleColorsDark();
	ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
	uint8_t* fontPixels = nullptr;
	int32_t fontWidth = 0, fontHeight = 0;
	ImGui::GetIO().Fonts->GetTexDataAsRGBA32(&fontPixels, &fontWidth, &fontHeight);
	fontTexture = gpu::createTexture(fontWidth, fontHeight, 1, fontPixels, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, DXGI_FORMAT_R8G8B8A8_UNORM, D3D12_RESOURCE_FLAG_NONE);
	ImGui::GetIO().Fonts->SetTexID((ImTextureID)fontTexture);
	setRenderEnable(false);

	auto ImGuiGetClipboardText = [](void* user_data) -> const char*
	{
		static String text;
		if (!OpenClipboard(nullptr))
			return nullptr;

		HANDLE data = GetClipboardData(CF_TEXT);
		if (data)
		{
			const char* clipText = static_cast<const char*>(GlobalLock(data));
			if (clipText)
			{
				text = clipText;
				GlobalUnlock(data);
			}
		}
		CloseClipboard();
		return text.rawStr();
	};

	auto ImGuiSetClipboardText = [](void* user_data, const char* text)
	{
		if (!OpenClipboard(nullptr))
			return;

		EmptyClipboard();
		const size_t len = strlen(text) + 1;
		HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, len);
		memcpy(GlobalLock(hMem), text, len);
		GlobalUnlock(hMem);
		SetClipboardData(CF_TEXT, hMem);
		CloseClipboard();
	};

	//ImGui::GetIO().SetClipboardTextFn = ImGuiSetClipboardText;
	//ImGui::GetIO().GetClipboardTextFn = ImGuiGetClipboardText;
	//ImGui::GetIO().ClipboardUserData = nullptr;
}

ImGuiRenderer::~ImGuiRenderer()
{
	gpu::destroyPipelineState(pipelineState);

	gpu::destroyTexture(fontTexture);
	for (uint32_t index = 0; index < GPU_FRAME_NUM; ++index)
	{
		gpu::destroyBuffer(vertexBuffer[index]);
		gpu::destroyBuffer(indexBuffer[index]);
		gpu::destroyBuffer(uploadIndexBuffer[index]);
		gpu::destroyBuffer(uploadVertexBuffer[index]);
	}
}

void ImGuiRenderer::beginFrame()
{
	// Update ImGui Input
	ImGui::GetIO().DisplaySize = { window->size.x, window->size.y };
	ImGui::NewFrame();

	ImGui::GetIO().AddMousePosEvent(input::mouseX(), input::mouseY());
	ImGui::GetIO().AddMouseButtonEvent(ImGuiMouseButton_Left, input::mouseDown(MouseButton::MOUSE_BUTTON_LEFT));
	ImGui::GetIO().AddMouseButtonEvent(ImGuiMouseButton_Right, input::mouseDown(MouseButton::MOUSE_BUTTON_RIGHT));
	ImGui::GetIO().AddMouseButtonEvent(ImGuiMouseButton_Middle, input::mouseDown(MouseButton::MOUSE_BUTTON_MIDDLE));
	ImGui::GetIO().AddInputCharacter(input::getLastChar());
	ImGui::GetIO().AddMouseWheelEvent(0, input::mouseWheelY());
	input::forEachKeyDown([](KeyCode keyCode, bool isKeyDown) {ImGui::GetIO().AddKeyEvent(ImGuiRenderer::mapKeyCode(keyCode), isKeyDown); });
	ImGui::GetIO().KeyCtrl = input::keyDown(KeyCode::CTRL);
	ImGui::GetIO().KeyMods = (input::keyDown(KeyCode::CTRL) ? ImGuiMod_Ctrl : 0) |
		(input::keyDown(KeyCode::SHIFT) ? ImGuiMod_Shift : 0) |
		(input::keyDown(KeyCode::ALT) ? ImGuiMod_Alt : 0);
}

void ImGuiRenderer::endFrame()
{
	ImGui::Render();
}

void ImGuiRenderer::draw(TFunction<void()> drawFunc)
{
	if (!shouldRender) return;
	beginFrame();
	drawFunc();
	endFrame();
}

void ImGuiRenderer::render(RenderGraph& graph)
{
	if (!shouldRender) return;

	ImDrawData* drawData = ImGui::GetDrawData();
	if (drawData == nullptr) return;

	uint64_t currentFrame = gpu::getFrameCount() % GPU_FRAME_NUM;

	static bool isFirstPass = true;
	if (isFirstPass)
	{
		graph.importBuffer("ImGuiVertexBuffer", nullptr);
		graph.importBuffer("ImGuiIndexBuffer", nullptr);
		graph.importBuffer("ImGuiUploadVertexBuffer", nullptr);
		graph.importBuffer("ImGuiUploadIndexBuffer", nullptr);
		graph.importBuffer("ImGuiTexture", nullptr);

		isFirstPass = false;
	}

	// ImGui: Create resources on demand
	if (drawData->TotalVtxCount > 0 && (vertexBuffer[currentFrame] == nullptr || vertexBuffer[currentFrame]->resource.apiResource->GetDesc().Width < drawData->TotalVtxCount * sizeof(ImDrawVert)))
	{
		if (vertexBuffer[currentFrame] != nullptr)
		{
			gpu::destroyBuffer(vertexBuffer[currentFrame]);
			gpu::destroyBuffer(uploadVertexBuffer[currentFrame]);
		}

		vertexBuffer[currentFrame] = gpu::createBuffer(drawData->TotalVtxCount * sizeof(ImDrawVert), D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER, D3D12_HEAP_TYPE_DEFAULT, nullptr, true);
		uploadVertexBuffer[currentFrame] = gpu::createBuffer(drawData->TotalVtxCount * sizeof(ImDrawVert), D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_HEAP_TYPE_UPLOAD, nullptr, true);
	}

	if (drawData->TotalIdxCount > 0 && (indexBuffer[currentFrame] == nullptr || indexBuffer[currentFrame]->resource.apiResource->GetDesc().Width < drawData->TotalIdxCount * sizeof(ImDrawIdx)))
	{
		if (indexBuffer[currentFrame] != nullptr)
		{
			gpu::destroyBuffer(indexBuffer[currentFrame]);
			gpu::destroyBuffer(uploadIndexBuffer[currentFrame]);
		}
		indexBuffer[currentFrame] = gpu::createBuffer(drawData->TotalIdxCount * sizeof(ImDrawIdx), D3D12_RESOURCE_STATE_INDEX_BUFFER, D3D12_HEAP_TYPE_DEFAULT, nullptr, true);
		uploadIndexBuffer[currentFrame] = gpu::createBuffer(drawData->TotalIdxCount * sizeof(ImDrawIdx), D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_HEAP_TYPE_UPLOAD, nullptr, true);
	}

	graph.updateBuffer("ImGuiVertexBuffer", vertexBuffer[currentFrame]);
	graph.updateBuffer("ImGuiIndexBuffer", indexBuffer[currentFrame]);
	graph.updateBuffer("ImGuiUploadVertexBuffer", uploadVertexBuffer[currentFrame]);
	graph.updateBuffer("ImGuiUploadIndexBuffer", uploadIndexBuffer[currentFrame]);

	if (drawData->CmdListsCount > 0)
	{
		// ImGui: Copy render data to upload buffers
		ImDrawVert* vertices = nullptr;
		ImDrawIdx* indices = nullptr;
		int32_t vbSize = 0, ibSize = 0;

		GPU_D3D_ASSERT(uploadVertexBuffer[currentFrame]->resource.apiResource->Map(0, nullptr, (void**)&vertices), "");
		GPU_D3D_ASSERT(uploadIndexBuffer[currentFrame]->resource.apiResource->Map(0, nullptr, (void**)&indices), "");

		for (int32_t index = 0; index < drawData->CmdListsCount; ++index)
		{
			const ImDrawList* cmdList = drawData->CmdLists[index];
			memcpy(vertices, cmdList->VtxBuffer.Data, cmdList->VtxBuffer.Size * sizeof(ImDrawVert));
			memcpy(indices, cmdList->IdxBuffer.Data, cmdList->IdxBuffer.Size * sizeof(ImDrawIdx));
			vertices += cmdList->VtxBuffer.Size;
			indices += cmdList->IdxBuffer.Size;
			vbSize += cmdList->VtxBuffer.Size * sizeof(ImDrawVert);
			ibSize += cmdList->IdxBuffer.Size * sizeof(ImDrawIdx);
		}

		DBG_ASSERT(vbSize == drawData->TotalVtxCount * sizeof(ImDrawVert), "");
		DBG_ASSERT(ibSize == drawData->TotalIdxCount * sizeof(ImDrawIdx), "");

		uploadIndexBuffer[currentFrame]->resource.apiResource->Unmap(0, nullptr);
		uploadVertexBuffer[currentFrame]->resource.apiResource->Unmap(0, nullptr);

		graph.addCopyPass("ImGui Buffers", [&](RgCopyPass& pass)
		{
			pass.copy("ImGuiVertexBuffer", "ImGuiUploadVertexBuffer");
			pass.copy("ImGuiIndexBuffer", "ImGuiUploadIndexBuffer");
		});
		struct PassData
		{
			RgResource vertexBuffer;
			RgResource indexBuffer;
			RgResource backbuffer;
			RgResource texture;
		};
		graph.addRasterPass<PassData>({ "ImGui", pipelineState }, [&](RgRasterPass& pass, PassData& passData)
		{
			passData.vertexBuffer = pass.createVBV("ImGuiVertexBuffer");
			passData.indexBuffer = pass.createIBV("ImGuiIndexBuffer");
			passData.texture = graph.updateTexture("ImGuiTexture", nullptr);
			passData.backbuffer = pass.createRTV("Backbuffer");
		},
		[=](const PassData& passData, RgPassRasterContext& ctx)
		{
			ctx.setVertexBuffer(passData.vertexBuffer, 0, ctx.getBufferSizeInBytes(passData.vertexBuffer), sizeof(ImDrawVert));
			ctx.setIndexBuffer(passData.indexBuffer, ctx.getBufferSizeInBytes(passData.indexBuffer), DXGI_FORMAT_R16_UINT);
			ctx.setViewport(0, 0, drawData->DisplaySize.x, drawData->DisplaySize.y, 0.0f, 1.0f);
			ctx.setPrimitiveTopology(RgPrimitiveTopology::TRIANGLE_LIST);
			ctx.setRenderTarget({ passData.backbuffer, RgView::RTVTex2D(DXGI_FORMAT_R8G8B8A8_UNORM, 0, 0) }, nullptr);
			ctx.set32BitConstants<Float2>(0, window->size, 0);
			ImVec2 clipOff = drawData->DisplayPos;
			uint32_t globalVtxOffset = 0;
			uint32_t globalIdxOffset = 0;
			for (int32_t Index = 0; Index < drawData->CmdListsCount; ++Index)
			{
				const ImDrawList* cmdList = drawData->CmdLists[Index];
				for (int32_t cmdIndex = 0; cmdIndex < cmdList->CmdBuffer.Size; ++cmdIndex)
				{
					const ImDrawCmd& cmd = cmdList->CmdBuffer[cmdIndex];
					ImVec2 clipMin = { cmd.ClipRect.x - clipOff.x, cmd.ClipRect.y - clipOff.y };
					ImVec2 clipMax = { cmd.ClipRect.z - clipOff.x, cmd.ClipRect.w - clipOff.y };
					if (clipMax.x <= clipMin.x || clipMax.y <= clipMin.y)
					{
						continue;
					}
					ctx.setScissorRect((uint32_t)clipMin.x, (uint32_t)clipMin.y, (uint32_t)clipMax.x, (uint32_t)clipMax.y);
					ctx.bindResources(1, {
						RgBinding::SRV(ctx.setTexture("ImGuiTexture", passData.texture, (GpuTexture*)cmd.TextureId), RgView::SRVTex2D(DXGI_FORMAT_R8G8B8A8_UNORM, 0, 1, 0, 0.0f))
						});
					ctx.drawIndexedInstanced(cmd.ElemCount, 1, cmd.IdxOffset + globalIdxOffset, cmd.VtxOffset + globalVtxOffset, 0);
				}
				globalIdxOffset += cmdList->IdxBuffer.Size;
				globalVtxOffset += cmdList->VtxBuffer.Size;
			}
		});
	}


}

void ImGuiRenderer::setRenderEnable(bool value)
{
	shouldRender = value;
	ShowCursor(shouldRender);
}

void ImGuiRenderer::toggleRenderEnable()
{
	shouldRender = !shouldRender;
	ShowCursor(shouldRender);
}

ImGuiKey ImGuiRenderer::mapKeyCode(KeyCode keyCode)
{
	switch (keyCode)
	{
	case KeyCode::ALT:
		return ImGuiKey_LeftAlt;
	case KeyCode::DOWN:
		return ImGuiKey_DownArrow;
	case KeyCode::LEFT:
		return ImGuiKey_LeftArrow;
	case KeyCode::RIGHT:
		return ImGuiKey_RightArrow;
	case KeyCode::UP:
		return ImGuiKey_UpArrow;
	case KeyCode::BACKSPACE:
		return ImGuiKey_Backspace;
	case KeyCode::CAPS_LOCK:
		return ImGuiKey_CapsLock;
	case KeyCode::CTRL:
		return ImGuiKey_LeftCtrl;
	case KeyCode::DEL:
		return ImGuiKey_Delete;
	case KeyCode::END:
		return ImGuiKey_End;
	case KeyCode::ENTER:
		return ImGuiKey_Enter;
	case KeyCode::ESC:
		return ImGuiKey_Escape;
	case KeyCode::F1:
		return ImGuiKey_F1;
	case KeyCode::F2:
		return ImGuiKey_F2;
	case KeyCode::F3:
		return ImGuiKey_F3;
	case KeyCode::F4:
		return ImGuiKey_F4;
	case KeyCode::F5:
		return ImGuiKey_F5;
	case KeyCode::F6:
		return ImGuiKey_F6;
	case KeyCode::F7:
		return ImGuiKey_F7;
	case KeyCode::F8:
		return ImGuiKey_F8;
	case KeyCode::F9:
		return ImGuiKey_F9;
	case KeyCode::F10:
		return ImGuiKey_F10;
	case KeyCode::F11:
		return ImGuiKey_F11;
	case KeyCode::F12:
		return ImGuiKey_F12;
	case KeyCode::HOME:
		return ImGuiKey_Home;
	case KeyCode::INSERT:
		return ImGuiKey_Insert;
	case KeyCode::NUM_LOCK:
		return ImGuiKey_NumLock;
	case KeyCode::NUMPAD_0:
		return ImGuiKey_Keypad0;
	case KeyCode::NUMPAD_1:
		return ImGuiKey_Keypad1;
	case KeyCode::NUMPAD_2:
		return ImGuiKey_Keypad2;
	case KeyCode::NUMPAD_3:
		return ImGuiKey_Keypad3;
	case KeyCode::NUMPAD_4:
		return ImGuiKey_Keypad4;
	case KeyCode::NUMPAD_5:
		return ImGuiKey_Keypad5;
	case KeyCode::NUMPAD_6:
		return ImGuiKey_Keypad6;
	case KeyCode::NUMPAD_7:
		return ImGuiKey_Keypad7;
	case KeyCode::NUMPAD_8:
		return ImGuiKey_Keypad8;
	case KeyCode::NUMPAD_9:
		return ImGuiKey_Keypad9;
	case KeyCode::NUMPAD_MINUS:
		return ImGuiKey_KeypadSubtract;
	case KeyCode::NUMPAD_ASTERIKS:
		return ImGuiKey_KeypadMultiply;
	case KeyCode::NUMPAD_DOT:
		return ImGuiKey_KeypadDecimal;
	case KeyCode::NUMPAD_SLASH:
		return ImGuiKey_KeypadDivide;
	case KeyCode::NUMPAD_SUM:
		return ImGuiKey_KeypadAdd;
	case KeyCode::PAGE_DOWN:
		return ImGuiKey_PageDown;
	case KeyCode::PAGE_UP:
		return ImGuiKey_PageUp;
	case KeyCode::PAUSE:
		return ImGuiKey_Pause;
	case KeyCode::PRINT_SCREEN:
		return ImGuiKey_PrintScreen;
	case KeyCode::SCROLL_LOCK:
		return ImGuiKey_ScrollLock;
	case KeyCode::SHIFT:
		return ImGuiKey_LeftShift;
	case KeyCode::SPACE:
		return ImGuiKey_Space;
	case KeyCode::TAB:
		return ImGuiKey_Tab;
	case KeyCode::A:
		return ImGuiKey_A;
	case KeyCode::B:
		return ImGuiKey_B;
	case KeyCode::C:
		return ImGuiKey_C;
	case KeyCode::D:
		return ImGuiKey_D;
	case KeyCode::E:
		return ImGuiKey_E;
	case KeyCode::F:
		return ImGuiKey_F;
	case KeyCode::G:
		return ImGuiKey_G;
	case KeyCode::H:
		return ImGuiKey_H;
	case KeyCode::I:
		return ImGuiKey_I;
	case KeyCode::J:
		return ImGuiKey_J;
	case KeyCode::K:
		return ImGuiKey_K;
	case KeyCode::L:
		return ImGuiKey_L;
	case KeyCode::M:
		return ImGuiKey_M;
	case KeyCode::N:
		return ImGuiKey_N;
	case KeyCode::O:
		return ImGuiKey_O;
	case KeyCode::P:
		return ImGuiKey_P;
	case KeyCode::Q:
		return ImGuiKey_Q;
	case KeyCode::R:
		return ImGuiKey_R;
	case KeyCode::S:
		return ImGuiKey_S;
	case KeyCode::T:
		return ImGuiKey_T;
	case KeyCode::U:
		return ImGuiKey_U;
	case KeyCode::V:
		return ImGuiKey_V;
	case KeyCode::W:
		return ImGuiKey_W;
	case KeyCode::X:
		return ImGuiKey_X;
	case KeyCode::Y:
		return ImGuiKey_Y;
	case KeyCode::Z:
		return ImGuiKey_Z;
	case KeyCode::NUM_0:
		return ImGuiKey_0;
	case KeyCode::NUM_1:
		return ImGuiKey_1;
	case KeyCode::NUM_2:
		return ImGuiKey_2;
	case KeyCode::NUM_3:
		return ImGuiKey_3;
	case KeyCode::NUM_4:
		return ImGuiKey_4;
	case KeyCode::NUM_5:
		return ImGuiKey_5;
	case KeyCode::NUM_6:
		return ImGuiKey_6;
	case KeyCode::NUM_7:
		return ImGuiKey_7;
	case KeyCode::NUM_8:
		return ImGuiKey_8;
	case KeyCode::NUM_9:
		return ImGuiKey_9;
	case KeyCode::QUOTE:
		return ImGuiKey_Apostrophe;
	case KeyCode::MINUS:
		return ImGuiKey_Minus;
	case KeyCode::COMMA:
		return ImGuiKey_Comma;
	case KeyCode::SLASH:
		return ImGuiKey_Slash;
	case KeyCode::SEMICOLON:
		return ImGuiKey_Semicolon;
	case KeyCode::LEFT_SQRBRACKET:
		return ImGuiKey_LeftBracket;
	case KeyCode::RIGHT_SQRBRACKET:
		return ImGuiKey_RightBracket;
	case KeyCode::BACKSLASH:
		return ImGuiKey_Backslash;
	case KeyCode::EQUALS:
		return ImGuiKey_Equal;
	default:
		break;
	}
	return ImGuiKey_None;
}
