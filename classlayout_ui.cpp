#include "bodoq_engine/window.h"
#include "bodoq_engine/gpu.h"
#include "bodoq_engine/input.h"
#include "bodoq_engine/file.h"
#include "bodoq_engine/filesystem.h"

#include "imgui_renderer.h"
#include "imgui/imgui_internal.h"

static TArray<String> PDBLogData;
#define PDB_LOG(fmt, ...) PDBLogData.add(str::format(fmt, ##__VA_ARGS__));
#include <algorithm>
#include <shellapi.h>

#include "pdb.h"

static void openInVisualStudio(const String& filePath, uint32_t lineNumber)
{
	String args = str::format(
		"/edit \"%s\" /command \"Edit.GoTo %u\"",
		*filePath,
		lineNumber
	);

	SHELLEXECUTEINFOA info = {};
	info.cbSize = sizeof(info);
	info.fMask = SEE_MASK_NOCLOSEPROCESS;
	info.lpVerb = "open";
	info.lpFile = "devenv.exe";
	info.lpParameters = *args;
	info.nShow = SW_HIDE;
	ShellExecuteExA(&info);
}

String serializeStructureToCSV(const pdb::StructureEntry& entry)
{
	String str = "Name,Type,Size,Offset,Access,BaseClass\n";

	for (const pdb::MemberEntry& memb : entry.members)
	{
		str += str::format("\"%s\",\"%s\",%u,%u,\"%s\",\"%s\"\n",
			*memb.name,
			*memb.type,
			memb.size,
			memb.offset,
			*pdb::asString(memb.access),
			*memb.baseClass
		);
	}

	return str;
}

String serializeListToCSV(const TArray<pdb::StructureEntry*>& entries)
{
	String str = "Type,Packed,Name,Size,Padding,Wastage,Members\n";

	for (const pdb::StructureEntry* entry : entries)
	{
		str += str::format("\"%s\",\"%s\",\"%s\",%u,%u,%.2f,%u\n",
			*pdb::asString(entry->type),
			entry->packed ? "true" : "false",
			*entry->name,
			entry->size,
			entry->padding,
			entry->wastage,
			entry->memberNum
		);
	}

	return str;
}

void hyperlink(const char* label, const char* url)
{
	ImVec4 linkColor = ImVec4(0.4f, 0.6f, 1.0f, 1.0f);
	ImGui::PushStyleColor(ImGuiCol_Text, linkColor);
	ImGui::Text(label);
	ImGui::PopStyleColor();
	if (ImGui::IsItemHovered())
	{
		ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
		ImGui::SetTooltip(url);
		ImVec2 min = ImGui::GetItemRectMin();
		ImVec2 max = ImGui::GetItemRectMax();
		ImGui::GetWindowDrawList()->AddLine(
			ImVec2(min.x, max.y),
			ImVec2(max.x, max.y),
			ImGui::ColorConvertFloat4ToU32(linkColor),
			1.0f);
	}
	if (ImGui::IsItemClicked())
	{
		ShellExecuteA(nullptr, "open", url, nullptr, nullptr, SW_SHOWNORMAL);
	}
}

#include "resource.h"

