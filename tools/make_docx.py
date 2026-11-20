#!/usr/bin/env python3
"""Make Word (.docx) versions of the project's Markdown documents with pandoc.

    python3 tools/make_docx.py                   # all documents
    python3 tools/make_docx.py README plan       # only these (names without .md)
    python3 tools/make_docx.py --force plan      # also overwrite a .docx edited in Word

A .docx that is newer than its .md has probably been edited in Word; it is skipped
(with a warning) unless --force is given, so Word edits are never overwritten.

First regenerates docs/guide-with-images.md from README.md (adds the StickS3 photos).
Writes docs/README.docx, docs/guide-with-images.docx, docs/plan.docx and
docs/activities.docx (whichever sources exist). The .docx files are distributed
separately and are not committed (see .gitignore).

Before conversion the Markdown is adapted for Word:
  - ```latex blocks become display equations (editable Word equations),
  - a ```mermaid flowchart (the rollout plan) becomes a table, since Word cannot draw it,
  - tables get borders via a generated reference document,
  - <sub>/<sup> tags become real Word subscripts and superscripts.
Needs pandoc (brew install pandoc).
"""
import pathlib
import re
import shutil
import subprocess
import sys
import tempfile
import zipfile

ROOT = pathlib.Path(__file__).resolve().parent.parent
DOCS = ROOT / "docs"
SOURCES = [ROOT / "README.md", DOCS / "guide-with-images.md", DOCS / "plan.md", DOCS / "activities.md"]


def latex_blocks(md: str) -> str:
    """```latex ... ``` -> $$ ... $$ so pandoc writes a Word equation."""
    return re.sub(r"```latex\n(.*?)\n```", lambda m: "$$\n" + m.group(1).strip() + "\n$$", md, flags=re.S)


def mermaid_blocks(md: str) -> str:
    """Turn a simple 'flowchart LR' of phases and labelled gates into a Markdown table."""
    def convert(m):
        body = m.group(1)
        nodes = {k: v.replace("<br/>", "; ") for k, v in re.findall(r'(\w+)\["(.*?)"\]', body)}
        gates = {}
        for line in body.splitlines():
            for a, label, b in re.findall(r'(\w+)\s*--\s*"(.*?)"\s*-->\s*(\w+)', line):
                gates[a] = (label, b)
            # chained edges: A -- "x" --> B -- "y" --> C
            parts = re.split(r'\s*--\s*"(.*?)"\s*-->\s*', line.strip())
            for i in range(0, len(parts) - 2, 2):
                gates[parts[i]] = (parts[i + 1], parts[i + 2])
        if not nodes:
            return "*(Diagram not available in this version; see the Markdown file.)*"
        rows = ["| Phase | Contents | Gate to next phase |", "| --- | --- | --- |"]
        for key, text in nodes.items():
            name, _, rest = text.partition("; ")
            rows.append(f"| {name} | {rest} | {gates.get(key, ('–',))[0]} |")
        return "\n".join(rows)
    return re.sub(r"```mermaid\n(.*?)\n```", convert, md, flags=re.S)


BORDERS = ('<w:tblBorders>' + ''.join(
    f'<w:{side} w:val="single" w:sz="4" w:space="0" w:color="A6A6A6"/>'
    for side in ("top", "left", "bottom", "right", "insideH", "insideV")) + '</w:tblBorders>')


def reference_docx(path: pathlib.Path) -> None:
    """pandoc's default reference document with bordered, padded tables."""
    data = subprocess.run(["pandoc", "--print-default-data-file", "reference.docx"],
                          check=True, capture_output=True).stdout
    path.write_bytes(data)
    tmp = path.with_suffix(".tmp")
    with zipfile.ZipFile(path) as zin, zipfile.ZipFile(tmp, "w", zipfile.ZIP_DEFLATED) as zout:
        for item in zin.infolist():
            content = zin.read(item.filename)
            if item.filename == "word/styles.xml":
                xml = content.decode("utf-8")
                # Add borders and cell padding to pandoc's "Table" style
                xml = re.sub(r'(<w:style [^>]*w:styleId="Table"[^>]*>.*?<w:tblPr>)',
                             r'\1' + BORDERS + '<w:tblCellMar><w:left w:w="80" w:type="dxa"/>'
                             '<w:right w:w="80" w:type="dxa"/></w:tblCellMar>', xml, count=1, flags=re.S)
                content = xml.encode("utf-8")
            zout.writestr(item, content)
    tmp.replace(path)


