#!/usr/bin/env python3
#
# (C) Copyright IBM Deutschland GmbH 2021, 2025
# (C) Copyright IBM Corp. 2021, 2025
#
# non-exclusively licensed to gematik GmbH
#
# Filter the output of a gtest executable run, omitting output of successful tests.

import argparse
import re
import sys
from typing import TextIO


class NullOutput(object):
    def write(self, data):
        pass

    def flush(self):
        pass

    def close(self):
        pass


def open_unfiltered_output(path: str | None) -> TextIO | NullOutput:
    if path:
        return open(path, "w")
    return NullOutput()


def flush_lines(lines: list[str]):
    for line in lines:
        print(f'{line}', end='')


def filter_output(unfiltered_output: TextIO | NullOutput):
    pre_test = True
    regex = re.compile(r'^\[([A-Z ]{10})]')
    pending_lines: list[str] = []
    sys.stdin.reconfigure(errors='ignore')
    for line in sys.stdin:
        match = regex.match(line)
        if match:
            event = match.group(1).strip()
            if event == 'RUN':
                pre_test = False
            elif event in {'OK', 'SKIPPED'}:
                pending_lines = []
                pre_test = True
            elif event == 'FAILED':
                pre_test = True
                flush_lines(pending_lines)
                pending_lines = []
            print(f'{line}', end='')
        else:
            if pre_test:
                print(f'{line}', end='')
            else:
                pending_lines.append(line)
        unfiltered_output.write(line)

    flush_lines(pending_lines)


def main():
    parser = argparse.ArgumentParser(
        description='Filter output of gtest executable run, omitting output of successful tests')
    parser.add_argument('--unfiltered_output', default='default',
                        help='Write the unfiltered output to this file.')
    args = parser.parse_args()

    with open_unfiltered_output(args.unfiltered_output) as unfiltered_output:
        filter_output(unfiltered_output)


if __name__ == "__main__":
    main()
