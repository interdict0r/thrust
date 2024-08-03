#pragma once
#include <atomic>
#include <condition_variable>
#include <iostream>
#include <mutex>
#include <queue>
#include <string>
#include <thread>
#include <vector>
#include <Windows.h>

#include "../imgui/imgui.h"
#include "../imgui/imgui_impl_dx9.h"
#include "../imgui/imgui_impl_win32.h"
#include "../imgui/imgui_internal.h"

namespace core
{
	BOOL IsProcessElevated() noexcept;
	void WorkerThread() noexcept;
	void DeleteAllFiles(const std::string& strPath) noexcept;
	void StartDeletionThreads(const std::vector<std::string>& paths) noexcept;
	void Initiate() noexcept;
}