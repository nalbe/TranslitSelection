// Transliterator.cpp - the whole F8/F9/F10 action on one key press
#include "Transliterator.hpp"

// Implementation-specific headers
#include "cst/winapi/ClipboardManager/ClipboardManager.hpp"
#include "LayoutMap.hpp"

// Standard library headers
#include <vector>

namespace
{
	// Scancodes, not bare virtual keys: some toolkits (Gecko) drop injections
	// that carry no scancode. Every keystroke we send - a bare key, a Ctrl
	// chord, Shift+Left N times - is this one call.
	void chord(WORD key, bool ctrl = false, bool shift = false, int repeat = 1)
	{
		auto stroke = [](WORD vk, bool up)
		{
			INPUT in {};
			in.type       = INPUT_KEYBOARD;
			in.ki.wVk     = vk;
			in.ki.wScan   = static_cast<WORD>(::MapVirtualKeyW(vk, MAPVK_VK_TO_VSC));
			in.ki.dwFlags = KEYEVENTF_SCANCODE | (up ? KEYEVENTF_KEYUP : 0u);
			return in;
		};

		std::vector<INPUT> seq;
		seq.reserve(2 + 2 * static_cast<size_t>(repeat));

		if (ctrl)  seq.push_back(stroke(VK_CONTROL, false));
		if (shift) seq.push_back(stroke(VK_SHIFT,   false));

		for (int i = 0; i < repeat; ++i) {
			seq.push_back(stroke(key, false));
			seq.push_back(stroke(key, true));
		}

		if (shift) seq.push_back(stroke(VK_SHIFT,   true));
		if (ctrl)  seq.push_back(stroke(VK_CONTROL, true));

		::SendInput(static_cast<UINT>(seq.size()), seq.data(), sizeof(INPUT));
	}

	// the log file is UTF-8, so cyrillic survives
	std::string toUtf8(const wchar_t* text, int len)
	{
		if (!text || len <= 0) {
			return {};
		}
		const int n = ::WideCharToMultiByte(CP_UTF8, 0, text, len, nullptr, 0, nullptr, nullptr);
		if (n <= 0) {
			return {};
		}
		std::string out(static_cast<size_t>(n), '\0');
		::WideCharToMultiByte(CP_UTF8, 0, text, len, out.data(), n, nullptr, nullptr);
		return out;
	}

	std::string toUtf8(const std::wstring& text)
	{
		return toUtf8(text.c_str(), static_cast<int>(text.size()));
	}

}  // namespace


std::wstring Transliterator::gestureHint()
{
	std::wstring hint;
	for (auto const& g : kGestures) {
		if (!hint.empty()) hint += L" / ";
		hint += g.what;
		hint += L": ";
		hint += g.name;
	}
	return hint;
}

bool Transliterator::isOwnKey(BYTE vk)
{
	return gestureOf(vk) != nullptr;
}

void Transliterator::load(const std::wstring& dir)
{
	if (!kf::LayoutMap::load(dir + L"layout.txt")) {
		log("layout.txt absent or malformed: translit disabled");
		return;
	}

	std::string langs;
	for (auto const& lang : kf::LayoutMap::languages()) {
		if (!langs.empty()) langs += ' ';
		langs += toUtf8(lang);
	}
	log("layout.txt: %d columns: %s", (int)kf::LayoutMap::languages().size(), langs.c_str());
}

// vk is the swallowed key, passed via the posted message (wParam)
void Transliterator::run(BYTE vk, bool switchLayout, bool keepSelection)
{
	const Gesture* g = gestureOf(vk);
	if (!g) {
		log("--- HOTKEY vk=0x%02X: not an owned key ---", vk);
		return;
	}
	log("--- HOTKEY %ls (%ls) ---", g->name, g->what);

	if (g->key) {
		log("%ls: selecting %ls", g->name, g->what);
		chord(g->key, g->ctrl, g->shift);
		::Sleep(kGestureSettleMs);
	}

	const cst::winapi::ClipboardSnapshot saved = cst::winapi::ClipboardManager::CaptureAll();
	transliterateSelection(keepSelection);
	cst::winapi::ClipboardManager::RestoreAll(saved);

	if (switchLayout) {
		toggleKeyboardLayout();
	}
}

