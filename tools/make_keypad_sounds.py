"""Makes the keypad's sounds (res/sounds/keypad_*.wav): what a door keypad really makes, a piezo
buzzer's tone (a square-ish wave from a small disc, its harmonics softened by the disc and the case),
and the key's own little click under the finger.

    python3 tools/make_keypad_sounds.py

- keypad_beep    a key pressed: the click, then a short beep
- keypad_accept  the right code: a short beep, then a long higher one
- keypad_deny    a wrong code: three short low beeps
(these two after a moment's silence: they answer the last key, whose beep plays as it is pressed)
"""
import math, os, random, struct, wave

RATE = 44100
OUT = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'res', 'sounds')


def tone(freq, seconds, level=0.32):
    """a piezo beep: odd harmonics (a square wave, band-limited), the higher ones damped as a small
    disc damps them, a 3 ms rise and an 8 ms fall (no click at its ends)"""
    n = int(seconds * RATE)
    out = []
    for i in range(n):
        t = i / RATE
        s = 0.0
        k = 1
        while k * freq < 12000:
            s += math.sin(2 * math.pi * k * freq * t) / k * (1.0 / (1.0 + (k * freq / 6000.0) ** 2))
            k += 2
        envelope = min(1.0, t / 0.003, (seconds - t) / 0.008)
        out.append(s * level * envelope)
    return out


def click(seconds=0.006, level=0.25, seed=1):
    """the key going down: a short burst of noise, low-passed, falling away fast"""
    rng = random.Random(seed)
    out, last = [], 0.0
    for i in range(int(seconds * RATE)):
        last += (rng.uniform(-1, 1) - last) * 0.35
        out.append(last * level * math.exp(-i / (RATE * 0.0012)))
    return out


def silence(seconds):
    return [0.0] * int(seconds * RATE)


def write(name, samples):
    path = os.path.join(OUT, name + '.wav')
    with wave.open(path, 'wb') as f:
        f.setnchannels(1)
        f.setsampwidth(2)
        f.setframerate(RATE)
        f.writeframes(b''.join(struct.pack('<h', int(max(-1.0, min(1.0, s)) * 32767)) for s in samples))
    print('%-14s %.2f s  %s' % (name, len(samples) / RATE, os.path.relpath(path)))


write('keypad_beep', click() + silence(0.012) + tone(2700, 0.085) + silence(0.05))
write('keypad_accept', silence(0.2) + tone(2700, 0.11) + silence(0.07) + tone(3200, 0.42) + silence(0.05))
write('keypad_deny', silence(0.2) + tone(1900, 0.09) + silence(0.07) + tone(1900, 0.09) + silence(0.07) + tone(1900, 0.09)
      + silence(0.05))
