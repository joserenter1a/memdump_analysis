// Dump2Picture version 2.2.3 (c) Software Diagnostics Institute 
// GNU GENERAL PUBLIC LICENSE 
// http://www.gnu.org/licenses/gpl-3.0.txt

#define NOMINMAX

#include <memory>
#include <string>
#include <iostream>
#include <windows.h>

BITMAPFILEHEADER bmfh = { 'MB', 0, 0, 0, sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER) };
BITMAPINFOHEADER bmih = { sizeof(BITMAPINFOHEADER), 0, 0, 1, 32, 0, 0, 0, 0, 0, 0 };
RGBQUAD rgb[256];

void ReportError (const std::wstring& strPrefix)
{
	std::wcout << strPrefix << " ";
	
	DWORD gle = ::GetLastError();

	if (LPWSTR errMessage{}; gle &&
		::FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM, nullptr, gle, 0, reinterpret_cast<LPWSTR>(&errMessage), 0, nullptr) && errMessage)
	{
		std::wcout << errMessage;
		::LocalFree(errMessage);
	}		

	std::wcout << std::endl;
}

auto hDeleter = [](auto ph) { if (ph && (*ph != INVALID_HANDLE_VALUE)) ::CloseHandle(*ph); };
using Handle = std::unique_ptr<HANDLE, decltype(hDeleter)>;

class RAII_HANDLE : public Handle 
{
public:
	RAII_HANDLE(HANDLE _h) : Handle(&h, hDeleter), h(_h) {};
	void operator= (HANDLE _h) { Handle::reset(); h = _h; Handle::reset(&h); }
	operator HANDLE() const { return h; }
private:
	HANDLE h;
};

int wmain(int argc, wchar_t* argv[])
{
	enum ARGS { DUMPFILE = 1, BMPFILE, REQUIRED, BITCOUNT = REQUIRED };

	std::wcout << std::endl << "Dump2Picture version 2.2.3" << std::endl << "Written by Dmitry Vostokov, Copyright (c) 2007 - 2018 Software Diagnostics Institute." << std::endl << std::endl;
	
	if (argc < ARGS::REQUIRED)
	{
		std::wcout << "Usage: Dump2Picture DumpFile BmpFile [8|16|24|32]" << std::endl;
		return -1;
	}

	if (argc > ARGS::REQUIRED)
	{
		try
		{
			bmih.biBitCount = std::stoi(argv[BITCOUNT]);
		}
		catch (const std::exception&)
		{
			std::wcout << "Invalid bit count value." << std::endl;
			return -1;
		}

		switch (bmih.biBitCount)
		{
		case 8:
			{
				decltype(RGBQUAD::rgbBlue) i = 0;

				for (auto& el : rgb)
				{
					el.rgbBlue = el.rgbGreen = el.rgbRed = i++;
					el.rgbReserved = 0;
				}
			}
		case 16:
		case 24:
		case 32:
			break;
		default:
			std::wcout << "Invalid bit count value." << std::endl;
			return -1;
		}
	}

	RAII_HANDLE hFile = ::CreateFile(argv[ARGS::DUMPFILE], GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);

	if (hFile == INVALID_HANDLE_VALUE)
	{
		ReportError(L"Cannot read dump file.");	
		return -1;
	}

	ULARGE_INTEGER qwDumpSize{};
	qwDumpSize.LowPart = ::GetFileSize(hFile, &qwDumpSize.HighPart);

	bmih.biWidth = bmih.biHeight = static_cast<decltype(bmih.biWidth)>(std::trunc(std::sqrt((qwDumpSize.QuadPart/(bmih.biBitCount/8)))));
	bmih.biWidth -= bmih.biWidth%2;

	if (bmih.biBitCount == 8)
	{
		bmih.biWidth -= bmih.biWidth%8;
	}

	bmih.biHeight -= bmih.biHeight%2;
	bmih.biHeight = -bmih.biHeight;

	if (bmih.biBitCount == 8)
	{
		bmfh.bfOffBits += sizeof(rgb);
	}

	auto fileSize = static_cast<unsigned long long>(bmih.biWidth)*bmih.biHeight*(bmih.biBitCount / 8);

	bmih.biSizeImage = ((fileSize + bmfh.bfOffBits) > std::numeric_limits<decltype(bmih.biSizeImage)>::max()) ? 0 : static_cast<decltype(bmih.biSizeImage)>(fileSize);

	bmfh.bfSize = (!bmih.biSizeImage) ? 0 : bmfh.bfOffBits + bmih.biSizeImage; 

	hFile = ::CreateFile(argv[ARGS::BMPFILE], GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);

	if (hFile == INVALID_HANDLE_VALUE)
	{
		ReportError(L"Cannot create bitmap header file.");	
		return -1;
	}

	DWORD dwWritten{};

	if (!::WriteFile(hFile, &bmfh, sizeof(bmfh), &dwWritten, nullptr))
	{
		ReportError(L"Cannot write bitmap file header.");	
		return -1;
	}
	 
	if (!::WriteFile(hFile, &bmih, sizeof(bmih), &dwWritten, nullptr))
	{
		ReportError(L"Cannot write bitmap information header.");	
		return -1;
	}

	if (bmih.biBitCount == 8)
	{
		if (!::WriteFile(hFile, &rgb, sizeof(rgb), &dwWritten, nullptr))
		{
			ReportError(L"Cannot write bitmap rgb values.");	
			return -1;
		}
	}

	hFile.reset();

	using namespace std::string_literals;

	if (_wsystem((L"copy \""s + argv[ARGS::BMPFILE] + L"\" /B + \"" + argv[ARGS::DUMPFILE] + L"\" /B \"" + argv[ARGS::BMPFILE] + L"\" /B").c_str()) == -1)
	{
		ReportError(L"Cannot copy dump data.");
		return -1;
	}
	
	return 0;
}
