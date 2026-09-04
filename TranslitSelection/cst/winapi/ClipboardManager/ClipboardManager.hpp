// ClipboardManager.hpp
#pragma once

// Implementation-specific headers
#include "cst/ErrorCollector/ErrorCollector.hpp"
#include "cst/FormattedRecord/ErrorRecord.Win32.hpp"

// Standard library headers
#include <string>
#include <vector>

// Windows system headers
#include <Windows.h>



namespace cst::winapi
{

	// ====================================================================================
	//  ClipboardSnapshot - an in-memory copy of every clipboard format.
	//  Lets us clobber the clipboard (EmptyClipboard + paste translit) and then
	//  put it back exactly as it was, including non-text formats such as
	//  CF_HDROP (files copied in Explorer), images, HTML, etc.
	// ====================================================================================
	struct ClipboardSnapshot
	{
		struct Format
		{
			UINT       format = 0;
			HGLOBAL    hMem   = nullptr;

			Format() = default;
			Format(const Format&) = delete;
			Format(Format&& o) noexcept : format(o.format), hMem(o.hMem) { o.hMem = nullptr; }
			Format& operator=(Format&& o) noexcept
			{
				if (this != &o) { release(); format = o.format; hMem = o.hMem; o.hMem = nullptr; }
				return *this;
			}
			~Format() { release(); }

			void release() { if (hMem) { ::GlobalFree(hMem); hMem = nullptr; } }
		};

		std::vector<Format> formats;

		ClipboardSnapshot() = default;
		ClipboardSnapshot(const ClipboardSnapshot&) = delete;
		ClipboardSnapshot(ClipboardSnapshot&&) = default;
		ClipboardSnapshot& operator=(ClipboardSnapshot&&) = default;

		bool empty() const noexcept { return formats.empty(); }
		void clear() noexcept { formats.clear(); }
	};

	// ====================================================================================
	//  ClipboardManager - manages the clipboard data
	// ====================================================================================
	struct ClipboardManager
	{
		// -- members -------------------------------------------------------
	private:
		inline static ErrorCollector m_errors;

	public:
		// Attempts to open the clipboard with retry logic on access denial
		static bool TryOpen()
		{
			BOOL bResult = OpenClipboard(NULL);
			if (bResult) {
				return true;
			}

			DWORD dwError = GetLastError();
			if (dwError == ERROR_ACCESS_DENIED) {
				INT nRetries = 4;
				UINT uDelay = 50;  // ms
				do {
					Sleep(uDelay);
					uDelay *= 2;
					if (OpenClipboard(NULL)) {
						return true;
					}
				} while (nRetries-- > 0);

				m_errors.emplace_back(
					FormattedRecord{ ErrorLevel::Error, "" }, dwError
				);
			}
			else {
				m_errors.emplace_back(
					FormattedRecord{ ErrorLevel::Error, "" }, dwError
				);
			}
			return bResult;
		}

		// Retrieves the executable name of the process that owns the clipboard
		static std::wstring GetOwnerName()
		{
			TCHAR szExePath[MAX_PATH];
			HWND hClipboardOwner = GetClipboardOwner();
			if (!hClipboardOwner) {
				m_errors.emplace_back(
					FormattedRecord{ ErrorLevel::Info, "No window currently owns the clipboard" }
				);
				return L"";
			}

			DWORD dwProcessId{};
			GetWindowThreadProcessId(hClipboardOwner, &dwProcessId);
			if (!dwProcessId) {
				m_errors.emplace_back(
					FormattedRecord{ ErrorLevel::Error, "" }, GetLastError()
				);
				return L"";
			}

			HANDLE hProcess = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, dwProcessId);
			if (!hProcess) {
				m_errors.emplace_back(
					FormattedRecord{ ErrorLevel::Error, "" }, GetLastError()
				);
				return L"";
			}

			DWORD dwPathSize = MAX_PATH;
			BOOL bSuccess = QueryFullProcessImageName(hProcess, 0, szExePath, &dwPathSize);
			if (!bSuccess) {
				m_errors.emplace_back(
					FormattedRecord{ ErrorLevel::Error, "" }, GetLastError()
				);
				CloseHandle(hProcess);
				return L"";
			}
			CloseHandle(hProcess);

			LPCTSTR cszExeName = wcsrchr(szExePath, L'\\');
			return cszExeName
				? (cszExeName + 1)
				: szExePath;
		}

		// Gets a copy of the clipboard text (Unicode)
		static std::wstring GetText()
		{
			std::wstring text;
			if (!TryOpen()) {
				return text;
			}

			if (!IsClipboardFormatAvailable(CF_UNICODETEXT)) {
				CloseClipboard();
				return text;
			}

			HGLOBAL hData = GetClipboardData(CF_UNICODETEXT);
			if (!hData) {
				m_errors.emplace_back(
					FormattedRecord{ ErrorLevel::Error, "" }, GetLastError()
				);
				CloseClipboard();
				return text;
			}

			wchar_t* pData = static_cast<wchar_t*>(GlobalLock(hData));
			if (!pData) {
				m_errors.emplace_back(
					FormattedRecord{ ErrorLevel::Error, "" }, GetLastError()
				);
				CloseClipboard();
				return text;
			}

			text = pData;
			GlobalUnlock(hData);

			CloseClipboard();
			return text;
		}

