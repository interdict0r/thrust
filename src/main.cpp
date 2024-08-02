#include "../mgr/mgr.h"

namespace globals
{
	std::mutex cout_mutex;
	std::mutex queue_mutex;
	std::condition_variable cv;
	std::atomic<bool> done(false);
	std::queue<std::string> file_queue;
	std::queue<std::string> dir_queue;
}

BOOL IsProcessElevated()
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

void WorkerThread() noexcept
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
					std::cout << "Deleted directory: [" << path << "]" << '\n';
				}
				else
				{
					std::lock_guard<std::mutex> lock(cout_mutex);
					std::cerr << "Failed to delete directory: [" << path << "]" << '\n';
				}
			}
			else
			{
				if (DeleteFile(path.c_str()))
				{
					std::lock_guard<std::mutex> lock(cout_mutex);
					std::cout << "Deleted file: [" << path << "]" << '\n';
				}
				else
				{
					std::lock_guard<std::mutex> lock(cout_mutex);
					std::cerr << "Failed to delete file: [" << path << "]" << '\n';
				}
			}
		}
	}
}

void DeleteAllFiles(const std::string& strPath) noexcept
{
	using namespace globals;

	WIN32_FIND_DATA wfd;
	std::string strSpec = strPath + "\\*.*";
	HANDLE hFile = FindFirstFile(strSpec.c_str(), &wfd);

	if (hFile != INVALID_HANDLE_VALUE)
	{
		std::vector<std::thread> threads;
		const unsigned int num_threads = std::thread::hardware_concurrency();

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


void StartDeletionThreads(const std::vector<std::string>& paths) noexcept
{
	std::vector<std::thread> threads;

	for (const auto& path : paths)
		threads.emplace_back(DeleteAllFiles, path);

	for (auto& thread : threads)
	{
		if (thread.joinable())
			thread.join();
	}
}


int main(void*)
{
	SetConsoleTitleW(L"THRUST");

	if (!IsProcessElevated())
	{
		MessageBoxA(nullptr, "This process must be run as administrator.", "THRUST", MB_ICONERROR | MB_OK);
		return 0;
	}

	int idx;
	char szPath[MAX_PATH];
	GetTempPath(MAX_PATH, szPath);

	while (true)
	{
		std::cout << "      THRUST Optimization Service\n------------------------\n";
		std::cout << " 1 - Exit\n 2 - CleanMgr\n 3 - ChkDsk (doesn't work as administrator)\n 4 - Prune\n";
		std::cout << "Enter corresponding character.\n";
		std::cin >> idx;

		switch (idx)
		{
		case 1:
			system("cls");
			std::cout << "Exiting";
			return 0;

		case 2:
			system("cls");
			system("echo @echo off > temp.bat");
			system("echo echo Starting dfrgui. >> temp.bat");
			system("echo dfrgui >> temp.bat");
			system("echo Starting cleanmgr. >> temp.bat");
			system("echo cleanmgr /D C >> temp.bat");
			system("temp.bat");
			std::cout << "Exiting.\n";
			system("del temp.bat");
			break;

		case 3:
			system("cls");
			system("chkdsk /f C:");
			break;

		case 4:
			system("cls");
			StartDeletionThreads({
				szPath, R"(C:\Windows\SoftwareDistribution\)", R"(C:\Windows\Temp\)",
				R"(%USERPROFILE%\AppData\Local\Temp)"
			});
			system("echo Completed.");
			system("echo Stopping Windows Update Service...");
			system("net stop wuauserv");
			break;

		default:
			system("echo Incorrect value entered. && echo Exiting...");
			break;
		}

		system("pause");
	}
}
