#include "gui.h"
#include "imgui_stream.h"
#include "core.h"
#include "../imgui/imgui.h"
#include "../imgui/imgui_impl_dx9.h"
#include "../imgui/imgui_impl_win32.h"
#include "../imgui/imgui_internal.h"

ImGuiStream imguiStream;
bool autoScroll = false;
static bool pruneStarted = false;
static bool pruneCompleted = false;
static std::string pruneDurationText;

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(
	HWND window,
	UINT message,
	WPARAM wideParameter,
	LPARAM longParameter
);

long __stdcall WindowProcess(
	HWND window,
	UINT message,
	WPARAM wideParameter,
	LPARAM longParameter)
{
	if (ImGui_ImplWin32_WndProcHandler(window, message, wideParameter, longParameter))
		return true;

	switch (message)
	{
	case WM_SIZE:
		{
			if (gui::device && wideParameter != SIZE_MINIMIZED)
			{
				gui::presentParameters.BackBufferWidth = LOWORD(longParameter);
				gui::presentParameters.BackBufferHeight = HIWORD(longParameter);
				gui::ResetDevice();
			}
		}
		return 0;

	case WM_SYSCOMMAND:
		{
			if ((wideParameter & 0xfff0) == SC_KEYMENU) // disable alt app menu
				return 0;
		}
		break;

	case WM_DESTROY:
		{
			PostQuitMessage(0);
		}
		return 0;

	case WM_LBUTTONDOWN:
		{
			gui::position = MAKEPOINTS(longParameter);
		}
		return 0;

	case WM_MOUSEMOVE:
		{
			if (wideParameter == MK_LBUTTON)
			{
				const auto points = MAKEPOINTS(longParameter);
				auto rect = RECT{};

				GetWindowRect(gui::window, &rect);

				rect.left += points.x - gui::position.x;
				rect.top += points.y - gui::position.y;

				if (gui::position.x >= 0 &&
					gui::position.x <= gui::WIDTH &&
					gui::position.y >= 0 && gui::position.y <= 19)
					SetWindowPos(
						gui::window,
						HWND_TOPMOST,
						rect.left,
						rect.top,
						0, 0,
						SWP_SHOWWINDOW | SWP_NOSIZE | SWP_NOZORDER
					);
			}
		}
		return 0;
	}


	return DefWindowProcW(window, message, wideParameter, longParameter);
}

void gui::CreateHWindow(
	const char* windowName,
	const char* className) noexcept
{
	windowClass.cbSize = sizeof(WNDCLASSEXA);
	windowClass.style = CS_CLASSDC;
	windowClass.lpfnWndProc = WindowProcess;
	windowClass.cbClsExtra = 0;
	windowClass.cbWndExtra = 0;
	windowClass.hInstance = GetModuleHandleA(nullptr);
	windowClass.hIcon = nullptr;
	windowClass.hCursor = nullptr;
	windowClass.hbrBackground = nullptr;
	windowClass.lpszMenuName = nullptr;
	windowClass.lpszClassName = className;
	windowClass.hIconSm = nullptr;

	RegisterClassExA(&windowClass);

	window = CreateWindowA(
		className,
		windowName,
		WS_POPUP,
		100,
		100,
		WIDTH,
		HEIGHT,
		nullptr,
		nullptr,
		windowClass.hInstance,
		nullptr
	);

	ShowWindow(window, SW_SHOWDEFAULT);
	UpdateWindow(window);
}

void gui::DestroyHWindow() noexcept
{
	DestroyWindow(window);
	UnregisterClass(windowClass.lpszClassName, windowClass.hInstance);
}

bool gui::CreateDevice() noexcept
{
	d3d = Direct3DCreate9(D3D_SDK_VERSION);

	if (!d3d)
		return false;

	ZeroMemory(&presentParameters, sizeof(presentParameters));
	presentParameters.Windowed = TRUE;
	presentParameters.SwapEffect = D3DSWAPEFFECT_DISCARD;
	presentParameters.BackBufferFormat = D3DFMT_UNKNOWN;
	presentParameters.EnableAutoDepthStencil = TRUE;
	presentParameters.AutoDepthStencilFormat = D3DFMT_D16;
	presentParameters.PresentationInterval = D3DPRESENT_INTERVAL_ONE;

	if (d3d->CreateDevice(
		D3DADAPTER_DEFAULT,
		D3DDEVTYPE_HAL,
		window,
		D3DCREATE_HARDWARE_VERTEXPROCESSING,
		&presentParameters,
		&device) < 0)
		return false;

	return true;
}


