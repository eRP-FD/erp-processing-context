#!/usr/bin/env python3

import json
import argparse
import sys

import jinja2



def main():
    parser = argparse.ArgumentParser(
        description="Converts json erp-pc config to ")
    parser.add_argument("input", help="JSON erp-pc configuration json file")
    parser.add_argument("--template", help="Template file", default="configuration.md.j2")

    args = parser.parse_args()

    config = {}

    with open(args.input, "r") as f:
        try:
            config = json.load(f)
        except Exception as e:
            print(f"Unable to parse json file={args.input!r}, err={e}", file=sys.stderr)

    environment = jinja2.Environment()
    with open(args.template, "r") as f:
        template = environment.from_string(f.read())
        print(template.render(config=config))



if __name__ == '__main__':
    main()