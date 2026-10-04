// Transliterator.hpp - the whole F8/F9/F10 action on one key press
#pragma once

// Implementation-specific headers
#include "cst/FormattedText/FormattedText.hpp"

// Standard library headers
#include <functional>
#include <string>
#include <utility>

// Windows system headers
#include <Windows.h>


// ====================================================================
//  Transliterator - select text, transliterate it into the next layout
//  of the switching cycle, paste it back over the selection
// ====================================================================

class Transliterator
{
public:
	// where the log lines of this class go
	using log_type = std::function<void(const char* line)>;

	explicit Transliterator(log_type log) :
		m_log(std::move(log))
	{}

	// how the owned keys spell themselves in the tray menu
	static std::wstring gestureHint();

	// the key filter the hook asks about every key stroke
	static bool isOwnKey(BYTE vk);

	// reads layout.txt, without it nothing gets transliterated
	void load(const std::wstring& dir);

	// one press: the selection gesture, the fix, the layout switch
	void run(BYTE vk, bool switchLayout, bool keepSelection);

private:
	// the F-keys we own, the selection gesture each one runs before the fix,
	// and how both are spelled in the tray menu and in the log
	struct Gesture
	{
		BYTE           vk;
		const wchar_t* name;
		const wchar_t* what;
		WORD           key;   // 0 = work with whatever is selected already
		bool           ctrl;
		bool           shift;
	};

	static constexpr Gesture kGestures[] = {
		{ static_cast<BYTE>(VK_F8),  L"F8",  L"selection", 0,       false, false },
		{ static_cast<BYTE>(VK_F9),  L"F9",  L"word",      VK_LEFT, true,  true  },
		{ static_cast<BYTE>(VK_F10), L"F10", L"line",      VK_HOME, false, true  },
	};

	// how long the target app is given to react to an injected keystroke, ms
	static constexpr DWORD kCopySettleMs    = 180;
	static constexpr DWORD kGestureSettleMs = 120;
	static constexpr DWORD kPasteSettleMs   = 120;

	static const Gesture* gestureOf(BYTE vk);

	// the HKL that follows <current> in the system layout cycle, or 0
	static HKL nextLayout(HKL current);

	// resolve an HKL's language to a dictionary column index, or -1
	static int languageOfLayout(HKL hkl);

	// empty the clipboard first, so a non-empty one after Ctrl+C proves the
	// focused control really had a selection to copy
	static std::wstring copySelection();

	void logTarget(HWND target);
	void transliterateSelection(bool keepSelection);
	void toggleKeyboardLayout();

	template <typename... Args>
	void log(const char* fmt, Args&&... args)
	{
		const cst::FormattedTextA<4096> body{ fmt, std::forward<Args>(args)... };
		m_log(body.c_str());
	}

	log_type m_log;

};  // class Transliterator