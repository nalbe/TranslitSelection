// SingleInstanceGuard.hpp
#pragma once

// Windows system headers
#include <windows.h>
#include <synchapi.h>



namespace cst::winapi
{

	// =============================================================================
	//  SingleInstanceGuard - guard against multiple running instances of the application
	// =============================================================================
	class SingleInstanceGuard final
	{
	private:
		HANDLE m_hMutex;
		bool   m_alreadyRunning;

	public:
		bool is_already_running() const noexcept
		{
			return m_alreadyRunning;
		}

	public:
		~SingleInstanceGuard()
		{
			if (m_hMutex) {
				CloseHandle(m_hMutex);
			}
		}

		SingleInstanceGuard(const wchar_t* mutexName) :
			m_hMutex(NULL),
			m_alreadyRunning(false)
		{
			m_hMutex = ::CreateMutexW(NULL, TRUE, mutexName);
			m_alreadyRunning = (::GetLastError() == ERROR_ALREADY_EXISTS);
		}

		operator bool() const noexcept
		{
			return m_alreadyRunning;
		}

	};  // class SingleInstanceGuard

}  // namespace cst::winapi



