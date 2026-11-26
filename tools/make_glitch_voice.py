#!/usr/bin/env python3
"""A public-address announcement: a chime ("ding-dong"), then a voice line. Then its voice breaking down
(the chime always clean), stage by stage, the way audio does when the sound card is fought over
or its buffer runs dry (an underrun): slices of it stuttering, dropping out with a click, jumping in
pitch (one program at 44.1 kHz, another at 48), and at the end a sliver repeated into a metallic buzz
(the "robot voice" of a bad call). Cursed: without its first words (the greeting: "Good
morning!"), deeper, breaking up; the worst also swelling out of its own echo before each word (a
reversed echo), and after it one word of the line again and again, deeper each time ("remain...
remain... remain..."). Never slower: like a real underrun the stream runs on in real time, and what a
glitch covered is lost (the words it ate), so the line keeps its length and pace.

Usage (from the repository root):
  python3 tools/make_glitch_voice.py NAME [--seed 2]

reads res/sounds/NAME.wav (mono, 16-bit: tools/import_sounds.py makes it) and writes, in the order
they play: res/sounds/NAME_1.wav (friendly: as it is), NAME_2.wav (cursed), NAME_3.wav (more
cursed). A fixed seed: the same files every time.
"""
import argparse
import math
import random
import struct
import wave

# how often each fault hits a slice of the voice. stutter: a slice played again 1-3 more times;
# drop: a slice gone (silence); pitch: a run of slices played 9 % fast or slow; buzz: a 5-12 ms
# sliver repeated across the slice; stuck: the buffer stuck, an 8-40 ms piece repeated hard (no
# smoothing between the repeats: the gritty "brrrr" of a real underrun) for stuck_for seconds
CURSED = dict(stutter=0.22, drop=0.06, pitch=0.12, buzz=0.16, stuck=0.07, stuck_for=(0.12, 0.4))
MORE_CURSED = dict(stutter=0.26, drop=0.07, pitch=0.25, buzz=0.32, stuck=0.20, stuck_for=(0.15, 0.6))
# how deep (pitch, 1: as it is): the cursed, the more cursed
DEEP = (0.84, 0.66)
# the chime before the voice: its notes (Hz, falling: "ding-dong"), when each is struck (s), how
# long each rings (s), the pause after it before the voice (s), how loud against the voice (1: as loud)
CHIME_NOTES, CHIME_STRIKE, CHIME_RING, CHIME_PAUSE, CHIME_LOUD = (659.3, 523.3), 0.45, 1.3, 0.35, 1.0
# the more cursed, after the line: words of it again (seconds into NAME.wav: "and remain", the end
# found by having a speech recogniser transcribe cuts of it: "in" starts by 2.72), how many times, how deep each, the
# pause between them and before the first (s), and how broken each is
TAIL_WORD, TAIL_DEEP, TAIL_GAP, TAIL_LEAD = (2.19, 2.69), (0.80, 0.63, 0.48), 0.75, 0.2
TAIL_FAULTS = dict(stutter=0.0, drop=0.0, pitch=0.0, buzz=0.0, stuck=0.0, stuck_for=(0.1, 0.2))
TAIL_ECHO = 0.15        # (its reversed echo faint: the word stays clear)
# the more cursed: the reversed echo's delay (s), feedback, and how far it swells ahead (s)
ECHO_DELAY, ECHO_FEEDBACK, ECHO_AHEAD = 0.13, 0.68, 0.35


