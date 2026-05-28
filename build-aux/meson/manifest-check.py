#!/usr/bin/env python3
#
# SPDX-FileCopyrightText: 2026 Emmanuele Bassi
# SPDX-License-Identifier: LGPL-2.1-or-later

import argparse
import os
import sys
import xml.etree.ElementTree as ET

from dataclasses import dataclass
from enum import Enum
from pathlib import Path


class TermColor(Enum):
    NONE = 0
    EXT = 1
    TRUE = 2


def setup_output() -> TermColor:
    try:
        if not os.isatty(sys.stdout.fileno()):
            return TermColor.NONE
        if os.environ.get('TERM', '') == 'dumb':
            return TermColor.NONE
        if os.environ.get('COLORTERM', '') == 'truecolor' or \
           os.environ.get('TERM', '') == 'xterm-256color':
            return TermColor.TRUE
        return TermColor.EXT
    except Exception:
        return TermColor.NONE


LOG_COLORIZE_OUTPUT: TermColor = setup_output()


def error(message: str) -> None:
    global LOG_COLORIZE_OUTPUT
    msg = []
    if LOG_COLORIZE_OUTPUT == TermColor.NONE:
        msg.append("ERROR: ")
    elif LOG_COLORIZE_OUTPUT == TermColor.EXT:
        msg.append("\033[1;38;5;160ERROR:\033[0;39;49m ")
    elif LOG_COLORIZE_OUTPUT == TermColor.TRUE:
        msg.append("\033[1;38;2;255;0;0mERROR:\033[0;39;49m ")
    msg.append(message)
    print("".join(msg), file=sys.stderr)
    sys.exit(1)


@dataclass
class Section:
    prefix: str
    files: list[str]


@dataclass
class Manifest:
    file: Path
    sections: list[Section]

    def __str__(self):
        res = [f"Source: {self.file}"]
        for sec in self.sections:
            res.append(f"  Section: {sec.prefix}")
            for f in sec.files:
                res.append(f"    File: {f}")
        return "\n".join(res)


def parse_section(node: ET.Element) -> Section:
    assert node.tag == "gresource"
    prefix = node.attrib.get("prefix", "/")
    files = []
    for child in node:
        if child.tag == "file":
            if child.text is None:
                error("Invalid file component")
            file = child.text
            files.append(file)
    return Section(prefix, files)


def load_manifest(manifest: Path) -> Manifest:
    tree = ET.parse(manifest)
    root = tree.getroot()
    if root.tag != "gresources":
        raise RuntimeError("Invalid manifest file")
    sections = []
    for node in root:
        if node.tag == "gresource":
            section = parse_section(node)
            sections.append(section)
    return Manifest(manifest, sections)


def check_file(manifest: Manifest, file: Path):
    manifest_basedir = manifest.file.parent.name
    # normalize the absolute path to the location of the manifest
    idx = file.parts.index(manifest_basedir)
    check = manifest.file.parent.joinpath(*file.parts[idx + 1:])
    if not check.exists():
        error(f"File {file} not found in {basedir}")
    check_file = "/".join(file.parts[idx + 1:])
    found = False
    for sect in manifest.sections:
        for f in sect.files:
            if f == check_file:
                found = True
                break
        if found:
            break
    if not found:
        error(f"File {file} not found in the manifest")


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("manifest", help="The GResource manifest to check")
    parser.add_argument("files", nargs="+", help="The list of files to check")

    args = parser.parse_args()

    try:
        manifest = load_manifest(Path(args.manifest).resolve())
    except Exception as err:
        error(f"Unable to parse {args.manifest}: {err}")

    for file in args.files:
        check_file(manifest, Path(file).resolve())

if __name__ == "__main__":
    main()
