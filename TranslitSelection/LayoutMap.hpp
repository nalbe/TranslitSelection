// LayoutMap.hpp - keyboard layout transliteration between any of N languages.
#pragma once

// Standard library headers
#include <string>
#include <vector>
#include <fstream>
#include <unordered_map>

// Windows system headers
#include <Windows.h>

namespace kf
{

	// ====================================================================
	//  LayoutMap - a multi-column transliteration dictionary loaded from a
	//  single UTF-8 text file.
	//
	//  File format:
	//    line 1        : column headers, the language names tab-separated,
	//                    e.g.  En<TAB>Ru<TAB>Fr
	//    line 2        : empty
	//    line 3+       : one physical key per line, tab-separated characters
	//                    in the SAME column order as line 1, e.g.
	//                    .<TAB>ю<TAB>*
	//
	//  Every line establishes a one-to-one mapping between the characters of
	//  ALL columns at once, so transliteration between any two languages is a
	//  simple O(1) lookup: find the row of a char in the source column, then
	//  read the char at the same row in the target column.
	// ====================================================================
	class LayoutMap
	{
	public:
		// names of the columns (languages), in file order; index = column id
		static const std::vector<std::wstring>& languages()
		{
			return s_languages;
		}

		// index of a language by its name, or -1. matching ignores case and
		// any trailing "-US" style subtag, so "En" matches "en-US".
		static int languageIndex(const std::wstring& name)
		{
			std::wstring base = baseName(name);
			for (size_t i = 0; i < s_languages.size(); ++i) {
				if (equalsIgnoreCase(baseName(s_languages[i]), base)) {
					return static_cast<int>(i);
				}
			}
			return -1;
		}

		// transliterate a single char from column <from> to column <to>.
		// returns 0 if the char has no mapping (the caller keeps the char).
		static wchar_t transliterate(wchar_t ch, int from, int to)
		{
			if (from < 0 || to < 0 || from >= (int)s_rows.size() || to >= (int)s_rows.size()) {
				return 0;
			}
			const auto& idx = s_index[from];
			auto it = idx.find(ch);
			if (it == idx.end()) return 0;
			int row = it->second;
			return s_rows[row][to];
		}

		// transliterate a whole string from column <from> to column <to>.
		static std::wstring transliterate(const std::wstring& text, int from, int to)
		{
			std::wstring result;
			result.reserve(text.size());
			for (wchar_t ch : text) {
				wchar_t mapped = transliterate(ch, from, to);
				result.push_back(mapped ? mapped : ch);
			}
			return result;
		}

		// load a dictionary from <path>. returns false (and keeps whatever was
		// loaded before) if the file is absent/malformed/empty.
		static bool load(const std::wstring& path)
		{
			std::vector<std::wstring> langs;
			std::vector<std::vector<wchar_t>> rows;

			std::ifstream ifs(path, std::ios::binary);
			if (!ifs) return false;

			// read all lines, strip CR, drop blank/#/; lines
			std::vector<std::string> lines;
			std::string line;
			while (std::getline(ifs, line)) {
				if (!line.empty() && line.back() == '\r') line.pop_back();
				// strip a UTF-8 BOM if a line happens to carry one
				if (line.size() >= 3 &&
					(unsigned char)line[0] == 0xEF &&
					(unsigned char)line[1] == 0xBB &&
					(unsigned char)line[2] == 0xBF) {
					line.erase(0, 3);
				}
				if (line.empty() || line[0] == '#' || line[0] == ';') continue;
				lines.push_back(line);
			}
			if (lines.size() < 1) return false;

			// column headers = languages
			langs = splitTabs(lines[0]);
			if (langs.size() < 2) return false;

			// data rows; each must match the column count
			for (size_t i = 1; i < lines.size(); ++i) {
				std::vector<std::wstring> cells = splitTabs(lines[i]);
				if (cells.size() != langs.size()) continue;
				std::vector<wchar_t> row;
				row.reserve(cells.size());
				bool ok = true;
				for (auto const& c : cells) {
					if (c.size() != 1) { ok = false; break; }
					row.push_back(c[0]);
				}
				if (ok) rows.push_back(std::move(row));
			}

			if (rows.empty()) return false;

			// commit
			s_languages = std::move(langs);
			s_rows = std::move(rows);
			rebuildIndex();
			s_loaded = true;
			return true;
		}

		static bool warmup(const std::wstring& path)
		{
			return load(path);
		}

		static bool loaded() { return s_loaded; }

	private:
		static std::vector<std::wstring> splitTabs(const std::string& s)
		{
			std::vector<std::wstring> out;
			size_t start = 0;
			while (true) {
				size_t tab = s.find('\t', start);
				std::string cell = (tab == std::string::npos)
					? s.substr(start)
					: s.substr(start, tab - start);
				out.push_back(decodeUtf8(cell));
				if (tab == std::string::npos) break;
				start = tab + 1;
			}
			return out;
		}

		// decode a UTF-8 byte string that must be exactly one character
		static std::wstring decodeUtf8(const std::string& s)
		{
			if (s.empty()) return L"";
			int n = ::MultiByteToWideChar(CP_UTF8, 0, s.c_str(), static_cast<int>(s.size()), nullptr, 0);
			if (n <= 0) return L"";
			std::wstring w;
			w.resize(n);
			::MultiByteToWideChar(CP_UTF8, 0, s.c_str(), static_cast<int>(s.size()), &w[0], n);
			// strip a leading BOM if the file carried one
			if (w.size() > 1 && w[0] == 0xFEFF) w.erase(w.begin());
			return w;
		}

		static void rebuildIndex()
		{
			int nCols = static_cast<int>(s_languages.size());
			s_index.assign(nCols, {});
			for (size_t row = 0; row < s_rows.size(); ++row) {
				for (int col = 0; col < nCols; ++col) {
					s_index[col][s_rows[row][col]] = static_cast<int>(row);
				}
			}
		}

		// strip anything from the first '-' (subtag) on; lowercase
		static std::wstring baseName(const std::wstring& name)
		{
			std::wstring base = name.substr(0, name.find(L'-'));
			for (auto& c : base) c = static_cast<wchar_t>(::towlower(c));
			return base;
		}

		static bool equalsIgnoreCase(const std::wstring& a, const std::wstring& b)
		{
			if (a.size() != b.size()) return false;
			for (size_t i = 0; i < a.size(); ++i) {
				if (::towlower(a[i]) != ::towlower(b[i])) return false;
			}
			return true;
		}

		// state
		static std::vector<std::wstring> s_languages;   // column headers (names)
		static std::vector<std::vector<wchar_t>> s_rows; // each row: char per column
		static std::vector<std::unordered_map<wchar_t, int>> s_index; // per column: char -> row
		static bool s_loaded;
	};

	// out-of-class definitions
	std::vector<std::wstring> LayoutMap::s_languages;
	std::vector<std::vector<wchar_t>> LayoutMap::s_rows;
	std::vector<std::unordered_map<wchar_t, int>> LayoutMap::s_index;
	bool LayoutMap::s_loaded = false;

}  // namespace kf
