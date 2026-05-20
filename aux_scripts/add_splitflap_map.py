"""
Takes a Cheetah display config file in JSON format
and adds the specified map based on a list of entries.
"""

import argparse
import json


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("-f", "--file", type=str, required=True, help="JSON config file to open. It will be overwritten in place!")
    parser.add_argument("-m", "--map", type=str, required=True, help="Name of the map to add")
    parser.add_argument("-e", "--entries", type=str, required=True, help="Text file with entries to add, one entry per line, starting at 0.")
    args = parser.parse_args()

    with open(args.file, 'r') as f:
        data = json.load(f)

    with open(args.entries, 'r') as f:
        entries = f.readlines()

    mapping = {}
    for pos, text in enumerate(entries):
        mapping[pos] = text.strip()

    data['maps'][args.map] = mapping

    with open(args.file, 'w') as f:
        json.dump(data, f, sort_keys=True, indent=4)


if __name__ == "__main__":
    main()
