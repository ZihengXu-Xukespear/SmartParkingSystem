#!/usr/bin/env python3
"""
Multi-method C++ ratio estimator for the SmartParking project.

Methods:
  A) Raw line count by file extension (any line, including blanks/comments)
  B) SLOC-style: lines containing non-whitespace characters (approximation of
     cloc output)
  C) GitHub Linguist-style classification: skip vendored / docs / generated,
     count bytes-of-source per file
  D) Linguist-style weighted share: weight each language by the *non-blank*
     lines × typical file-size factor

We DO NOT have the real linguist installed; this script intends to be an
*independent* sanity check.
"""

from __future__ import annotations
import os, re, sys, json
from collections import defaultdict
from pathlib import Path

# ---------------------------------------------------------------------------
# Configuration
# ---------------------------------------------------------------------------
ROOT = Path(sys.argv[1] if len(sys.argv) > 1 else
            r"C:\Users\Ziheng Xu\Desktop\final\SmartParkingSystem")

# Vendored / auto-generated directories that Linguist ignores by default
VENDORED_DIRS = {"third_party", "third-party", "node_modules", "vendor",
                 "build", "cmake-build-debug", "cmake-build-release", ".git"}
GENERATED_FILES = {".vcxproj", ".vcxproj.filters", ".sln"}
BINARY_EXTS = {".png", ".jpg", ".jpeg", ".gif", ".ico", ".pdf",
               ".zip", ".tar", ".gz", ".so", ".dll", ".exe", ".lib",
               ".obj", ".exp", ".ilk", ".pdb", ".o", ".a"}

# Extension -> (language, category)
EXT_MAP = {
    ".cpp": ("C++", "code"), ".cc": ("C++", "code"), ".cxx": ("C++", "code"),
    ".c":   ("C",   "code"),
    ".h":   ("C++", "code"), ".hpp": ("C++", "code"), ".hh": ("C++", "code"),
    ".js":  ("JavaScript", "code"), ".mjs": ("JavaScript", "code"),
    ".ts":  ("TypeScript", "code"),
    ".html":("HTML", "code"), ".htm": ("HTML", "code"),
    ".css": ("CSS", "code"),
    ".py":  ("Python", "code"),
    ".json":("JSON",  "data"),
    ".sql": ("SQL",   "code"),
    ".md":  ("Markdown", "docs"),
    ".txt": ("Text", "docs"),
    ".cmake":("CMake", "code"),
    ".bat": ("Batch", "code"), ".cmd": ("Batch", "code"),
    ".sh":  ("Shell", "code"),
    ".yml": ("YAML", "data"),  ".yaml": ("YAML", "data"),
    ".ini": ("INI",  "data"),
    ".xml": ("XML",  "data"),
}

def classify(path: Path) -> tuple[str, str] | None:
    """Return (language, category) for a file, or None to skip."""
    name = path.name
    if name.startswith(".") and name not in {".gitignore"}:
        if name in {".gitignore", ".gitattributes"}:
            return ("Dotfiles", "docs")
        return None
    # Filename-based classification
    if name == "CMakeLists.txt":
        return ("CMake", "code")
    if name in {"package.json", "tsconfig.json"}:
        return ("JSON", "data")
    ext = path.suffix.lower()
    if ext in GENERATED_FILES:
        return (None, "generated")
    if ext in BINARY_EXTS:
        return (None, "binary")
    if not ext:
        return (None, "unknown")
    return EXT_MAP.get(ext, ("Other-" + ext.lstrip("."), "data"))

# ---------------------------------------------------------------------------
# Walk
# ---------------------------------------------------------------------------
def walk(root: Path):
    lang_raw = defaultdict(int)        # raw lines per language
    lang_sloc = defaultdict(int)       # non-blank lines per language
    lang_bytes = defaultdict(int)      # file bytes per language
    lang_cat = {}                      # language -> category
    file_count = defaultdict(int)
    skip_dirs = 0
    skip_files = 0
    cpp_raw = cpp_sloc = cpp_bytes = 0

    for base, dirs, files in os.walk(root):
        # Remove vendored/build dirs in-place
        before = len(dirs)
        dirs[:] = [d for d in dirs if d not in VENDORED_DIRS]
        skip_dirs += before - len(dirs)

        for f in files:
            p = Path(base) / f
            rel = p.relative_to(root)
            # Skip third_party even if it's a file (defensive)
            if any(part in VENDORED_DIRS for part in rel.parts):
                skip_files += 1
                continue
            klass = classify(p)
            if klass is None:
                skip_files += 1; continue
            lang, cat = klass
            try:
                text = p.read_text(encoding="utf-8", errors="replace")
            except Exception:
                skip_files += 1; continue
            raw = text.count("\n") + (0 if text.endswith("\n") else 1)
            sloc = sum(1 for ln in text.splitlines() if ln.strip())
            lang_raw[lang]   += raw
            lang_sloc[lang]  += sloc
            lang_bytes[lang] += len(text.encode("utf-8", errors="replace"))
            lang_cat[lang] = cat
            file_count[lang] += 1
            if lang == "C++":
                cpp_raw += raw
                cpp_sloc += sloc
                cpp_bytes += len(text.encode("utf-8", errors="replace"))

    return (lang_raw, lang_sloc, lang_bytes, lang_cat, file_count,
            skip_dirs, skip_files)

