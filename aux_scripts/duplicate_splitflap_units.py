"""
Takes a Cheetah display config file in JSON format
and duplicates all units, giving the copies a specified X and Y offset.
This is useful for creating the rear side of double-sided displays.
"""

import argparse
import json


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("-f", "--file", type=str, required=True, help="JSON config file to open. It will be overwritten in place!")
    parser.add_argument("-a", "--addr-offset", type=int, required=True, help="Address offset for the copied units")
    parser.add_argument("-x", "--x-offset", type=int, required=True, help="X offset for the copied units")
    parser.add_argument("-y", "--y-offset", type=int, required=True, help="Y offset for the copied units")
    args = parser.parse_args()

    with open(args.file, 'r') as f:
        data = json.load(f)

    units = data['units'].copy()

    for unit in units:
        new_unit = unit.copy()
        new_unit['addr'] += args.addr_offset
        new_unit['x'] += args.x_offset
        new_unit['y'] += args.y_offset
        data['units'].append(new_unit)

    with open(args.file, 'w') as f:
        json.dump(data, f, sort_keys=True, indent=4)


if __name__ == "__main__":
    main()
