#include <Windows.h>
#include <iostream>
#include <string>
#include <thread>
#include <vector>
#include <mutex>

std::mutex cout_mutex;

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


void DeleteAllFiles(const std::string& strPath) noexcept
{
	WIN32_FIND_DATA wfd;
	std::string strSpec = strPath + "*.*";
	HANDLE hFile = FindFirstFile(strSpec.c_str(), &wfd);

	if (hFile != INVALID_HANDLE_VALUE)
	{
		std::vector<std::thread> threads;

		do
		{
			std::string strFile = wfd.cFileName;
			std::string strPathFile = strPath + strFile;
			DWORD dwFileAttribute = GetFileAttributes(strPathFile.c_str());

			if (dwFileAttribute & FILE_ATTRIBUTE_READONLY)
			{
				dwFileAttribute &= ~FILE_ATTRIBUTE_READONLY;
				SetFileAttributes(strPathFile.c_str(), dwFileAttribute);
			}

			if (wfd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
			{
				if (strcmp(strFile.c_str(), ".") != 0 && strcmp(strFile.c_str(), "..") != 0)
				{
					strPathFile += "\\";
					threads.emplace_back(DeleteAllFiles, strPathFile);

					std::lock_guard<std::mutex> lock(cout_mutex);
					if (RemoveDirectory(strPathFile.c_str()))
						std::cout << "Deleted directory: [" << strPathFile << "]" << '\n';
					else
						std::cerr << "Failed to delete directory: [" << strPathFile << "]" << '\n';
				}
			}
			else
			{
				threads.emplace_back([strPathFile]()
				{
					std::lock_guard<std::mutex> lock(cout_mutex);
					if (DeleteFile(strPathFile.c_str()))
						std::cout << "Deleted file: [" << strPathFile << "]" << '\n';
					else
						std::cerr << "Failed to delete file: [" << strPathFile << "]" << '\n';
				});
			}
		}
		while (FindNextFile(hFile, &wfd));

		FindClose(hFile);

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
	IsProcessElevated();

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
		std::cout << "      THRUST - Optimization Service\n------------------------\n";
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
