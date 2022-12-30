import argparse
import datetime
import time
import traceback

from cheetah_api import Display


def main():
    parser = argparse.ArgumentParser(add_help=False)
    parser.add_argument("-h", "--host", type=str, required=True, help="Host")
    parser.add_argument("-d", "--date", type=str, required=True, help="Target to count down to (Y-m-d H:M:S)")
    parser.add_argument("-t", "--text", type=str, required=True, help="Text to display when target is reached")
    parser.add_argument("-f", "--format", type=str, required=False, help="Countdown format string. Available format specifiers: d, H, M, S")
    args = parser.parse_args()

    disp = Display(f"http://{args.host}")

    if not args.format:
        args.format = "{d}d {H:02d}:{M:02d}:{S:02d}"
    
    target = datetime.datetime.strptime(args.date, "%Y-%m-%d %H:%M:%S")
    prev_now = datetime.datetime.now()

    while True:
        now = datetime.datetime.now()
        if now.second == prev_now.second:
            time.sleep(0.1)
            continue
        if now >= target:
            print(args.text)
            try:
                disp.set_text(args.text)
            except:
                traceback.print_exc()
            return
        seconds = round((target - now).total_seconds())

        days, remainder = divmod(seconds, 86400)
        hours, remainder = divmod(remainder, 3600)
        minutes, seconds = divmod(remainder, 60)

        text = args.format.format(d=days, H=hours, M=minutes, S=seconds)
        print(text)
        try:
            disp.set_text(text)
        except:
            traceback.print_exc()
        prev_now = now



if __name__ == "__main__":
    main()
