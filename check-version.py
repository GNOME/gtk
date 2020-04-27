#!/usr/bin/env python3

import re
import sys

try:
    configure_ac = sys.argv[1]
except Exception:
    configure_ac = 'configure.ac'

try:
    meson_build = sys.argv[2]
except Exception:
    meson_build = 'meson.build'

CONFIGURE_MAJOR_VERSION_RE = re.compile(
    r'''
    ^
    \s*
    m4_define\(
    \s*
    \[gtk_major_version\]
    \s*
    ,
    \s*
    \[
    (?P<version>[0-9]+)
    \]
    \s*
    \)
    $
    ''',
    re.UNICODE | re.VERBOSE
)

CONFIGURE_MINOR_VERSION_RE = re.compile(
    r'''
    ^
    \s*
    m4_define\(
    \s*
    \[gtk_minor_version\]
    \s*
    ,
    \s*
    \[
    (?P<version>[0-9]+)
    \]
    \s*
    \)
    $
    ''',
    re.UNICODE | re.VERBOSE
)

CONFIGURE_MICRO_VERSION_RE = re.compile(
    r'''
    ^
    \s*
    m4_define\(
    \s*
    \[gtk_micro_version\]
    \s*
    ,
    \s*
    \[
    (?P<version>[0-9]+)
    \]
    \s*
    \)
    $
    ''',
    re.UNICODE | re.VERBOSE
)

CONFIGURE_INTERFACE_AGE_RE = re.compile(
    r'''
    ^
    \s*
    m4_define\(
    \s*
    \[gtk_interface_age\]
    \s*
    ,
    \s*
    \[
    (?P<age>[0-9]+)
    \]
    \s*
    \)
    $
    ''',
    re.UNICODE | re.VERBOSE
)

MESON_VERSION_RE = re.compile(
    r'''
    ^
    \s*
    version
    \s*
    :{1}
    \s*
    \'{1}
    (?P<major>[0-9]+)
    \.{1}
    (?P<minor>[0-9]+)
    \.{1}
    (?P<micro>[0-9]+)
    \'{1}
    \s*
    ,?
    $
    ''',
    re.UNICODE | re.VERBOSE
)

MESON_INTERFACE_AGE_RE = re.compile(
    r'''
    ^\s*gtk_interface_age\s*={1}\s*(?P<age>[0-9]+)\s*$
    ''',
    re.UNICODE | re.VERBOSE
)

version = {}

with open(configure_ac, 'r') as f:
    line = f.readline()
    while line:
        res = CONFIGURE_MAJOR_VERSION_RE.match(line)
        if res:
            if 'major' in version:
                print(f'Redefinition of major version; version is already set to {version["major"]}')
                sys.exit(1)
            version['major'] = res.group('version')
            line = f.readline()
            continue
        res = CONFIGURE_MINOR_VERSION_RE.match(line)
        if res:
            if 'minor' in version:
                print(f'Redefinition of minor version; version is already set to {version["minor"]}')
                sys.exit(1)
            version['minor'] = res.group('version')
            line = f.readline()
            continue
        res = CONFIGURE_MICRO_VERSION_RE.match(line)
        if res:
            if 'micro' in version:
                print(f'Redefinition of micro version; version is already set to {version["micro"]}')
                sys.exit(1)
            version['micro'] = res.group('version')
            line = f.readline()
            continue
        res = CONFIGURE_INTERFACE_AGE_RE.match(line)
        if res:
            if 'age' in version:
                print(f'Redefinition of interface age; age is already set to {version["age"]}')
                sys.exit(1)
            version['age'] = res.group('age')
            line = f.readline()
            continue
        if ('major', 'minor', 'micro', 'age') in version:
            break
        line = f.readline()

print(f'GTK version defined in {configure_ac}: {version["major"]}.{version["minor"]}.{version["micro"]} (age: {version["age"]})')

configure_version = version
version = {}

with open(meson_build, 'r') as f:
    line = f.readline()
    inside_project = False
    while line:
        if line.startswith('project('):
            inside_project = True
        if inside_project:
            res = MESON_VERSION_RE.match(line)
            if res:
                version['major'] = res.group('major')
                version['minor'] = res.group('minor')
                version['micro'] = res.group('micro')
        if inside_project and line.endswith(')'):
            inside_project = False
        res = MESON_INTERFACE_AGE_RE.match(line)
        if res:
            version['age'] = res.group('age')
        if ('major', 'minor', 'micro', 'age') in version:
            break
        line = f.readline()

print(f'GTK version defined in {meson_build}: {version["major"]}.{version["minor"]}.{version["micro"]} (age: {version["age"]})')

meson_version = version

if configure_version != meson_version:
    print('Version mismatch between Autotools and Meson builds')
    sys.exit(1)

sys.exit(0)
