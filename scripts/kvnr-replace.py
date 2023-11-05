#!/usr/bin/env python3

import json
import argparse
import sys
import fileinput

def kvnr_checksum(kvnr: str) -> int:
    card_no = f"{ord(kvnr[0]) - ord('@'):02d}{kvnr[1:9]}"
    total_sum = 0
    for i in range(10):
        d = int(card_no[i]) - int('0')
        if i % 2 == 1:
            d *= 2
        if d > 9:
            d -= 9
        total_sum += d
    return total_sum % 10

def process_rg_data(json):
    if json["type"] != "match":
        return
    data = json["data"]
    submatch = data["submatches"][0]
    kvnr = submatch["match"]["text"]
    new_kvnr = kvnr[0:-1] + str(kvnr_checksum(kvnr))
    if kvnr == new_kvnr:
        return
    file = data["path"]["text"]
    line_number = data["line_number"]
    print(f"kvnr = {kvnr}, correct kvnr = {new_kvnr}, file = {file}:{line_number}")
    with open(file, 'rb') as f:
        inputdata = bytearray(f.read())
    start = data["absolute_offset"] + submatch["start"]
    end = data["absolute_offset"] + submatch["end"]
    #print(inputdata[start:end])
    if inputdata[start:end].decode() != kvnr:
        print(f"[!] data changed, expected '{kvnr}', found '{inputdata[start:end]}'")
        return
    inputdata[start:end] = new_kvnr.encode()
    with open(file, 'wb') as f:
        f.write(inputdata)


def main():
    parser = argparse.ArgumentParser(
        description="Converts Journal log from JSON format into human readable")
    parser.add_argument("input", help="JSON ripgrep output, '-' for stdin")

    args = parser.parse_args()

    with fileinput.input(files=args.input) as f:
        try:
            for line in f:
                process_rg_data(json.loads(line))
        except Exception as e:
            print(f"Could not process file={args.input!r}, err={e}", file=sys.stderr)



if __name__ == '__main__':
    main()