# ---------------------------------------------------------------------------
# Report
# ---------------------------------------------------------------------------
def ratio(total: dict, key: str = "C++") -> float:
    s = sum(total.values())
    if not s: return 0.0
    return 100.0 * total[key] / s

def fmt_table(rows, headers):
    widths = [max(len(str(row[i])) for row in [headers] + rows) for i in range(len(headers))]
    line = lambda r: " | ".join(str(c).ljust(w) for c, w in zip(r, widths))
    sep = "-+-".join("-" * w for w in widths)
    out = [line(headers), sep] + [line(r) for r in rows]
    return "\n".join(out)

def main():
    print(f"Project root: {ROOT}")
    print(f"Vendored / ignored dirs: {sorted(VENDORED_DIRS)}\n")

    lang_raw, lang_sloc, lang_bytes, lang_cat, file_count, skip_dirs, skip_files = walk(ROOT)

    total_raw = sum(lang_raw.values())
    total_sloc = sum(lang_sloc.values())
    total_bytes = sum(lang_bytes.values())

    # Restrict to "code" languages for Linguist-style comparison
    code_langs = {l for l, c in lang_cat.items() if c == "code"}

    # Method A — raw lines
    a_total = {l: lang_raw[l] for l in code_langs}
    # Method B — SLOC (non-blank lines)
    b_total = {l: lang_sloc[l] for l in code_langs}
    # Method C — bytes (Linguist's primary metric)
    c_total = {l: lang_bytes[l] for l in code_langs}
    # Method D — bytes *within code only*, weighted by file count (a proxy
    # for tokenization complexity, similar to Linguist)
    d_total = {l: lang_bytes[l] * (1 + 0.1 * (file_count[l] - 1)) for l in code_langs}

    print(f"Skipped: {skip_dirs} dirs, {skip_files} files (vendored/generated/binary)\n")

    headers = ["Language", "Files", "RawLines", "SLOC", "Bytes"]
    rows = []
    for lang in sorted(lang_raw, key=lambda x: -lang_raw[x]):
        if lang in code_langs:
            rows.append([lang, file_count[lang], lang_raw[lang],
                         lang_sloc[lang], lang_bytes[lang]])
    # Show data/docs categories too
    for lang in sorted(lang_raw, key=lambda x: -lang_raw[x]):
        if lang not in code_langs:
            rows.append([f"{lang} ({lang_cat[lang]})", file_count[lang],
                         lang_raw[lang], lang_sloc[lang], lang_bytes[lang]])
    print("=== Per-language totals ===")
    print(fmt_table(rows, headers))
    print()

    def report(name, totals):
        total = sum(totals.values())
        pct = ratio(totals)
        # Sorted descending
        sd = sorted(totals.items(), key=lambda kv: -kv[1])
        print(f"--- {name} ---")
        print(f"  Total measured : {total:,}")
        print(f"  C++ share      : {pct:.2f}%   (C++ raw = {totals.get('C++', 0):,})")
        for lang, v in sd[:8]:
            mark = " <-- C++" if lang == "C++" else ""
            print(f"    {lang:<12} {v:>10,}  ({100*v/total:>5.2f}%){mark}")
        print()

    print("====== C++ share by counting method ======\n")
    report("Method A — raw lines (any line, incl. blanks/comments)", a_total)
    report("Method B — SLOC (non-blank lines only)", b_total)
    report("Method C — bytes (GitHub Linguist primary metric)", c_total)
    report("Method D — bytes weighted by file count", d_total)

    # Cross-check: include all langs (not just "code")
    print("--- Method C' — bytes across ALL languages (incl. JSON/SQL/MD) ---")
    total = sum(lang_bytes.values())
    sd = sorted(lang_bytes.items(), key=lambda kv: -kv[1])
    for lang, v in sd[:10]:
        if lang is None: continue
        mark = " <-- C++" if lang == "C++" else ""
        print(f"    {lang:<14} {v:>10,}  ({100*v/total:>5.2f}%){mark}")
    print()
    print(f"  C++ share (bytes, all langs): {100.0*lang_bytes.get('C++', 0)/total:.2f}%\n")

    # Cross-check: include only LOC (no data/docs)
    print("--- Method B' — SLOC across CODE languages only ---")
    total = sum(b_total.values())
    print(f"  C++ share: {100.0*lang_sloc.get('C++', 0)/total:.2f}%  "
          f"(out of {total:,} non-blank lines)")
    print()

if __name__ == "__main__":
    main()