def sub_sup(md: str) -> str:
    """HTML <sub>/<sup> (shown by GitHub) -> pandoc ~sub~ / ^sup^, which become real Word
    subscripts; pandoc drops raw HTML tags when writing .docx."""
    esc = lambda t: re.sub(r"([ ~^])", r"\\\1", t)
    md = re.sub(r"<sub>(.*?)</sub>", lambda m: "~" + esc(m.group(1)) + "~", md)
    return re.sub(r"<sup>(.*?)</sup>", lambda m: "^" + esc(m.group(1)) + "^", md)


LIST_ITEM = re.compile(r"^\s*([-*+]|\d+\.)\s")


def list_spacing(md: str) -> str:
    """Pandoc Markdown needs a blank line before a list that follows a paragraph line
    (GitHub does not); add it, outside code blocks."""
    out, in_code, prev = [], False, ""
    for line in md.splitlines():
        if line.lstrip().startswith(("```", "$$")):
            in_code = not in_code if line.lstrip().startswith("```") else in_code
        if (not in_code and LIST_ITEM.match(line) and prev.strip()
                and not LIST_ITEM.match(prev) and not prev.startswith((" ", "\t", "|", "#"))):
            out.append("")
        out.append(line)
        prev = line
    return "\n".join(out) + "\n"


IMAGE_WIDTH = {"sticks3-front.png": "5cm", "sticks3-overview.png": "13cm"}  # default 12cm


def size_images(md: str) -> str:
    """Give images a sensible printed width (pandoc otherwise uses the full page width)."""
    def sized(m):
        name = pathlib.Path(m.group(2)).name
        return f"![{m.group(1)}]({m.group(2)}){{width={IMAGE_WIDTH.get(name, '12cm')}}}"
    return re.sub(r"!\[(.*?)\]\((.*?)\)(?!\{)", sized, md)


# The illustrated guide is the README with the StickS3 photos in place of the link to
# M5Stack's product page. It is regenerated here so it never falls behind the README.
README_PHOTO_LINK = "Button and port locations: see M5Stack's [StickS3 product page](https://docs.m5stack.com/en/core/StickS3).\n"
GUIDE_PHOTOS = ("![StickS3 front: the blue KEY1 button below the screen](images/sticks3-front.png)\n\n"
                "![StickS3 button and port locations](images/sticks3-overview.png)\n")


def make_guide() -> None:
    if not (DOCS / "images").is_dir():
        return                                  # photos not present (e.g. a fresh clone)
    readme = (ROOT / "README.md").read_text(encoding="utf-8")
    if README_PHOTO_LINK not in readme:
        sys.exit("README photo link line not found; update README_PHOTO_LINK in make_docx.py")
    (DOCS / "guide-with-images.md").write_text(readme.replace(README_PHOTO_LINK, GUIDE_PHOTOS), encoding="utf-8")
    print("wrote docs/guide-with-images.md")


def main() -> int:
    if not shutil.which("pandoc"):
        sys.exit("pandoc not found: brew install pandoc")
    args = sys.argv[1:]
    force = "--force" in args
    wanted = {a for a in args if not a.startswith("--")}
    if not wanted or "guide-with-images" in wanted:
        make_guide()
    with tempfile.TemporaryDirectory() as tmpdir:
        tmpdir = pathlib.Path(tmpdir)
        ref = tmpdir / "reference.docx"
        reference_docx(ref)
        for src in SOURCES:
            if not src.exists() or (wanted and src.stem not in wanted):
                continue
            out = DOCS / (src.stem + ".docx")
            if out.exists() and out.stat().st_mtime > src.stat().st_mtime + 60 and not force:
                print(f"skipped {out.relative_to(ROOT)}: edited after {src.name} (use --force to overwrite)")
                continue
            md = sub_sup(list_spacing(size_images(mermaid_blocks(latex_blocks(src.read_text(encoding="utf-8"))))))
            staged = tmpdir / src.name
            staged.write_text(md, encoding="utf-8")
            subprocess.run(["pandoc", str(staged), "-f", "markdown-smart+pipe_tables", "-o", str(out),
                            "--reference-doc", str(ref), "--resource-path", f"{src.parent}:{DOCS}"],
                           check=True)
            print("wrote", out.relative_to(ROOT))
    return 0


if __name__ == "__main__":
    sys.exit(main())
