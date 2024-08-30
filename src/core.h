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
	void ChangeIP() noexcept;
}

namespace globals
{
	inline std::mutex cout_mutex;
	inline std::mutex queue_mutex;
	inline std::condition_variable cv;
	inline std::atomic<bool> done(false);
	inline std::queue<std::string> file_queue;
	inline std::queue<std::string> dir_queue;
	inline std::vector<std::string> pruneDirectories = {
		R"((C:\Windows\SoftwareDistribution\))",
		R"((C:\Windows\Temp))",
		R"((%USERPROFILE%\AppData\Local\Temp))"
	};

	inline bool reserveThreads = true;
	inline bool logPruneTime = true;
}
