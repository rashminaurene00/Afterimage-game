"""Generate AFTERIMAGE's original synthetic gameplay and horror WAV cues."""

import math
import random
import struct
import wave
from pathlib import Path


RATE = 44_100
DURATION = 1.45
OUT = Path(__file__).resolve().parents[1] / "assets" / "audio" / "death_stinger.wav"


def write_wav(path: Path, samples: list[float], peak_target: float = 0.8) -> None:
    peak = max((abs(v) for v in samples), default=1.0) or 1.0
    gain = peak_target / peak
    path.parent.mkdir(parents=True, exist_ok=True)
    with wave.open(str(path), "wb") as wav:
        wav.setnchannels(1)
        wav.setsampwidth(2)
        wav.setframerate(RATE)
        wav.writeframes(b"".join(
            struct.pack("<h", int(max(-1.0, min(1.0, value * gain)) * 32767))
            for value in samples
        ))


def write_movement_cues() -> None:
    audio_dir = OUT.parent
    rng = random.Random(404)

    # Soft shoe scuff: filtered noise with a low impact and a quick release.
    count = int(RATE * 0.18)
    samples = []
    low = 0.0
    for i in range(count):
        t = i / RATE
        env = (1.0 - t / 0.18) ** 2
        noise = rng.random() * 2.0 - 1.0
        low += (noise - low) * 0.11
        samples.append(env * (low * 0.36 + math.sin(2 * math.pi * 72 * t) * 0.58))
    write_wav(audio_dir / "footstep.wav", samples, 0.72)

    # Sprinting has a heavier impact and bright loose-metal rattle.
    count = int(RATE * 0.24)
    samples = []
    low = 0.0
    for i in range(count):
        t = i / RATE
        env = math.exp(-t * 17.0)
        noise = rng.random() * 2.0 - 1.0
        low += (noise - low) * 0.2
        rattle = (math.sin(2 * math.pi * 930 * t) + 0.55 * math.sin(2 * math.pi * 1470 * t + 0.4))
        samples.append(env * (low * 0.38 + math.sin(2 * math.pi * 58 * t) * 0.5 + rattle * 0.13))
    write_wav(audio_dir / "sprint_clatter.wav", samples, 0.78)

    # Breath is noise shaped into two close, uneven inhale/exhale swells.
    duration = 1.15
    count = int(RATE * duration)
    samples = []
    low = 0.0
    for i in range(count):
        t = i / RATE
        cycle = (0.5 + 0.5 * math.sin(2 * math.pi * 0.92 * t - 0.8)) ** 1.5
        noise = rng.random() * 2.0 - 1.0
        low += (noise - low) * 0.018
        formant = math.sin(2 * math.pi * (150 + 22 * math.sin(2 * math.pi * 0.7 * t)) * t)
        samples.append(cycle * (low * 0.75 + formant * 0.25))
    write_wav(audio_dir / "breathing.wav", samples, 0.6)

    # Five-second tonal bed for the full on-screen scare: descending detuned
    # harmonics, pulsing low impacts and a faint filtered-noise texture.
    duration = 5.0
    count = int(RATE * duration)
    samples = []
    low = 0.0
    for i in range(count):
        t = i / RATE
        fade = min(1.0, t * 14.0) * max(0.0, min(1.0, (duration - t) * 9.0))
        sweep = 54.0 - 16.0 * (t / duration)
        drone = math.sin(2 * math.pi * sweep * t) + 0.42 * math.sin(2 * math.pi * (sweep * 1.047) * t)
        drone += 0.22 * math.sin(2 * math.pi * (sweep * 2.91) * t + 0.7)
        beat_phase = t % 1.18
        thump = math.exp(-beat_phase * 22.0) * math.sin(2 * math.pi * (58.0 - 12.0 * beat_phase) * beat_phase)
        if 0.22 < beat_phase < 0.48:
            second = beat_phase - 0.22
            thump += 0.56 * math.exp(-second * 25.0) * math.sin(2 * math.pi * 49.0 * second)
        noise = rng.random() * 2.0 - 1.0
        low += (noise - low) * 0.025
        samples.append(fade * (drone * 0.24 + thump * 0.46 + low * 0.12))
    write_wav(audio_dir / "jumpscare_bed.wav", samples, 0.72)


def main() -> None:
    rng = random.Random(4072)
    count = int(RATE * DURATION)
    samples = [0.0] * count
    raw = [0.0] * count

    for i in range(count):
        t = i / RATE
        # Sudden broadband impact with a low, quickly falling body.
        impact_env = math.exp(-t * 38.0)
        impact = (rng.random() * 2.0 - 1.0) * impact_env * 0.42
        impact += math.sin(2.0 * math.pi * (82.0 - 40.0 * min(t * 3.0, 1.0)) * t) * impact_env * 0.55

        # A rising, strained synthetic shriek that bends down into the impact.
        u = max(0.0, t - 0.035)
        rise = min(u / 0.48, 1.0)
        fall = max(0.0, 1.0 - max(0.0, u - 0.72) / 0.58)
        freq = 185.0 + 790.0 * rise - 390.0 * max(0.0, u - 0.62)
        vibrato = 1.0 + 0.014 * math.sin(2.0 * math.pi * 7.3 * t)
        phase = 2.0 * math.pi * freq * vibrato * t
        voice = (math.sin(phase) + 0.42 * math.sin(2.0 * phase + 0.3)
                 + 0.20 * math.sin(3.07 * phase + 1.1))
        rasp = (rng.random() * 2.0 - 1.0) * (0.12 + 0.2 * rise)
        attack = min(1.0, u / 0.045)
        raw[i] = impact + attack * fall * (0.52 * voice + rasp)

    # Short, dark slapback makes the one-shot feel larger without a long tail.
    delay = int(RATE * 0.105)
    for i, value in enumerate(raw):
        echo = raw[i - delay] * 0.23 if i >= delay else 0.0
        samples[i] = math.tanh((value + echo) * 1.5)

    write_wav(OUT, samples, 0.82)
    print(f"Wrote {OUT} ({count} samples, {DURATION:.2f}s)")
    write_movement_cues()
    print(f"Wrote movement cues in {OUT.parent}")


if __name__ == "__main__":
    main()
