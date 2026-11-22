"""Imports recorded sound files (any format ffmpeg reads: wav, ogg, flac,
mp3...) into the engine as res/sounds/<name>.wav: mono, 44.1 kHz, 16-bit,
trimmed, levelled, faded; loops get a crossfaded seam so they repeat
without a click.

usage:
  python3 tools/import_sounds.py NAME FILE [--start S] [--length S] [--loop] [--gain DB]

  NAME     the engine's sound name, e.g. step_grass_1, wind_loop, splash
  --start  where to begin in the file (seconds)
  --length how much to keep (seconds; default: to the end)
  --loop   make it a seamless loop (for *_loop sounds)
  --gain   extra level in dB after normalising (default 0)

The names the engine uses: wind_loop, crickets_loop, fire_loop (--loop),
step_grass_1..4, land, wade_1..3, splash, hiss, boom_1..3. Record where each
came from in res/sounds/CREDITS.md.
"""
import argparse, math, os, struct, subprocess, sys, tempfile, wave

RATE = 44100


def decode(path, start, length):
    """the file as mono float samples at 44.1 kHz (via ffmpeg)"""
    command = ['ffmpeg', '-v', 'error', '-y']
    if start > 0:
        command += ['-ss', str(start)]
    command += ['-i', path]
    if length > 0:
        command += ['-t', str(length)]
    command += ['-ac', '1', '-ar', str(RATE), '-f', 's16le', '-']
    raw = subprocess.run(command, check=True, capture_output=True).stdout
    n = len(raw) // 2
    return [s / 32768.0 for s in struct.unpack('<%dh' % n, raw[:n * 2])]


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('name')
    parser.add_argument('file')
    parser.add_argument('--start', type=float, default=0.0)
    parser.add_argument('--length', type=float, default=0.0)
    parser.add_argument('--loop', action='store_true')
    parser.add_argument('--gain', type=float, default=0.0)
    args = parser.parse_args()

    samples = decode(args.file, args.start, args.length)
    if not samples:
        sys.exit('no audio decoded from ' + args.file)
    # remove any DC offset
    mean = sum(samples) / len(samples)
    samples = [s - mean for s in samples]

    if args.loop:
        # crossfade the last half second into the start: a seamless loop
        fade = min(int(0.5 * RATE), len(samples) // 4)
        body, tail = samples[:-fade], samples[-fade:]
        for i in range(fade):
            t = i / fade
            body[i] = body[i] * math.sqrt(t) + tail[i] * math.sqrt(1.0 - t)
        samples = body
    else:
        # short fades so a one-shot never clicks
        edge = min(int(0.004 * RATE), len(samples) // 2)
        for i in range(edge):
            samples[i] *= i / edge
        tailFade = min(int(0.05 * RATE), len(samples) // 2)
        for i in range(tailFade):
            samples[-1 - i] *= i / tailFade

    # normalise the peak to -1 dB, then the extra gain
    peak = max(abs(s) for s in samples) or 1.0
    gain = 10 ** (-1.0 / 20.0) / peak * 10 ** (args.gain / 20.0)

    out = os.path.join('res', 'sounds', args.name + '.wav')
    os.makedirs(os.path.dirname(out), exist_ok=True)
    with wave.open(out, 'wb') as w:
        w.setnchannels(1)
        w.setsampwidth(2)
        w.setframerate(RATE)
        w.writeframes(b''.join(struct.pack('<h', int(max(-1.0, min(1.0, s * gain)) * 32767)) for s in samples))
    print('%s: %.2f s from %s' % (out, len(samples) / RATE, args.file))


if __name__ == '__main__':
    main()