void gui::ResetDevice() noexcept
{
	ImGui_ImplDX9_InvalidateDeviceObjects();

	const auto result = device->Reset(&presentParameters);

	if (result == D3DERR_INVALIDCALL)
		IM_ASSERT(0);

	ImGui_ImplDX9_CreateDeviceObjects();
}

void gui::DestroyDevice() noexcept
{
	if (device)
	{
		device->Release();
		device = nullptr;
	}

	if (d3d)
	{
		d3d->Release();
		d3d = nullptr;
	}
}

void gui::CreateImGui() noexcept
{
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO();
	io.IniFilename = nullptr;
	ImGui::StyleColorsDark();
	ImGui_ImplWin32_Init(window);
	ImGui_ImplDX9_Init(device);

	// set stream buffer pointer
	std::cout.rdbuf(imguiStream.rdbuf());
}

void gui::DestroyImGui() noexcept
{
	ImGui_ImplDX9_Shutdown();
	ImGui_ImplWin32_Shutdown();
	ImGui::DestroyContext();
}

void gui::BeginRender() noexcept
{
	MSG message;

	while (PeekMessage(&message, nullptr, 0, 0, PM_REMOVE))
	{
		TranslateMessage(&message);
		DispatchMessage(&message);
	}

	ImGui_ImplDX9_NewFrame();
	ImGui_ImplWin32_NewFrame();
	ImGui::NewFrame();
}

void gui::EndRender() noexcept
{
	ImGui::EndFrame();

	device->SetRenderState(D3DRS_ZENABLE, FALSE);
	device->SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);
	device->SetRenderState(D3DRS_SCISSORTESTENABLE, FALSE);

	device->Clear(0, nullptr, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER, D3DCOLOR_RGBA(0, 0, 0, 255), 1.0f, 0);

	if (device->BeginScene() >= 0)
	{
		ImGui::Render();
		ImGui_ImplDX9_RenderDrawData(ImGui::GetDrawData());
		device->EndScene();
	}

	const auto result = device->Present(nullptr, nullptr, nullptr, nullptr);

	if (result == D3DERR_DEVICELOST && device->TestCooperativeLevel() == D3DERR_DEVICENOTRESET)
		ResetDevice();
}

