"""
Takes a Cheetah display config file in JSON format
and rotates / shifts the specified map by the specified amount.
This is useful for when the unit home position was not known
when the texts were written down.
"""

import argparse
import json


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("-f", "--file", type=str, required=True, help="JSON config file to open. It will be overwritten in place!")
    parser.add_argument("-m", "--map", type=str, required=True, help="Name of the map to rotate")
    parser.add_argument("-r", "--rotate", type=int, required=True, help="Amount to rotate by. E.g. +1 moves entry #4 to the #5 position and vice versa.")
    parser.add_argument("-n", "--num-flaps", type=int, required=True, help="Number of flaps on the specified module, e.g. for num_flaps=64 and rotate=1, entry #63 becomes entry #0.")
    args = parser.parse_args()

    with open(args.file, 'r') as f:
        data = json.load(f)

    mapping = data['maps'][args.map]

    new_items = []
    for key, value in mapping.items():
        new_key = (int(key) + args.rotate) % args.wrap
        new_items.append((new_key, value))

    data['maps'][args.map] = dict(new_items)

    with open(args.file, 'w') as f:
        json.dump(data, f, sort_keys=True, indent=4)


if __name__ == "__main__":
    main()