def read(path):
    with wave.open(path, 'rb') as w:
        assert w.getnchannels() == 1 and w.getsampwidth() == 2, 'mono 16-bit expected'
        rate = w.getframerate()
        data = w.readframes(w.getnframes())
    return [v / 32768.0 for v in struct.unpack('<%dh' % (len(data) // 2), data)], rate


def write(path, samples, rate):
    with wave.open(path, 'wb') as w:
        w.setnchannels(1)
        w.setsampwidth(2)
        w.setframerate(rate)
        w.writeframes(b''.join(struct.pack('<h', int(max(-1.0, min(1.0, v)) * 32767)) for v in samples))


def resampled(samples, start, length, factor):
    """length samples read from start at factor times the speed (linear interpolation)"""
    out = []
    for k in range(length):
        x = start + k * factor
        i = int(x)
        if i + 1 >= len(samples):
            break
        f = x - i
        out.append(samples[i] * (1.0 - f) + samples[i + 1] * f)
    return out


def chime(rate):
    """a PA chime: each note a struck bell (a soft attack, its overtones, ringing away)"""
    length = int(rate * (CHIME_STRIKE * (len(CHIME_NOTES) - 1) + CHIME_RING))
    out = [0.0] * length
    for n, freq in enumerate(CHIME_NOTES):
        start = int(rate * CHIME_STRIKE * n)
        for k in range(min(int(rate * CHIME_RING), length - start)):
            t = k / rate
            ring = math.exp(-t * 3.2) * min(1.0, t / 0.005)
            ring *= min(1.0, (CHIME_RING - t) / 0.25)                   # (out to silence: no click)
            tone = (math.sin(2 * math.pi * freq * t) + 0.3 * math.sin(2 * math.pi * freq * 2 * t) * math.exp(-t * 4) +
                    0.1 * math.sin(2 * math.pi * freq * 3 * t) * math.exp(-t * 6) +
                    0.08 * math.sin(2 * math.pi * freq * 2.76 * t) * math.exp(-t * 8))
            out[start + k] += tone * ring
    top = max(abs(v) for v in out)
    return [v / top for v in out]


def faded(piece, n, tail=True):
    """the piece faded in (and out: tail) over n samples: a cut without a click"""
    piece = list(piece)
    n = min(n, len(piece) // 2)
    for k in range(n):
        w = k / n
        piece[k] *= w
        if tail:
            piece[-1 - k] *= w
    return piece


def fade_tail(out, n):
    for k in range(min(n, len(out))):
        out[-1 - k] *= k / n


def broken(samples, rate, faults, rng, counts):
    out = []
    edge = int(rate * 0.002)                              # every cut faded over 2 ms: no cracks
    cut = False                                           # the last piece ended in a cut
    quiet = 0.03 * max(abs(v) for v in samples)          # (between words: a fault there is not heard)
    i = 0
    pitch_left, factor = 0, 1.0
    while i < len(samples):
        size = int(rate * rng.uniform(0.02, 0.06))
        if pitch_left == 0 and rng.random() < faults['pitch']:
            pitch_left, factor = rng.randint(3, 9), rng.choice((48000 / 44100, 44100 / 48000))
            counts['pitch'] += 1
            cut = True                                    # (the jump in: a cut)
        jumped_back = False
        if pitch_left > 0:
            grain = resampled(samples, i, size, factor)
            i += int(size * factor)
            pitch_left -= 1
            if pitch_left == 0 or len(grain) < size:
                grain = faded(grain, edge)                # (the jump back: a cut)
                fade_tail(out, edge)
                cut = jumped_back = True
        else:
            grain = samples[i:i + size]
            i += size
        if not grain:
            break
        if max(abs(v) for v in grain) < quiet:
            if cut:
                fade_tail(out, edge)
            out.extend(faded(grain, edge, tail=False) if cut else grain)
            cut = jumped_back
            continue
        r = rng.random()
        fault = r < faults['drop'] + faults['buzz'] + faults['stutter'] + faults['stuck']
        if fault or cut:
            fade_tail(out, edge)
            grain_in = faded(grain, edge)
        else:
            grain_in = grain
        before = len(out)
        if r < faults['drop']:
            out.extend([0.0] * len(grain))                      # gone: silence
            counts['drop'] += 1
        elif r < faults['drop'] + faults['buzz']:
            sliver = faded(grain[:max(8, int(rate * rng.uniform(0.005, 0.012)))], max(2, edge // 4))
            buzz = (sliver * (len(grain) // len(sliver) + 2))[:len(grain) * rng.randint(1, 3)]
            out.extend(buzz)                                    # a sliver over and over: metallic
            fade_tail(out, edge)                                # (cut off mid-sliver: faded)
            counts['buzz'] += 1
        elif r < faults['drop'] + faults['buzz'] + faults['stutter']:
            for _ in range(rng.randint(2, 4)):
                out.extend(grain_in)                            # the same slice again
            counts['stutter'] += 1
        elif fault:
            loop = grain[:max(16, int(rate * rng.uniform(0.008, 0.04)))]
            stuck = loop * max(2, int(rate * rng.uniform(*faults['stuck_for']) / len(loop)))
            out.extend(faded(stuck, edge))                      # the buffer stuck: brrrr
            counts['stuck'] += 1
        else:
            out.extend(faded(grain, edge, tail=jumped_back) if cut else grain)   # (on from the cut)
        cut = fault or jumped_back
        if fault:
            i += max(0, (len(out) - before) - len(grain))       # (real time runs on: what it covered is lost)
    return out


def deeper(samples, rate, factor, slice_for_depth=False):
    """lower in pitch, the timing kept: short overlapping slices each read slower (so deeper) and laid
    back where they were. slice_for_depth: longer slices the deeper it goes (short ones made a very
    deep voice rough)"""
    size = int(rate * (min(0.1, 0.037 / factor) if slice_for_depth else 0.046))
    hop = size // 2
    window = [0.5 - 0.5 * math.cos(2 * math.pi * k / size) for k in range(size)]
    out = [0.0] * (len(samples) + size)
    for start in range(0, len(samples), hop):
        grain = resampled(samples, start, size, factor)
        for k, v in enumerate(grain):
            out[start + k] += v * window[k]
    return out[:len(samples)]


def reversed_echo(samples, rate, feedback=None):
    """each word swelling out of its own echo before it is spoken"""
    feedback = ECHO_FEEDBACK if feedback is None else feedback
    rev = samples[::-1] + [0.0] * int(rate * ECHO_AHEAD)
    delay = int(rate * ECHO_DELAY)
    for n in range(delay, len(rev)):
        rev[n] += feedback * rev[n - delay]
    return rev[::-1]


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('name')
    parser.add_argument('--seed', type=int, default=2)
    args = parser.parse_args()

    voice, rate = read('res/sounds/%s.wav' % args.name)
    peak = max(abs(v) for v in voice) or 1.0
    # the announcement: the chime, a pause, the voice
    lead = [v * peak * CHIME_LOUD for v in chime(rate)] + [0.0] * int(rate * CHIME_PAUSE)
    samples = lead + voice
    # its first words (the greeting): up to its first pause (a fifth of a second quiet); the cursed
    # ones go straight from the chime to the words after it
    quiet = 0.03 * peak
    run, rest = 0, 0
    for k in range(next(k for k, v in enumerate(voice) if abs(v) > quiet), len(voice)):
        run = run + 1 if abs(voice[k]) < quiet else 0
        if run > rate * 0.2:
            rest = next(n for n in range(k, len(voice)) if abs(voice[n]) > quiet)
            break
    cursed_voice = voice[max(0, rest - int(rate * 0.03)):]

    def save(number, out, what):
        top = max(abs(v) for v in out) or 1.0
        out = [v * peak * max(1.0, CHIME_LOUD) / top for v in out]   # as loud as the announcement
        write('res/sounds/%s_%d.wav' % (args.name, number), out, rate)
        print('res/sounds/%s_%d.wav: %.2f s, %s' % (args.name, number, len(out) / rate, what))

    save(1, samples, 'friendly: as it is')
    for number, faults, k in ((2, CURSED, 0), (3, MORE_CURSED, 1)):
        counts = dict(stutter=0, drop=0, pitch=0, buzz=0, stuck=0)
        out = deeper(cursed_voice, rate, DEEP[k])
        out = broken(out, rate, faults, random.Random(args.seed * 100 + number), counts)
        if number == 3:
            out = reversed_echo(out, rate)
            word = faded(voice[int(rate * TAIL_WORD[0]):int(rate * TAIL_WORD[1])], int(rate * 0.012))
            tail, rng = [], random.Random(args.seed * 100 + 33)
            for n, factor in enumerate(TAIL_DEEP):
                said = broken(deeper(word, rate, factor, slice_for_depth=True), rate, TAIL_FAULTS, rng, dict(counts))
                tail += [0.0] * int(rate * (TAIL_GAP if n else TAIL_LEAD)) + said
            tail = reversed_echo(tail, rate, TAIL_ECHO)
            # (the echo's lead-in is near silence: off, the first word right after the line)
            loud = max(abs(v) for v in tail)
            heard = next(k for k, v in enumerate(tail) if abs(v) > 0.02 * loud)
            tail = tail[max(0, heard - int(rate * TAIL_LEAD)):]
            top_body, top_tail = max(abs(v) for v in out) or 1.0, max(abs(v) for v in tail) or 1.0
            out = out + [v * top_body / top_tail for v in tail]
        top = max(abs(v) for v in out) or 1.0
        out = lead + [v * peak / top for v in out]              # the chime clean, before it
        save(number, out, 'cursed: ' + ', '.join('%d %s' % (n, k) for k, n in counts.items()))
    print('(the cursed ones start the voice at %.2f s of it: after the greeting)' % (rest / rate))

if __name__ == '__main__':
    main()