void gui::Render() noexcept
{
	ImGui::SetNextWindowPos({0, 0,});
	ImGui::SetNextWindowSize({WIDTH, HEIGHT});
	ImGui::Begin(
		"veil technologies",
		&exit,
		ImGuiWindowFlags_NoResize |
		ImGuiWindowFlags_NoSavedSettings |
		ImGuiWindowFlags_NoCollapse |
		ImGuiWindowFlags_NoMove
	);


	// begin tab
	if (ImGui::BeginTabBar("tabs"))
	{
		// main tab
		if (ImGui::BeginTabItem("main"))
		{
			ImGui::LabelText(" ", "windows optimization tool | version 1\nveil technologies - 2024");
			ImGui::SeparatorEx(1);

			ImGui::Spacing();

			if (ImGui::Button("start cleanmgr"))
			{
				system("echo off");
				system("cleanmgr /D C");
			}
			if (ImGui::IsItemHovered())
				ImGui::SetTooltip("starts windows cleanmgr service.");

			ImGui::SameLine();

			if (ImGui::Button("stop windows update service"))
				system("net stop wuauserv");
			if (ImGui::IsItemHovered())
				ImGui::SetTooltip("stops windows update service & current update process.");

			ImGui::SameLine();

			if (ImGui::Button("start dfrgui"))
				system("dfrgui");
			if (ImGui::IsItemHovered())
				ImGui::SetTooltip("starts defragmentation gui.");

			ImGui::SameLine();

			if (ImGui::Button("restore disk image"))
				system("dism /online /cleanup-image /restorehealth");
			if (ImGui::IsItemHovered())
				ImGui::SetTooltip("starts the windows disk image repair tool");

			ImGui::SeparatorText("pruning - deletes unnecessary update files and temporary folders.");
			ImGui::Spacing();

			if (ImGui::Button("start prune"))
			{
				if(globals::logPruneTime)
				{
					pruneStarted = true;
					pruneCompleted = false;
					pruneDurationText = "";

					std::chrono::time_point<std::chrono::steady_clock> start = std::chrono::high_resolution_clock::now();
					core::Initiate();
					std::chrono::time_point<std::chrono::steady_clock> end = std::chrono::high_resolution_clock::now();
					auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

					pruneDurationText = "completed in " + std::to_string(duration.count()) + " ms";
					pruneCompleted = true;
					pruneStarted = false;
				}
				else
					core::Initiate();
			}

			if (ImGui::IsItemHovered())
				ImGui::SetTooltip("deletes unnecessary update files and temporary folders.");

			if (pruneCompleted)
			{
				ImGui::SameLine();
				ImGui::TextColored(ImVec4(0, 1, 0, 1), pruneDurationText.c_str());
			}


			ImGui::Spacing();
			ImGui::SeparatorText("logs");

			std::vector<std::string> lines;
			imguiStream.GetLines(lines);

			// quick fix for the first 4 lines being blank
			if (lines.size() > 4)
				lines.erase(lines.begin(), lines.begin() + 4);

			if (ImGui::BeginListBox("##logs", ImVec2(719, 330)))
			{
				for (const auto& line : lines)
				{
					ImGui::Selectable(line.c_str());
					ImGui::SetScrollHereY(1.0f);
				}

				ImGui::EndListBox();
			}

			// end of main tab
			ImGui::EndTabItem();
		}

		if (ImGui::BeginTabItem("settings"))
		{
			static char newDir[256] = "";
			static int selectedDirIndex = -1;

			ImGui::Spacing();
			ImGui::SeparatorText("directory settings");
			ImGui::Spacing();

			if (ImGui::BeginCombo("prune directories",
			                      selectedDirIndex >= 0
				                      ? globals::pruneDirectories[selectedDirIndex].c_str()
				                      : "select directory"))
			{
				for (size_t i = 0; i < globals::pruneDirectories.size(); i++)
				{
					bool isSelected = (selectedDirIndex == i);
					if (ImGui::Selectable(globals::pruneDirectories[i].c_str(), isSelected))
						selectedDirIndex = i;
					if (isSelected)
						ImGui::SetItemDefaultFocus();
				}

				ImGui::EndCombo();
			}

			// not a good solution but does the job i guess lol
			ImGui::Spacing();
			ImGui::Spacing();
			ImGui::Separator();
			ImGui::Spacing();

			ImGui::Text("add a new directory");
			ImGui::InputText("enter directory path", newDir, IM_ARRAYSIZE(newDir));
			if (ImGui::Button("add"))
			{
				if (strlen(newDir) > 0)
				{
					globals::pruneDirectories.emplace_back(std::string(newDir));
					newDir[0] = '\0';
				}
			}


			// adds remove button AFTER a directory has been selected	
			if (selectedDirIndex >= 0)
			{
				ImGui::SameLine();
				if (ImGui::Button("remove"))
				{
					globals::pruneDirectories.erase(globals::pruneDirectories.begin() + selectedDirIndex);
					selectedDirIndex--;
				}
			}


			ImGui::Spacing();
			ImGui::SeparatorText("multithreading settings");
			ImGui::Spacing();


			ImGui::Checkbox("reserve threads", &globals::reserveThreads);
			if(ImGui::IsItemHovered())
				ImGui::SetTooltip("controls thread reservation to limit thread creation for file operations");

			ImGui::Checkbox("log prune time", &globals::logPruneTime);
			if(ImGui::IsItemHovered())
				ImGui::SetTooltip("logs prune time");

			// end of settings tab
			ImGui::EndTabItem();
		}

		if(ImGui::BeginTabItem("misc"))
		{
			ImGui::Spacing();
			ImGui::SeparatorText("network solutions");
			ImGui::Spacing();

			if(ImGui::Button("change ip"))
				core::ChangeIP();
			if(ImGui::IsItemHovered())
				ImGui::SetTooltip("changes the ip address");

			ImGui::SameLine();
		}

		// end of tabs
		ImGui::EndTabBar();
	}

	ImGui::End();
}
