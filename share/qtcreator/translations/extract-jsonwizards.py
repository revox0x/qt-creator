#!/usr/bin/python
# Copyright (C) 2019 The Qt Company Ltd.
# SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only WITH Qt-GPL-exception-1.0

import os
import sys
import json

if len(sys.argv) != 3:
    print("Please provide a top level directory to scan and a file to write into.")
    sys.exit(1)

top_dir = sys.argv[1]
target_file = sys.argv[2]


def recursive_iter(obj, key=''):
    if isinstance(obj, dict):
        for k in sorted(obj.keys()):
            yield from recursive_iter(obj[k], k)
    elif isinstance(obj, tuple):
        for item in obj:
            yield from recursive_iter(item, '')
    elif isinstance(obj, list):
        for item in obj:
            yield from recursive_iter(item, '')
    else:
        yield key, obj


def fix_value(value):
    value = value.replace('\"', '\\"')
    value = value.replace('\n', '\\n')
    return value


def parse_file(file_path):
    root = ''
    result = '#include <QtGlobal>\n\n'

    index = 0
    with open(file_path) as f:
        root = json.load(f)

    for key, value in recursive_iter(root):
        if key.startswith('tr'):
            result += 'const char *a{} = QT_TRANSLATE_NOOP("QtC::ProjectExplorer", "{}"); // {}\n'.format(index, fix_value(value), file_path)

            index += 1
    return result

result = '// This file is autogenerated\n'

# traverse root directory, and list directories as dirs and files as files
for root, _, files in os.walk(top_dir):
    for file in files:
        if file == "wizard.json":
            result += parse_file(os.path.join(root, file))

with open(target_file, 'w') as header_file:
    header_file.write(result)