		// Opens the clipboard, empties it and places the given text (CF_UNICODETEXT)
		static bool SetText(const std::wstring& text)
		{
			if (!TryOpen()) {
				return false;
			}

			if (!EmptyClipboard()) {
				m_errors.emplace_back(
					FormattedRecord{ ErrorLevel::Error, "" }, GetLastError()
				);
				CloseClipboard();
				return false;
			}

			if (text.empty()) {
				CloseClipboard();
				return true;
			}

			do {
				const size_t byteSize = (text.size() + 1) * sizeof(wchar_t);
				HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, byteSize);
				if (!hMem) {
					m_errors.emplace_back(
						FormattedRecord{ ErrorLevel::Error, "" }, GetLastError()
					);
					break;
				}

				wchar_t* pMem = static_cast<wchar_t*>(GlobalLock(hMem));
				if (!pMem) {
					m_errors.emplace_back(
						FormattedRecord{ ErrorLevel::Error, "" }, GetLastError()
					);
					GlobalFree(hMem);
					break;
				}

				memcpy(pMem, text.c_str(), byteSize);
				GlobalUnlock(hMem);

				if (!SetClipboardData(CF_UNICODETEXT, hMem)) {
					m_errors.emplace_back(
						FormattedRecord{ ErrorLevel::Error, "" }, GetLastError()
					);
					GlobalFree(hMem);
				}
			} while (false);

			CloseClipboard();
			return true;
		}

		static bool IsClipboardHistorySupported() noexcept
		{
			using RtlGetVersion_t = LONG(WINAPI*)(PRTL_OSVERSIONINFOW);
			HMODULE hNtdll = GetModuleHandleW(L"ntdll.dll");
			if (!hNtdll) {
				m_errors.emplace_back(
					FormattedRecord{ ErrorLevel::Error, "" }, GetLastError()
				);
				return false;
			}

			auto pRtlGetVersion = reinterpret_cast<RtlGetVersion_t>(
				GetProcAddress(hNtdll, "RtlGetVersion")
				);
			if (!pRtlGetVersion) {
				m_errors.emplace_back(
					FormattedRecord{ ErrorLevel::Error, "" }, static_cast<DWORD>(ERROR_PROC_NOT_FOUND)
				);
				return false;
			}

			RTL_OSVERSIONINFOW osvi = { sizeof(osvi) };
			if (pRtlGetVersion(&osvi) != 0) {  // STATUS_SUCCESS is 0
				m_errors.emplace_back(
					FormattedRecord{ ErrorLevel::Warning, "RtlGetVersion execution failed" }
				);
				return false;
			}

			//  Windows 10 1809 ( 17763)  
			if (osvi.dwMajorVersion > 10) {
				return true;
			}
			if (osvi.dwMajorVersion == 10 && osvi.dwBuildNumber >= 17763) {
				return true;
			}
			return false;
		}

		// Makes a deep copy of every format currently on the clipboard, so the
		// clipboard can be emptied and later restored with RestoreAll().
		static ClipboardSnapshot CaptureAll()
		{
			ClipboardSnapshot snap;
			if (!TryOpen()) {
				return snap;
			}

			UINT format = 0;
			while ((format = ::EnumClipboardFormats(format)) != 0) {
				HGLOBAL hData = ::GetClipboardData(format);
				if (!hData) {
					continue;
				}

				SIZE_T size = ::GlobalSize(hData);
				if (size == 0) {
					continue;
				}

				const void* pSrc = ::GlobalLock(hData);
				if (!pSrc) {
					continue;
				}

				HGLOBAL hCopy = ::GlobalAlloc(GMEM_MOVEABLE, size);
				if (hCopy) {
					void* pDst = ::GlobalLock(hCopy);
					if (pDst) {
						::memcpy(pDst, pSrc, size);
						::GlobalUnlock(hCopy);

						ClipboardSnapshot::Format f;
						f.format = format;
						f.hMem   = hCopy;
						snap.formats.push_back(std::move(f));
					}
					else {
						::GlobalFree(hCopy);
					}
				}

				::GlobalUnlock(hData);
			}

			CloseClipboard();
			return snap;
		}

		// Empties the clipboard and re-places every format captured by CaptureAll().
		static bool RestoreAll(const ClipboardSnapshot& snap)
		{
			if (!TryOpen()) {
				return false;
			}

			if (!::EmptyClipboard()) {
				m_errors.emplace_back(
					FormattedRecord{ ErrorLevel::Error, "" }, ::GetLastError()
				);
				CloseClipboard();
				return false;
			}

			for (auto const& f : snap.formats) {
				if (!f.hMem) {
					continue;
				}

				SIZE_T size = ::GlobalSize(f.hMem);
				HGLOBAL hCopy = ::GlobalAlloc(GMEM_MOVEABLE, size);
				if (!hCopy) {
					continue;
				}

				void* pDst = ::GlobalLock(hCopy);
				if (!pDst) {
					::GlobalFree(hCopy);
					continue;
				}

				const void* pSrc = ::GlobalLock(f.hMem);
				if (!pSrc) {
					::GlobalUnlock(hCopy);
					::GlobalFree(hCopy);
					continue;
				}

				::memcpy(pDst, pSrc, size);
				::GlobalUnlock(hCopy);
				::GlobalUnlock(f.hMem);

				if (!::SetClipboardData(f.format, hCopy)) {
					m_errors.emplace_back(
						FormattedRecord{ ErrorLevel::Error, "" }, ::GetLastError()
					);
					::GlobalFree(hCopy);
				}
			}

			CloseClipboard();
			return true;
		}

		static bool ClearClipboardData() noexcept
		{
			if (!TryOpen()) {
				return false;
			}

			BOOL bResult = EmptyClipboard();
			if (!bResult) {
				m_errors.emplace_back(
					FormattedRecord{ ErrorLevel::Error, "" }, GetLastError()
				);
			}

			CloseClipboard();
			return true;
		}

	};  // class ClipboardManager

}  // namespace cst::winapi



