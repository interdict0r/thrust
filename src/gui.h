#pragma once
#include <d3d9.h>

namespace gui
{
	// window size
	constexpr int WIDTH = 635;
	constexpr int HEIGHT = 435;

	inline bool exit = true;

	// winapi vars
	inline HWND window = nullptr;
	inline WNDCLASSEXA windowClass = { };

	// window movement points
	inline POINTS position = { };

	// dx statevar
	inline PDIRECT3D9 d3d = nullptr;
	inline LPDIRECT3DDEVICE9 device = nullptr;
	inline D3DPRESENT_PARAMETERS presentParameters = { };

	// window creation & destruction
	void CreateHWindow(
		const char* windowName,
		const char* className) noexcept;
	void DestroyHWindow() noexcept;

	// window creation & destruction
	bool CreateDevice() noexcept;
	void ResetDevice() noexcept;
	void DestroyDevice() noexcept;

	// handle imgui creation
	void CreateImGui() noexcept;
	void DestroyImGui() noexcept;

	void BeginRender() noexcept;
	void EndRender() noexcept;
	void Render() noexcept;
} 