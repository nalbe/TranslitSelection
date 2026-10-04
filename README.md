# TranslitSelection — dictionary rules

TranslitSelection fixes text that was typed in the wrong keyboard layout: it takes the selected text and transliterates it from one layout into another. The mapping table is a single text file that you can edit and extend to any number of languages.

## Dictionary file

`layout.txt` lives next to the executable and is copied there automatically at build time.

### Format

```
En<TAB>Ru<TAB>Fr
(empty line)
.<TAB>ю<TAB>star
a<TAB>ф<TAB>alpha
q<TAB>й<TAB>ku
```

- **First line** — the languages = columns, tab-separated. One column per language. At least 2 languages.
- **Empty line** — separator between the header and the data.
- **Then** — one "key" per line: tab-separated characters that key maps to in each language, in the same order as the first line.
- Characters are **UTF-8** (a leading BOM is fine — it is stripped).
- Lines starting with `#` are comments and are ignored. Any other character — including punctuation like `;`, `.`, `'` — is a valid dictionary key.

### How the direction is chosen

The selected text was typed in the **active** layout of the window (`from`). It is transliterated into the **next layout in the switching cycle** (`to`). The language name in the first line can be written in any form (`En`, `en`, `en-US`) — matching is by language code, case- and region-insensitive.

## Limits and rules

### Rows (keys)
- **No limit on the number of rows.** 66, 3, or even 10000 all work.
- Any row whose column count does not match is silently skipped.
- A character missing from the dictionary passes through unchanged.

### Columns (languages)
- At least **2** languages.
- No maximum: as many columns as the first line declares.
- **Important:** every data row must contain exactly as many columns as there are languages in the first line, otherwise it is ignored.

### Other rules
- Changes take effect **after restarting** the program (the dictionary is read at startup).
- If the file is missing, broken, or empty — transliteration is disabled and an error is written to the log (when running with `--log`).
- The last successfully loaded table is kept: a failed reload never breaks an already working dictionary.
- Characters in one row establish a mutual correspondence across all languages at once. Transliteration between any two languages is a single step (O(1)).

## Usage

Select text in any application and press **F8** — the text is transliterated in place using the active keyboard layout.

### Tray menu (right-click)

- **Switch layout after translit** — toggle the system keyboard layout to the next one after transliteration.
- **Keep selection after translit** — re-select the pasted text so you can see what changed.

### Command-line options

| Flag | Description |
|------|-------------|
| `--log` | Enable logging to `translit.log` next to the executable |

Without `--log`, no log file is created and the app runs silently. With it, the log records startup health (which hotkey was registered, whether `layout.txt` loaded) and every transliteration: target window, text read from the clipboard, layout direction, and the resulting text.

## Examples

```text
En<TAB>Ru            (minimum — two languages)
(empty line)
q<TAB>й
w<TAB>ц
e<TAB>у
r<TAB>к
t<TAB>е
```

```text
En<TAB>Ru<TAB>Fr     (three languages)
(empty line)
q<TAB>й<TAB>ku          still exactly three columns per row
a<TAB>ф<TAB>clé
```
