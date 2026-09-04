// MetadataDescriptor.Win32.hpp
#pragma once

// Implementation-specific headers
#include "cst/FormattedRecord/FormattedRecord.hpp"

// Standard library headers
#include <string>

// Windows system headers
#include <Windows.h>

// Specialization of MetadataDescriptor for Windows error codes (DWORD)
namespace cst
{
	template<>
	struct MetadataDescriptor<DWORD>
	{
		ErrorMetadata operator()(DWORD code) const
		{
			ErrorMetadata meta{};
			meta.source = "Win32";
			meta.code   = std::to_string(static_cast<unsigned long long>(code));
			meta.what   = win32_message(code);
			return meta;
		}

	private:
		static std::string win32_message(DWORD code)
		{
			LPWSTR buf = nullptr;

			const DWORD flags =
				FORMAT_MESSAGE_ALLOCATE_BUFFER |
				FORMAT_MESSAGE_FROM_SYSTEM |
				FORMAT_MESSAGE_IGNORE_INSERTS;

			const DWORD len = ::FormatMessageW(
				flags,
				nullptr,
				code,
				MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
				reinterpret_cast<LPWSTR>(&buf),
				0,
				nullptr
			);

			auto free_buf = [&] {
				if (buf) {
					::LocalFree(buf);
					buf = nullptr;
				}
				};

			if (len == 0 || buf == nullptr) {
				free_buf();
				return "Unknown Win32 error";
			}

			const int needed = ::WideCharToMultiByte(
				CP_UTF8, 0,
				buf, static_cast<int>(len),
				nullptr, 0,
				nullptr, nullptr
			);

			std::string msg;
			if (needed > 0) {
				msg.resize(static_cast<std::size_t>(needed));
				::WideCharToMultiByte(
					CP_UTF8, 0,
					buf, static_cast<int>(len),
					msg.data(), needed,
					nullptr, nullptr
				);

				while (!msg.empty() and (msg.back() == '\r' || msg.back() == '\n')) {
					msg.pop_back();
				}
			}
			else {
				msg = "Unknown Win32 error";
			}

			free_buf();
			return msg;
		}
	};

} // namespace cst
