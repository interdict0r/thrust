#include "core.h"


BOOL core::IsProcessElevated() noexcept
{
	BOOL fIsElevated = false;
	HANDLE hToken = nullptr;
	TOKEN_ELEVATION elevation;
	DWORD dwSize;

	if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &hToken))
		goto Cleanup;

	if (!GetTokenInformation(hToken, TokenElevation, &elevation, sizeof(elevation), &dwSize))
		goto Cleanup;

	fIsElevated = static_cast<BOOL>(elevation.TokenIsElevated);

Cleanup:
	if (hToken)
	{
		CloseHandle(hToken);
		hToken = nullptr;
	}

	return fIsElevated;
}

void core::WorkerThread() noexcept
{
	using namespace globals;

	while (true)
	{
		std::string path;
		{
			std::unique_lock<std::mutex> lock(queue_mutex);
			cv.wait(lock, [] { return !file_queue.empty() || !dir_queue.empty() || done.load(); });

			if (file_queue.empty() && dir_queue.empty() && done.load())
				return;

			if (!file_queue.empty())
			{
				path = file_queue.front();
				file_queue.pop();
			}
			else if (!dir_queue.empty())
			{
				path = dir_queue.front();
				dir_queue.pop();
			}
		}

		if (!path.empty())
		{
			DWORD dwFileAttribute = GetFileAttributes(path.c_str());

			if (dwFileAttribute & FILE_ATTRIBUTE_READONLY)
			{
				dwFileAttribute &= ~FILE_ATTRIBUTE_READONLY;
				SetFileAttributes(path.c_str(), dwFileAttribute);
			}

			if (dwFileAttribute & FILE_ATTRIBUTE_DIRECTORY)
			{
				if (RemoveDirectory(path.c_str()))
				{
					std::lock_guard<std::mutex> lock(cout_mutex);
					std::cout << "deleted directory: [" << path << "]" << '\n';
				}
				else
				{
					std::lock_guard<std::mutex> lock(cout_mutex);
					std::cerr << "failed to delete directory: [" << path << "]" << '\n';
				}
			}
			else
			{
				if (DeleteFile(path.c_str()))
				{
					std::lock_guard<std::mutex> lock(cout_mutex);
					std::cout << "deleted file: [" << path << "]" << '\n';
				}
				else
				{
					std::lock_guard<std::mutex> lock(cout_mutex);
					std::cerr << "failed to delete file: [" << path << "]" << '\n';
				}
			}
		}
	}
}

void core::DeleteAllFiles(const std::string& strPath) noexcept
{
	using namespace globals;

	WIN32_FIND_DATA wfd;
	std::string strSpec = strPath + "\\*.*";
	HANDLE hFile = FindFirstFile(strSpec.c_str(), &wfd);

	if (hFile != INVALID_HANDLE_VALUE)
	{
		std::vector<std::thread> threads;
		const unsigned int num_threads = std::thread::hardware_concurrency();

		if (reserveThreads)
			threads.reserve(num_threads);

		for (int i = 0; i < num_threads; i++)
			threads.emplace_back(WorkerThread);

		do
		{
			std::string strFile = wfd.cFileName;
			std::string strPathFile = strPath + "\\" + strFile;

			if (wfd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
			{
				if (strcmp(strFile.c_str(), ".") != 0 && strcmp(strFile.c_str(), "..") != 0)
				{
					std::string nestedPath = strPathFile + "\\";
					DeleteAllFiles(nestedPath);
					{
						std::lock_guard<std::mutex> lock(queue_mutex);
						dir_queue.push(strPathFile);
					}
					cv.notify_one();
				}
			}
			else
			{
				std::lock_guard<std::mutex> lock(queue_mutex);
				file_queue.push(strPathFile);
				cv.notify_one();
			}
		}
		while (FindNextFile(hFile, &wfd));
		FindClose(hFile);

		done.store(true);
		cv.notify_all();

		for (auto& thread : threads)
		{
			if (thread.joinable())
				thread.join();
		}
	}
}

void core::StartDeletionThreads(const std::vector<std::string>& paths) noexcept
{
	std::vector<std::thread> threads;
	if (globals::reserveThreads)
		threads.reserve(paths.size());

	for (const auto& path : paths)
		threads.emplace_back(DeleteAllFiles, path);

	for (auto& thread : threads)
	{
		if (thread.joinable())
			thread.join();
	}
}

void core::Initiate() noexcept
{
	char szPath[MAX_PATH];
	GetTempPath(MAX_PATH, szPath);

	std::vector<std::string> paths = {szPath};
	paths.insert(paths.end(), globals::pruneDirectories.begin(), globals::pruneDirectories.end());

	StartDeletionThreads(paths);
}

void core::ChangeIP() noexcept
{
	system("ipconfig /release");
	system("ipconfig /renew");
}
