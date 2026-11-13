#!/usr/bin/env python3
"""Hold an AMD performance policy for one container's lifetime."""
import argparse
import json
from pathlib import Path
import signal
import time


def process_start(pid):
    try:
        fields = Path(f'/proc/{pid}/stat').read_text().rsplit(')', 1)[1].split()
        return fields[19] if fields[0] not in ('Z', 'X') else None
    except (OSError, IndexError):
        return None


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--pid', type=int, required=True)
    parser.add_argument('--start-time', required=True)
    parser.add_argument('--mode', choices=['high', 'profile_peak'], default='high')
    parser.add_argument('--status', required=True)
    args = parser.parse_args()
    policy = Path('/gpu-performance-level')
    status = Path(args.status)
    original = policy.read_text().strip()
    state = {'original': original, 'requested': args.mode, 'pid': args.pid,
             'start_time': args.start_time, 'restored': False}

    def interrupted(signum, frame):
        raise SystemExit(128 + signum)

    for sig in (signal.SIGINT, signal.SIGTERM, signal.SIGHUP):
        signal.signal(sig, interrupted)
    if process_start(args.pid) != args.start_time:
        return 0
    try:
        policy.write_text(args.mode + '\n')
        state['active'] = policy.read_text().strip()
        status.write_text(json.dumps(state, indent=2))
        while process_start(args.pid) == args.start_time:
            time.sleep(.5)
    finally:
        # Wait for the application GPU context to close before restoring the policy.
        for attempt in range(3):
            time.sleep(.5)
            if args.mode == 'profile_peak':
                try:
                    policy.write_text('profile_exit\n')
                except OSError:
                    pass
            policy.write_text(original + '\n')
        state['restored'] = policy.read_text().strip() == original
        state['end'] = time.time()
        status.write_text(json.dumps(state, indent=2))
    return 0 if state['restored'] else 1


if __name__ == '__main__':
    raise SystemExit(main())