int main()
{
	PDBLogData.resize(1000);
	Window* window = win::createWindow("ClassLayout UI", 0, 0, 1920, 1080, false, true);

	HICON iconLarge = (HICON)LoadImageA(
		GetModuleHandle(nullptr),
		MAKEINTRESOURCEA(IDI_ICON1), // use your resource ID from Resource.rc
		IMAGE_ICON,
		GetSystemMetrics(SM_CXICON),    // 32x32
		GetSystemMetrics(SM_CYICON),
		LR_DEFAULTCOLOR
	);

	HICON iconSmall = (HICON)LoadImageA(
		GetModuleHandle(nullptr),
		MAKEINTRESOURCEA(IDI_ICON1),
		IMAGE_ICON,
		GetSystemMetrics(SM_CXSMICON),  // 16x16
		GetSystemMetrics(SM_CYSMICON),
		LR_DEFAULTCOLOR
	);

	SendMessageA((HWND)window->windowHandle, WM_SETICON, ICON_BIG, (LPARAM)iconLarge);
	SendMessageA((HWND)window->windowHandle, WM_SETICON, ICON_SMALL, (LPARAM)iconSmall);

	gpu::init(window, false, false);
	RenderGraph* graph = new RenderGraph();
	ImGuiRenderer* ui = new ImGuiRenderer(window);

	ui->toggleRenderEnable();

	RgResource backbuffer = graph->importTexture("Backbuffer", nullptr, true);
	TArray<pdb::StructureEntry> results;
	TArray<pdb::StructureEntry> displayResults;
	int32_t selectedStruct = -1;
	TArray<int32_t> selectedStructStack;

	enum class LoadingState
	{
		WAITING,
		LOADING,
		FINISHED
	};

	struct LoadingContext
	{
		TArray<pdb::StructureEntry> results;
		TArray<pdb::StructureEntry> displayResults;
		WString fileToLoad;
		LoadingState state = LoadingState::WAITING;
		bool shouldRun = true;
	};

	LoadingContext loadingContext{};
	HANDLE loadingThreadHandle = CreateThread(nullptr, 4096, [](void* data) -> DWORD
	{
		LoadingContext* loadingContext = (LoadingContext*)data;
		while (loadingContext->shouldRun)
		{
			if (loadingContext->state == LoadingState::LOADING)
			{
				size_t fileSize = 0;
				if (void* fileData = file::readFile(str::toString(loadingContext->fileToLoad), fileSize))
				{
					loadingContext->results.clear();
					loadingContext->displayResults.clear();
					if (pdb::process(fileData, fileSize, loadingContext->results, []() { DBG_PANIC("Error"); }))
					{
						loadingContext->displayResults = loadingContext->results;
						std::sort(loadingContext->displayResults.begin(), loadingContext->displayResults.end(), [](const pdb::StructureEntry& a, const pdb::StructureEntry& b)
						{
							return a.size > b.size;
						});

					}
					free(fileData);
				}
				loadingContext->state = LoadingState::FINISHED;
			}
			Yield();
		}
		return 0;
	
	}, &loadingContext, 0, nullptr);

	char searchBuffer[256] = {};
	TArray<pdb::StructureEntry*> filteredResults;
	bool searchDirty = true;

	do
	{
		input::pollEvents();

		// Draw the UI
		ui->draw([&]()
		{
			if (displayResults.getNum() > 0)
			{
				float menuBarHeight = ImGui::GetFrameHeight();
				ImGuiViewport* viewport = ImGui::GetMainViewport();
				ImVec2 vpSize = viewport->Size;
				vpSize.y -= menuBarHeight;
				ImGui::SetNextWindowSize(vpSize);
				ImGui::SetNextWindowPos(ImVec2(viewport->Pos.x, viewport->Pos.y + menuBarHeight), ImGuiCond_Once);
				if (ImGui::Begin("ClassLayout", nullptr, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoTitleBar))
				{
					ImGui::SetNextItemWidth(-1);
					ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x * 0.2f);
					if (ImGui::InputTextWithHint("##search", "Search...", searchBuffer, sizeof(searchBuffer), ImGuiInputTextFlags_EnterReturnsTrue))
					{
						searchDirty = true;
					}

					ImGui::SameLine();
					if (ImGui::Button("Search"))
					{
						searchDirty = true;
					}

					ImGui::SameLine();
					ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical, 2.0f);

					ImGui::SameLine();
					if (ImGui::Button("Export CSV"))
					{
						WString suggestedName = L"classlayout.csv";
						WString outputPath;
						if (win::dialogSave(window, L"CSV File (*.csv)\0*.csv\0All Files (*.*)\0*.*\0", suggestedName, L"csv", outputPath))
						{
							String csv = serializeListToCSV(filteredResults);
							file::writeFile(str::toString(outputPath), *csv, csv.length());
						}
					}

					if (searchDirty)
					{
						filteredResults.reset();
						String searchLower = searchBuffer;
						for (char& c : searchLower) c = (char)tolower(c);

						for (uint64_t i = 0; i < displayResults.getNum(); ++i)
						{
							pdb::StructureEntry& entry = displayResults[i];
							if (searchBuffer[0] == '\0')
							{
								filteredResults.add(&entry);
								continue;
							}

							String nameLower = *entry.name;
							for (char& c : nameLower) c = (char)tolower(c);

							if (nameLower.find(searchLower) != String::npos)
							{
								filteredResults.add(&entry);
							}
						}
						searchDirty = false;
					}

                    if (ImGui::BeginTable("classlayout_layout", 2, ImGuiTableFlags_Resizable | ImGuiTableFlags_SizingFixedSame))
                    {
                        ImGui::TableSetupColumn("names##res", ImGuiTableColumnFlags_WidthStretch, vpSize.x * 0.5f);
                        ImGui::TableSetupColumn("properties##res", ImGuiTableColumnFlags_WidthStretch, vpSize.x * 0.5f);
                        ImGui::TableNextRow();

                        // Left column: list
                        ImGui::TableSetColumnIndex(0);
                        ImGui::BeginChild("classlayout_list", ImVec2(0, 0), true, ImGuiWindowFlags_AlwaysUseWindowPadding);
                        {
							if (ImGui::BeginTable("ClassList", 7, ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_Sortable | ImGuiTableFlags_ScrollY | ImGuiTableFlags_ScrollX | ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersOuter | ImGuiTableFlags_BordersV | ImGuiTableFlags_Resizable | ImGuiTableFlags_Hideable | ImGuiTableFlags_NoSavedSettings))
							{
								ImGui::TableSetupScrollFreeze(0, 1);
								ImGui::TableSetupColumn("Type", ImGuiTableColumnFlags_None, 100.0f, 0);
								ImGui::TableSetupColumn("Packed", ImGuiTableColumnFlags_None, 100.0f, 1);
								ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_None, 300.0f, 2);
								ImGui::TableSetupColumn("Size", ImGuiTableColumnFlags_None, 200.0f, 3);
								ImGui::TableSetupColumn("Padding", ImGuiTableColumnFlags_None, 100.0f, 4);
								ImGui::TableSetupColumn("Wastage", ImGuiTableColumnFlags_None, 100.0f, 5);
								ImGui::TableSetupColumn("Members", ImGuiTableColumnFlags_None, 100.0f, 6);
								ImGui::TableHeadersRow();

								if (ImGuiTableSortSpecs* sortSpecs = ImGui::TableGetSortSpecs())
								{
									if (sortSpecs->SpecsDirty)
									{
										if (sortSpecs->SpecsCount > 0)
										{
											searchDirty = true;
											const ImGuiTableColumnSortSpecs& spec = sortSpecs->Specs[0];
											bool ascending = spec.SortDirection == ImGuiSortDirection_Ascending;
											switch (spec.ColumnUserID)
											{
											case 0: // Type
												std::sort(displayResults.getData(), displayResults.getData() + displayResults.getNum(), [ascending](const pdb::StructureEntry& a, const pdb::StructureEntry& b) 
												{
													int cmp = strcmp(pdb::asString(a.type), pdb::asString(b.type));
													return ascending ? cmp < 0 : cmp > 0;
												});
												break;
											case 1: // Packed
												std::sort(displayResults.getData(), displayResults.getData() + displayResults.getNum(), [ascending](const pdb::StructureEntry& a, const pdb::StructureEntry& b)
												{
													return ascending ? a.packed < b.packed : a.packed > b.packed;
												});
												break;
											case 2: // Name
												std::sort(displayResults.getData(), displayResults.getData() + displayResults.getNum(), [ascending](const pdb::StructureEntry& a, const pdb::StructureEntry& b)
												{
													int cmp = strcmp(*a.name, *b.name);
													return ascending ? cmp < 0 : cmp > 0;
												});
												break;
											case 3: // Size
												std::sort(displayResults.getData(), displayResults.getData() + displayResults.getNum(), [ascending](const pdb::StructureEntry& a, const pdb::StructureEntry& b) 
												{
													return ascending ? a.size < b.size : a.size > b.size;
												});
												break;
											case 4: // Padding
												std::sort(displayResults.getData(), displayResults.getData() + displayResults.getNum(), [ascending](const pdb::StructureEntry& a, const pdb::StructureEntry& b) 
												{
													return ascending ? a.padding < b.padding : a.padding > b.padding;
												});
												break;
											case 5: // Wastage
												std::sort(displayResults.getData(), displayResults.getData() + displayResults.getNum(), [ascending](const pdb::StructureEntry& a, const pdb::StructureEntry& b) 
												{
													return ascending ? a.wastage < b.wastage : a.wastage > b.wastage;
												});
												break;
											case 6: // Members
												std::sort(displayResults.getData(), displayResults.getData() + displayResults.getNum(), [ascending](const pdb::StructureEntry& a, const pdb::StructureEntry& b) 
												{
													return ascending ? a.memberNum < b.memberNum : a.memberNum > b.memberNum;
												});
												break;
											}
										}
										sortSpecs->SpecsDirty = false;
									}
								}

								ImGuiListClipper clipper;
								clipper.Begin((int)filteredResults.getNum());
								bool showCopyCtx = true;
								while (clipper.Step())
								{
									for (uint32_t row = clipper.DisplayStart; row < (uint32_t)clipper.DisplayEnd; row++)
									{
										ImGui::TableNextRow();

										const pdb::StructureEntry& entry = *filteredResults[row];

										ImGui::TableSetColumnIndex(0);
										ImGui::Selectable(*pdb::asString(entry.type));

										ImGui::TableSetColumnIndex(1);
										ImGui::Selectable(entry.packed ? "true" : "false");

										ImGui::TableSetColumnIndex(2);
										if (ImGui::Selectable(*entry.name))
										{
											selectedStruct = (int32_t)entry.index;
											selectedStructStack.reset();
										}

										ImGui::TableSetColumnIndex(3);
										ImGui::Selectable(*str::format("%u b", entry.size));

										ImGui::TableSetColumnIndex(4);
										if (!entry.hasVirtualBases)
										{
											ImGui::Selectable(*str::format("%u b", entry.padding));
										}
										else
										{
											ImGui::Selectable("Unavailable");
										}

										ImGui::TableSetColumnIndex(5);

										if (!entry.hasVirtualBases)
										{
											ImU32 wastageColor = 0;
											float input = cbrtf((entry.wastage / 100.0f) * 2.0f);
											input = input < 0.0f ? 0.0f : (input > 1.0f ? 1.0f : input);
											input = powf(input, 0.8f);
											float hue = (1.0f - input) * 126.0f / 360.0f;
											float r, g, b;
											ImGui::ColorConvertHSVtoRGB(hue, 0.6f, 0.7f, r, g, b);
											wastageColor = IM_COL32((int)(r * 255), (int)(g * 255), (int)(b * 255), 255);
											ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(0, 0, 0, 255));
											ImGui::TableSetBgColor(ImGuiTableBgTarget_CellBg, wastageColor);
											ImGui::Selectable(*str::format("%.2f%%", entry.wastage));
											ImGui::PopStyleColor();
										}
										else
										{
											ImGui::Selectable("Unavailable");
										}

										ImGui::TableSetColumnIndex(6);
										ImGui::Selectable(*str::format("%u", (uint32_t)entry.memberNum));

									}
								}

								ImGui::EndTable();
							}

                        }
                        ImGui::EndChild();

                        // Right column: properties
                        ImGui::TableSetColumnIndex(1);
                        ImGui::BeginChild("classlayout_props", ImVec2(0, 0), true, ImGuiWindowFlags_AlwaysUseWindowPadding);
                        {
                            if (selectedStruct >= 0 && selectedStruct < (int)results.getNum())
                            {
								if (selectedStructStack.getNum())
								{
									if (ImGui::Button("< Back"))
									{
										selectedStruct = selectedStructStack.last();
										selectedStructStack.pop();
									}
								}

								const pdb::StructureEntry& selectedEntry = results[(uint64_t)selectedStruct];

								ImGui::Text("Name: ");
								ImGui::SameLine();
								ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0, 0, 0, 0));
								ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0, 0, 0, 0));
								ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0, 0));
								ImGui::SetNextItemWidth(-1);
								ImGui::InputText("##label", const_cast<char*>(*selectedEntry.name), selectedEntry.name.length(),
									ImGuiInputTextFlags_ReadOnly);
								ImGui::PopStyleVar();
								ImGui::PopStyleColor(2);

								if (ImGui::BeginPopupContextItem("##copy"))
								{
									if (ImGui::MenuItem("Copy"))
									{
										ImGui::SetClipboardText(*selectedEntry.name);
									}
									ImGui::EndPopup();
								}

								ImGui::Text("File: ");
								ImGui::SameLine();
								ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0, 0, 0, 0));
								ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0, 0, 0, 0));
								ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0, 0));
								ImGui::SetNextItemWidth(-1);
								ImGui::InputText("##label2", const_cast<char*>(*selectedEntry.filePath), selectedEntry.filePath.length(),
									ImGuiInputTextFlags_ReadOnly);
								ImGui::PopStyleVar();
								ImGui::PopStyleColor(2);

								if (ImGui::BeginPopupContextItem("##copy2"))
								{
									if (ImGui::MenuItem("Copy"))
									{
										ImGui::SetClipboardText(*selectedEntry.filePath);
									}
									else if (ImGui::MenuItem("Open"))
									{
										openInVisualStudio(selectedEntry.filePath, selectedEntry.lineNumber);
									}
									ImGui::EndPopup();
								}

								ImGui::Text("Type %s", *pdb::asString(selectedEntry.type));
								ImGui::Text("Packed: %s", (selectedEntry.packed ? "true" : "false"));
								ImGui::Text("Size: %u b", (selectedEntry.size));
								ImGui::Text("Padding: %u b", (selectedEntry.padding));
								if (!selectedEntry.hasVirtualBases)
								{
									ImU32 wastageColor = 0;
									float input = cbrtf((selectedEntry.wastage / 100.0f) * 2.0f);
									input = input < 0.0f ? 0.0f : (input > 1.0f ? 1.0f : input);
									input = powf(input, 0.8f);
									float hue = (1.0f - input) * 126.0f / 360.0f;
									float r, g, b;
									ImGui::ColorConvertHSVtoRGB(hue, 0.6f, 0.7f, r, g, b);
									wastageColor = IM_COL32((int)(r * 255), (int)(g * 255), (int)(b * 255), 255);
									ImGui::PushStyleColor(ImGuiCol_Text, wastageColor);
									ImGui::Text("Wastage: %.2f%%", (selectedEntry.wastage));
									ImGui::PopStyleColor();
								}
								else
								{
									ImGui::Text("Wastage: Has virtual bases - Padding info unavailable");
								}

								ImGui::Text("Members: %u", (selectedEntry.memberNum));

								if (ImGui::Button("Copy as CSV"))
								{
									String csv = serializeStructureToCSV(selectedEntry);
									ImGui::SetClipboardText(*csv);
								}

								ImGui::Separator();

								if (ImGui::BeginTable("ClassProps", 5, ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_ScrollY | ImGuiTableFlags_ScrollX | ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersOuter | ImGuiTableFlags_BordersV | ImGuiTableFlags_Resizable | ImGuiTableFlags_Hideable))
								{
									ImGui::TableSetupScrollFreeze(0, 1);
									ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_None, 100.0f);
									ImGui::TableSetupColumn("Type", ImGuiTableColumnFlags_None, 100.0f);
									ImGui::TableSetupColumn("Size", ImGuiTableColumnFlags_None, 50.0f);
									ImGui::TableSetupColumn("Offset", ImGuiTableColumnFlags_None, 50.0f);
									//ImGui::TableSetupColumn("Access", ImGuiTableColumnFlags_None, 100.0f);
									ImGui::TableSetupColumn("BaseClass", ImGuiTableColumnFlags_None, 100.0f);
									ImGui::TableHeadersRow();

									ImGuiListClipper clipper;
									clipper.Begin((int)selectedEntry.members.getNum());
									while (clipper.Step())
									{
										for (uint32_t row = clipper.DisplayStart; row < (uint32_t)clipper.DisplayEnd; row++)
										{
											ImGui::TableNextRow();

											const pdb::MemberEntry& member = selectedEntry.members[row];

											if (member.isPadding)
											{
												ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg0, IM_COL32(100, 50, 50, 255));
											}

											ImGui::TableSetColumnIndex(0);
											ImGui::Selectable(*member.name);

											ImGui::TableSetColumnIndex(1);
											if (ImGui::Selectable(str::format("%s##%u", *member.type, row)) && member.typeIndex > -1)
											{
												selectedStructStack.add(selectedStruct);
												selectedStruct = (int32_t)member.typeIndex;
											}

											ImGui::TableSetColumnIndex(2);
											ImGui::Selectable(*str::format("%u", member.size));

											ImGui::TableSetColumnIndex(3);
											ImGui::Selectable(*str::format("%u", member.offset));

											ImGui::TableSetColumnIndex(4);
											if (ImGui::Selectable(str::format("%s##%u", *member.baseClass, row)) && member.baseClassIndex > -1)
											{
												selectedStructStack.add(selectedStruct);
												selectedStruct = (int32_t)member.baseClassIndex;
											}
										}
									}

									ImGui::EndTable();
								}

                            }
                        }
                        ImGui::EndChild();
                        ImGui::EndTable();
                    }


										
					ImGui::End();
				}
			}

			if (loadingContext.state == LoadingState::LOADING)
			{
				float menuBarHeight = ImGui::GetFrameHeight();
				ImGuiViewport* viewport = ImGui::GetMainViewport();
				ImVec2 vpSize = viewport->Size;
				vpSize.y -= menuBarHeight;
				ImGui::SetNextWindowSize(vpSize);
				ImGui::SetNextWindowPos(ImVec2(viewport->Pos.x, viewport->Pos.y + menuBarHeight), ImGuiCond_Once);
				if (ImGui::Begin("LoadingWindow", nullptr, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoTitleBar))
				{
					static char loadingBar[] = { '|', '/', '-', '\\' };
					static uint32_t loadingIndex = 0;
					ImGui::Text("Loading %s %c", *str::toString(loadingContext.fileToLoad), loadingBar[loadingIndex % 4]);
					for (uint64_t index = 0, num = PDBLogData.getNum(); index < num; ++index)
					{
						ImGui::Text("%s", *PDBLogData[index]);
					}
					ImGui::End();
					loadingIndex++;
				}
			}
			else if (loadingContext.state == LoadingState::FINISHED)
			{
				results = loadingContext.results;
				displayResults = loadingContext.displayResults;
				loadingContext.state = LoadingState::WAITING;
				selectedStruct = -1;
				PDBLogData.reset();
				searchDirty = true;
				searchBuffer[0] = '\0';
			}

			if (ImGui::BeginMainMenuBar())
			{
				if (ImGui::BeginMenu("File"))
				{
					if (ImGui::MenuItem("Open PDB File"))
					{
						WString filePath;
						if (win::dialogOpen(window, L"PDB File (*.pdb)\0*.pdb\0All Files (*.*)\0*.*\0", L"pdb", filePath))
						{
							loadingContext.fileToLoad = filePath;
							loadingContext.state = LoadingState::LOADING;
							results.clear();
							displayResults.clear();
						}
					}
					ImGui::EndMenu();
				}
				
				if (ImGui::BeginMenu("About"))
				{
					hyperlink("Author: Felipe Alfonso (bitnenfer)", "https://bitnenfer.com/");
					hyperlink("Source Code ", "https://github.com/bitnenfer/classlayout_ui");
					ImGui::Separator();
					hyperlink("Dear ImGui for UI", "https://github.com/ocornut/imgui");
					ImGui::EndMenu();
				}

				ImGui::EndMainMenuBar();
			}
		});
		
		// Render the frame
		if (GpuFrameData* frameData = gpu::beginFrame())
		{
			graph->updateTexture("Backbuffer", backbuffer, frameData->backbuffer);
			graph->addRasterPass<RgResource>({ "Clean Backbuffer", nullptr }, [&](RgRasterPass& pass, RgResource& passData)
			{
				passData = pass.createRTV(backbuffer);
			},
			[=](const RgResource& passData, RgPassRasterContext& ctx)
			{
				ctx.setRenderTargetClear({ 0, 0, 0, 1 }, 1.0f, 0);
				ctx.setRenderTarget({ passData, RgView::RTVTex2D(DXGI_FORMAT_R8G8B8A8_UNORM, 0, 0) }, nullptr, RgClearFlag::CLEAR_COLOR);
			});

			ui->render(*graph);
			graph->execute(frameData->commandList);
			gpu::endFrame();
		}

		gpu::present();
	} while (!input::shouldQuit());

	loadingContext.shouldRun = false;
	WaitForSingleObject(loadingThreadHandle, INFINITE);

	// Just exit process
	ExitProcess(0);

	gpu::waitForAllFrames();
	delete ui;
	gpu::destroy();
	win::destroyWindow(window);

	return 0;
}