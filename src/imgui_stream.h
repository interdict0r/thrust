#pragma once
#include "core.h"


class ImGuiBuffer : public std::streambuf
{
public:
	ImGuiBuffer()
	{
		setp(buffer, buffer + sizeof(buffer) - 1);
	}

protected:
	int overflow(int c) override
	{
		if(c == '\n')
		{
			std::lock_guard<std::mutex> guard(mutex);
			lines.push_back(std::string(pbase(), pptr()));
			setp(buffer, buffer + sizeof(buffer - 1));
		}
		else
		{
			*pptr() = c;
			pbump(1);
		}

		return c;
	}

	int sync() override
	{
		return overflow('\n');
	}

public:
	void GetLines(std::vector<std::string>& outputLines)
	{
		std::lock_guard<std::mutex> guard(mutex);
        outputLines = lines;
	}

private:
	char buffer[1024];
	std::vector<std::string> lines;
	std::mutex mutex;
};

class ImGuiStream : public std::ostream
{
public:
	ImGuiStream() : std::ostream(&buffer) {}
	void GetLines(std::vector<std::string>& outputLines) { buffer.GetLines(outputLines); }

private:
	ImGuiBuffer buffer;
};