// the user never leaves the target app, so keyboard focus stays inside it;
// a real Ctrl+C / Ctrl+V therefore lands in the right window with no
// focus-juggling at all
void Transliterator::transliterateSelection(bool keepSelection)
{
	HWND target = ::GetForegroundWindow();
	logTarget(target);

	const DWORD targetTid = target ? ::GetWindowThreadProcessId(target, nullptr) : 0;
	if (!targetTid) {
		log("transliterate: no target thread, aborting");
		return;
	}

	std::wstring text = copySelection();
	if (text.empty()) {
		log("transliterate: clipboard empty after Ctrl+C, nothing to fix");
		return;
	}
	log("transliterate: copied %d chars", (int)text.size());

	// the text was typed in the ACTIVE layout of the target window (from),
	// and belongs in the NEXT layout in the switching cycle (to). this
	// scales to any number of installed layouts.
	const HKL targetLayout = ::GetKeyboardLayout(targetTid);
	const int from = languageOfLayout(targetLayout);
	const int to   = languageOfLayout(nextLayout(targetLayout));
	if (from < 0 || to < 0) {
		log("transliterate: layout(s) missing from layout.txt (from=%d to=%d), aborting", from, to);
		return;
	}

	std::wstring fixed = kf::LayoutMap::transliterate(text, from, to);
	log("transliterate: in ='%s'", toUtf8(text).c_str());
	log("transliterate: out='%s'", toUtf8(fixed).c_str());

	// pasted unconditionally: the gesture left the caret at the start of
	// the selection, and a paste always parks it at the end - which is
	// exactly where the caret started out
	cst::winapi::ClipboardManager::SetText(fixed);
	chord('V', true);
	::Sleep(kPasteSettleMs);

	// re-select the pasted text from the cursor, which sits at its end
	if (keepSelection) {
		chord(VK_LEFT, false, true, static_cast<int>(fixed.size()));
		log("selection re-highlighted via keyboard");
	}

	log("transliterate: DONE");
}

void Transliterator::logTarget(HWND target)
{
	wchar_t cls[128] = {};
	const int len = target ? ::GetClassNameW(target, cls, 128) : 0;
	log("target HWND=%p tid=%lu class='%s'",
		static_cast<void*>(target),
		target ? ::GetWindowThreadProcessId(target, nullptr) : 0,
		len ? toUtf8(cls, len).c_str() : "none");
}

// toggle the foreground window's keyboard layout to the next one. a
// WM_INPUTLANGCHANGEREQUEST works in GUI apps, but the classic console
// ignores it.
void Transliterator::toggleKeyboardLayout()
{
	HWND hwnd = ::GetForegroundWindow();
	if (!hwnd) {
		log("toggleKeyboardLayout: no foreground window");
		return;
	}

	DWORD threadId = ::GetWindowThreadProcessId(hwnd, nullptr);
	HKL current    = ::GetKeyboardLayout(threadId);
	HKL next       = nextLayout(current);
	if (!next) {
		log("toggleKeyboardLayout: <2 layouts installed");
		return;
	}

	::PostMessageW(hwnd, WM_INPUTLANGCHANGEREQUEST, 0, reinterpret_cast<LPARAM>(next));

	log("toggleKeyboardLayout: current=0x%04X next=0x%04X",
		static_cast<unsigned>(LOWORD(current)), static_cast<unsigned>(LOWORD(next)));
}

std::wstring Transliterator::copySelection()
{
	cst::winapi::ClipboardManager::ClearClipboardData();
	chord('C', true);
	::Sleep(kCopySettleMs);
	return cst::winapi::ClipboardManager::GetText();
}

const Transliterator::Gesture* Transliterator::gestureOf(BYTE vk)
{
	for (auto const& g : kGestures) {
		if (g.vk == vk) {
			return &g;
		}
	}
	return nullptr;
}

HKL Transliterator::nextLayout(HKL current)
{
	UINT count = ::GetKeyboardLayoutList(0, nullptr);
	if (count < 2) return 0;
	std::vector<HKL> layouts(count);
	::GetKeyboardLayoutList(count, layouts.data());
	for (UINT i = 0; i < count; ++i) {
		if (layouts[i] == current) {
			return layouts[(i + 1) % count];
		}
	}
	return 0;
}

// matches by ISO 639 language code (En/Ru/Fr...), falling back to the raw
// LANGID hex, so a column header can be "en", "en-US", "En" etc.
int Transliterator::languageOfLayout(HKL hkl)
{
	if (!hkl) return -1;
	WORD langid = LOWORD(hkl);

	wchar_t buf[32] = {};
	if (::GetLocaleInfoW(MAKELCID(langid, SORT_DEFAULT), LOCALE_SISO639LANGNAME, buf, 32) > 0) {
		const int idx = kf::LayoutMap::languageIndex(buf);
		if (idx >= 0) return idx;
	}

	wchar_t hex[8];
	swprintf_s(hex, L"%04X", static_cast<unsigned>(langid));
	return kf::LayoutMap::languageIndex(hex);
}