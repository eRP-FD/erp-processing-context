#!/usr/bin/env python3
# (C) Copyright IBM Deutschland GmbH 2021, 2026
# (C) Copyright IBM Corp. 2021, 2026
#
# non-exclusively licensed to gematik GmbH
#
# Regenerates the version column in Dependencies.md from conanfile.py.
# Preserves all other columns (license, download link) and any footnotes.
# Run from the repository root:
#   python3 scripts/update-dependencies-md.py

import re
import sys

CONANFILE = "conanfile.py"
DEPS_MD = "Dependencies.md"

# Map conan package name -> Dependencies.md package name (where they differ)
CONAN_TO_MD_NAME = {
    "antlr4-cppruntime": "antlr",
}


def parse_conan_versions(path: str) -> dict[str, str]:
    """Extract {name: version} from the requires list in conanfile.py.

    Only reads the top-level `requires = [...]` block; ignores tool_requires
    and build_requirements so we don't pick up build-only versions.
    """
    with open(path) as f:
        content = f.read()

    # Extract only the static requires = [...] block
    m = re.search(r"requires\s*=\s*\[([^\]]+)\]", content, re.DOTALL)
    if not m:
        return {}

    block = m.group(1)
    versions: dict[str, str] = {}
    for entry in re.finditer(r"'([a-zA-Z0-9_\-]+)/([^']+)'", block):
        name, version = entry.group(1), entry.group(2)
        # Strip local suffixes like +erp so the displayed version stays clean
        version = version.split("+")[0]
        versions[name] = version

    return versions


def update_deps_md(md_path: str, conan_versions: dict[str, str]) -> bool:
    """
    Update the version column in Dependencies.md in-place.
    Returns True if any line was changed.
    """
    with open(md_path) as f:
        lines = f.readlines()

    changed = False
    new_lines: list[str] = []

    for line in lines:
        # Only process table data rows (start with | but not the header/separator)
        if line.startswith("|") and not re.match(r"^\|[-| ]+\|", line):
            cols = line.split("|")
            # cols[0]="", cols[1]=package, cols[2]=version, cols[3]=license, cols[4]=link, cols[5]=""
            if len(cols) >= 5:
                raw_name_col = cols[1]
                md_name = raw_name_col.strip().rstrip("*").strip()

                # Resolve conan name
                conan_name = None
                for cname, mname in CONAN_TO_MD_NAME.items():
                    if mname == md_name:
                        conan_name = cname
                        break
                if conan_name is None and md_name in conan_versions:
                    conan_name = md_name

                if conan_name and conan_name in conan_versions:
                    new_version = conan_versions[conan_name]
                    # Preserve the original column width (leading/trailing spaces)
                    old_version_col = cols[2]
                    # Keep same padding: same total length as original column
                    padded = f" {new_version} ".ljust(len(old_version_col))
                    if padded.rstrip() != old_version_col.rstrip():
                        cols[2] = padded
                        line = "|".join(cols)
                        changed = True

        new_lines.append(line)

    if changed:
        with open(md_path, "w") as f:
            f.writelines(new_lines)
        print(f"Updated {md_path}")
    else:
        print(f"No changes needed in {md_path}")

    return changed


def main() -> None:
    conan_versions = parse_conan_versions(CONANFILE)
    if not conan_versions:
        print(f"ERROR: No dependencies found in {CONANFILE}", file=sys.stderr)
        sys.exit(1)

    update_deps_md(DEPS_MD, conan_versions)


if __name__ == "__main__":
    main()
