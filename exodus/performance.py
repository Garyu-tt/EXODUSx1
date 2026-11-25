"""Compare frame-time traces from matched game benchmark runs (milliseconds)."""
import argparse
import json
import math
import statistics

MAX_FRAMES = 1_000_000
MAX_FRAME_MS = 60_000


def read_trace(path):
    values = []
    with open(path, encoding='utf-8') as stream:
        for line in stream:
            if len(values) >= MAX_FRAMES:
                raise ValueError('too many frames')
            values.append(float(line.strip()))
    return values


def metrics(values):
    if not 100 <= len(values) <= MAX_FRAMES:
        raise ValueError('expected 100 to 1000000 frames')
    if any(not math.isfinite(x) or not 0.001 <= x <= MAX_FRAME_MS for x in values):
        raise ValueError('invalid frame time')
    ordered = sorted(values)
    tail = ordered[-math.ceil(len(values) / 100):]
    return {'frames': len(values),
            'mean_fps': 1000 / statistics.fmean(values),
            'one_percent_low_fps': 1000 / statistics.fmean(tail),
            'p99_frame_ms': ordered[math.ceil(.99 * len(values)) - 1]}


def compare(baseline, candidate):
    a, b = metrics(baseline), metrics(candidate)
    return {'baseline': a, 'candidate': b,
            'delta_percent': {key: 100 * (b[key] / a[key] - 1)
                              for key in ('mean_fps', 'one_percent_low_fps', 'p99_frame_ms')}}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('baseline')
    parser.add_argument('candidate')
    args = parser.parse_args()
    try:
        result = compare(read_trace(args.baseline), read_trace(args.candidate))
    except (ValueError, OSError) as exc:
        parser.error(str(exc))
    print(json.dumps(result, indent=2, allow_nan=False))


if __name__ == '__main__':
    main()
