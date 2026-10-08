#!/usr/bin/env python3
"""Generate original synthetic M8 training cards with Python's standard library.

The writer uses the Song 6.0.1 flat layout and FAT16. It constructs all
records from format fields. It does not read song, instrument or sample assets.
Generated samples use integer arithmetic so their bytes do not depend on libm.
"""
from __future__ import annotations

import argparse
import collections
import hashlib
import json
from pathlib import Path
import struct

DEFAULT_SEED = 0x4D38
KINDS = {"WAVSYNTH": 0, "MACROSYNTH": 1, "SAMPLER": 2,
         "FMSYNTH": 4, "HYPERSYNTH": 5}
COMMON = {"WAVSYNTH": 23, "MACROSYNTH": 23, "SAMPLER": 24,
          "FMSYNTH": 51, "HYPERSYNTH": 30}
SONG_BYTES = 0x1B6A6
SONG = 0x2EE
PHRASES = 0xAEE
CHAINS = 0x9A5E
TABLES = 0xBA3E
INSTRUMENTS = 0x13A3E
FX = 0x1A5C1
SCALES = 0x1AA7E
EQ = 0x1AD5E
INSTRUMENT_BYTES = 215
ANCHORS = ("idle-dry", "simple", "eight-heavy", "eight-mixed")
COMMANDS = {"RET": 0x08, "REP": 0x09, "PSL": 0x0C, "PVB": 0x0E,
            "PIT": 0x81, "FIN": 0x82, "CUT": 0x89, "RES": 0x8A, "PAN": 0x8D}
ENGINE_CONTROL = {"WAVSYNTH": "SIZ", "MACROSYNTH": "TBR", "SAMPLER": "STA",
                  "FMSYNTH": "FM1", "HYPERSYNTH": "SWM"}


class Random:
    """A fixed xorshift32 sequence; independent of Python's random module."""

    def __init__(self, seed):
        self.state = seed & 0xFFFFFFFF or 1

    def next(self):
        x = self.state
        x ^= (x << 13) & 0xFFFFFFFF
        x ^= x >> 17
        x ^= (x << 5) & 0xFFFFFFFF
        self.state = x & 0xFFFFFFFF
        return self.state

    def shuffle(self, values):
        for i in range(len(values) - 1, 0, -1):
            j = self.next() % (i + 1)
            values[i], values[j] = values[j], values[i]


def _quantile(sorted_values, fraction):
    return sorted_values[round((len(sorted_values) - 1) * fraction)]


def _apportion(counts, total):
    """Largest-remainder allocation with a stable tie break."""
    mass = sum(counts.values())
    result = {k: counts[k] * total // mass for k in counts}
    order = sorted(counts, key=lambda k: (-((counts[k] * total) % mass), k))
    for k in order[:total - sum(result.values())]:
        result[k] += 1
    return result


def _allocate_feature(rows, rng, project_count, instrument_count):
    """Select nonempty projects, then fill an exact instrument-level quota."""
    indices = list(range(len(rows)))
    rng.shuffle(indices)
    active = indices[:project_count]
    selected = set()
    for index in active:
        selected.add((index, rng.next() % len(rows[index]["tracks"])))
    available = [(index, track) for index in active for track in range(len(rows[index]["tracks"]))
                 if (index, track) not in selected]
    rng.shuffle(available)
    if instrument_count > len(available) + len(selected):
        raise ValueError("Feature allocation exceeds the active instrument slots")
    selected.update(available[:max(0, instrument_count - len(selected))])
    return [[track for track in range(len(row["tracks"])) if (index, track) in selected]
            for index, row in enumerate(rows)]


def _allocate_variants(rows, rng, calibration):
    """Match unique-instrument density without changing tracks or assets."""
    total = round(len(rows) * calibration["instrument_count"]["mean"])
    goals = _apportion(calibration["engines"], total)
    features = {"filter": calibration["filters"], **calibration["sends"]}
    feature_goals = {k: round(total * v["instruments"] / sum(calibration["engines"].values()))
                     for k, v in features.items()}
    entries = [(i, t) for i, r in enumerate(rows) for t in range(len(r["tracks"]))]
    rng.shuffle(entries)
    variants = [[1] * len(r["tracks"]) for r in rows]
    light = {i for i, r in enumerate(rows) if len(r["tracks"]) == 1}
    actual = collections.Counter(engine for r in rows for engine in r["tracks"])
    dense = set()
    # Retain three dense, fast arrangements, but bound every engine quota.
    for i in reversed(range(len(rows))):
        additions = collections.Counter(rows[i]["tracks"])
        if len(rows[i]["tracks"]) == 8 and all(actual[k] + 2 * n <= goals[k] for k, n in additions.items()):
            variants[i] = [3] * 8
            actual.update({k: 2 * n for k, n in additions.items()})
            dense.add(i)
            if len(dense) == 3:
                break
    membership = {(i, t): {k: int(t in rows[i][k + "_tracks"]) for k in features}
                  for i, t in entries}
    counts = {k: sum(variants[i][t] * membership[i, t][k] for i, t in entries) for k in features}

    def error(delta):
        return sum((counts[k] + delta[k] - feature_goals[k]) ** 2 for k in features)

    while sum(actual.values()) < total:
        candidates = [(error(membership[i, t]), variants[i][t], rank, i, t)
                      for rank, (i, t) in enumerate(entries)
                      if i not in light and variants[i][t] < 3
                      and actual[rows[i]["tracks"][t]] < goals[rows[i]["tracks"][t]]]
        if not candidates:
            raise ValueError("Instrument variant quotas cannot be allocated")
        _, _, _, i, t = min(candidates)
        variants[i][t] += 1
        actual[rows[i]["tracks"][t]] += 1
        for k in features:
            counts[k] += membership[i, t][k]
    # Same-engine exchanges keep the exact engine quota and reduce feature error.
    for _ in range(len(entries)):
        current = error(dict.fromkeys(features, 0))
        best = None
        for i, t in entries:
            if i in dense or i in light or variants[i][t] <= 1:
                continue
            for j, u in entries:
                if j in dense or j in light or variants[j][u] >= 3 or rows[i]["tracks"][t] != rows[j]["tracks"][u]:
                    continue
                delta = {k: membership[j, u][k] - membership[i, t][k] for k in features}
                score = error(delta)
                if score < current and (best is None or score < best[0]):
                    best = score, i, t, j, u, delta
        if best is None:
            break
        _, i, t, j, u, delta = best
        variants[i][t] -= 1
        variants[j][u] += 1
        for k in features:
            counts[k] += delta[k]
    for row, selected in zip(rows, variants):
        row["instrument_variants"] = selected


def _allocate_phrase_controls(rows, rng, calibration):
    """Place original controls at the measured sparse phrase-FX density."""
    events = [(i, t, step) for i, r in enumerate(rows) for t in range(len(r["tracks"]))
              for step in range(0, 15, 4 if t % 3 == 0 else 2)]
    active = len(events) + sum(len(r["tracks"]) for r in rows)  # Include unchanged OFF rows.
    density = calibration["phrase_fx"]
    quotas = _apportion(density["active_row_fx_count_histogram"], active)
    commands = sum(int(k) * n for k, n in quotas.items())
    lane_goal = _apportion(density["fx_lane_counts"], commands)
    lanes = collections.Counter()
    for row in rows:
        row["phrase_controls"] = []
    early = [(i, i % len(r["tracks"]), 0) for i, r in enumerate(rows)]
    events = [event for event in events if event not in early]
    rng.shuffle(events)
    for count in (3, 2, 1):
        for _ in range(quotas[str(count)]):
            i, track, step = early.pop() if count == 1 and early else events.pop()
            available = sorted(range(3), key=lambda lane: (lanes[str(lane)] - lane_goal[str(lane)], lane))[:count]
            effects = []
            for lane in sorted(available):
                selector = (step + track + rows[i]["variation"] + lane) % 7
                filtered = track in rows[i]["filter_tracks"]
                name = ("CUT" if filtered else "PAN", "RES" if filtered else "FIN", "PIT",
                        ENGINE_CONTROL[rows[i]["tracks"][track]], "RET", "PSL", "PVB")[selector]
                code = COMMANDS.get(name, 0x84)
                value = {"CUT": 80 + (step * 7 + track * 11) % 144,
                         "PAN": 64 + (step * 7 + track * 11) % 128,
                         "RES": 8 + (step + track) % 40, "FIN": 1,
                         "PIT": 1 if step % 4 == 0 else 255, "RET": 2,
                         "PSL": 3, "PVB": 0x12}.get(name, 16 + (step * 3 + track * 5) % 80)
                effects.append({"slot": lane, "command": name, "code": code, "value": value})
                lanes[str(lane)] += 1
            rows[i]["phrase_controls"].append({"track": track, "row": step, "effects": effects})
    # Repeat only between two explicit commands in the same lane. The final
    # command cancels REP before the unchanged OFF row and phrase wrap.
    for row in rows:
        row["phrase_controls"].sort(key=lambda e: (e["track"], e["row"]))
        for track in range(len(row["tracks"])):
            for lane in range(3):
                selected = [fx for event in row["phrase_controls"] if event["track"] == track
                            for fx in event["effects"] if fx["slot"] == lane]
                if len(selected) >= 3:
                    name = "CUT" if track in row["filter_tracks"] else "PAN"
                    for fx, command, value in zip(selected[:3], (name, "REP", name), (128, 1, 160)):
                        fx.update(command=command, code=COMMANDS[command], value=value)
                    break


def specifications(seed=DEFAULT_SEED):
    """Return 24 representative rows and four low-cost coverage anchors."""
    calibration = json.loads(Path(__file__).with_name("calibration.json").read_text())
    rng = Random(seed)
    # 19/24 eight-track rows approximates 171/203. One-track/light rows are
    # deliberately retained instead of rounding all small bins to zero.
    counts = [1, 4, 5, 6, 7] + [8] * 19
    tempos = sorted(float(tempo) for tempo, n in calibration["tempo_bpm"]["histogram"].items()
                    for _ in range(n))
    allocation = _apportion(calibration["engines"], sum(counts))
    remaining = []
    for engine in sorted(allocation):
        if engine != "SAMPLER":
            remaining.extend([engine] * allocation[engine])
    rng.shuffle(remaining)
    # Spread the sampler instruments over 8/24 projects (65/203 references).
    sampler_rows = (7, 9, 11, 13, 15, 17, 20, 23)
    sampler_counts = _apportion({str(i): 1 for i in sampler_rows}, allocation["SAMPLER"])
    sample_budgets = (8244, 633678, 1000000, 1874494, 4000000, 7751518, 14935430, 42272362)
    rows = []
    for index, count in enumerate(counts):
        n = sampler_counts.get(str(index), 0)
        engines = ["SAMPLER"] * n + [remaining.pop() for _ in range(count - n)]
        rng.shuffle(engines)
        sample_budget = sample_budgets[sampler_rows.index(index)] if n else 0
        rows.append({"name": f"train-{index + 1:02d}", "role": "representative",
                     "tempo_bpm": round(_quantile(tempos, index / (len(counts) - 1)), 3),
                     "tracks": engines, "sample_bytes_target": sample_budget,
                     "seconds": 2, "non_silent": True, "variation": index})
    total_instruments = sum(calibration["engines"].values())
    slots = sum(counts)
    features = {"filter": calibration["filters"], **calibration["sends"]}
    for feature, observed in features.items():
        assignments = _allocate_feature(rows, rng,
                                        round(len(rows) * observed["projects"] / calibration["projects"]),
                                        round(slots * observed["instruments"] / total_instruments))
        for row, selected in zip(rows, assignments):
            row[feature + "_tracks"] = selected
    _allocate_variants(rows, rng, calibration)
    _allocate_phrase_controls(rows, rng, calibration)
    rows += [
        {"name": "idle-dry", "role": "anchor", "tempo_bpm": 120, "tracks": [],
         "sample_bytes_target": 0, "seconds": 2, "non_silent": False, "variation": 0},
        {"name": "simple", "role": "anchor", "tempo_bpm": 120, "tracks": ["WAVSYNTH"],
         "sample_bytes_target": 0, "seconds": 2, "non_silent": True, "variation": 1},
        {"name": "eight-heavy", "role": "anchor", "tempo_bpm": 160,
         "tracks": ["FMSYNTH"] * 4 + ["HYPERSYNTH"] * 2 + ["MACROSYNTH"] * 2,
         "sample_bytes_target": 0, "seconds": 2, "non_silent": True, "variation": 2},
        {"name": "eight-mixed", "role": "anchor", "tempo_bpm": 120,
         "tracks": ["SAMPLER"] * 2 + ["FMSYNTH"] * 2 + ["WAVSYNTH"] * 2
                   + ["MACROSYNTH", "HYPERSYNTH"],
         "sample_bytes_target": 176488, "seconds": 2, "non_silent": True, "variation": 3},
    ]
    return rows


def sample_wav(seed, target_bytes, variant):
    """Return generated PCM WAV bytes; tile integer tone/noise waveforms."""
    channels = 2 if variant % 3 else 1
    width = 3 if variant % 4 == 3 else 2
    rate = 22050 if variant % 5 == 4 else 44100
    frame_bytes = channels * width
    frames = max(256, (target_bytes - 44) // frame_bytes)
    rng = Random(seed)
    period = 2048
    tile = bytearray()
    for i in range(period):
        phase = (i * (37 + variant * 7)) & 1023
        triangle = (phase if phase < 512 else 1023 - phase) * 48 - 12000
        noise = (rng.next() & 16383) - 8192
        envelope = period - i
        value = ((triangle * 3 + noise) // 4) * envelope // period
        for channel in range(channels):
            v = value if not channel else value * 3 // 4
            tile.extend(struct.pack("<h", v) if width == 2
                        else (v << 8).to_bytes(3, "little", signed=True))
    payload = bytes(tile) * (frames // period) + bytes(tile[:(frames % period) * frame_bytes])
    return (b"RIFF" + struct.pack("<I", 36 + len(payload)) + b"WAVEfmt "
            + struct.pack("<IHHIIHH", 16, 1, channels, rate, rate * frame_bytes,
                          frame_bytes, width * 8)
            + b"data" + struct.pack("<I", len(payload)) + payload), {
                "channels": channels, "sample_bits": width * 8, "sample_rate": rate,
                "frames": frames, "bytes": len(payload) + 44,
                "waveform": "integer triangle plus seeded noise with repeated decay",
            }


def _instrument(engine, index, variation, sample_path, filtered, sends, anchor):
    record = bytearray(INSTRUMENT_BYTES)
    record[0] = KINDS[engine]
    record[1:13] = f"PGO{index:02d}".encode().ljust(12, b"\xff")
    record[13:18] = bytes((1, 1, 0, 0, 128))  # table ticks, pitch and fine tune
    if engine == "WAVSYNTH":
        shape = 6 if anchor else (0, 1, 2, 6, 8, 9, 15, 31)[variation % 8]
        record[18:23] = bytes((shape, 32, 0, 0, variation * 9 % 256))
    elif engine == "MACROSYNTH":
        shapes = (0, 1, 3, 12, 25, 28, 34, 35, 36, 37, 39, 43)
        record[18:23] = bytes((shapes[variation % len(shapes)],
                              32 + variation * 17 % 192, 64 + variation * 23 % 128, 0, 0))
    elif engine == "SAMPLER":
        record[18:24] = bytes(((0, 2, 3)[variation % 3], 0, 0, 0, 255, 0))
        record[87:215] = sample_path.encode().ljust(128, b"\0")
    elif engine == "FMSYNTH":
        record[18] = variation % 12
        record[19:23] = bytes((0, 0, 0, 0))
        record[23:31] = bytes((1, 0, 2, 0, 3, 0, 1, 0))
        record[31:39] = bytes((48, 0, 64, 0, 96, 0, 192, 0))
    elif engine == "HYPERSYNTH":
        chord = bytes((7, 0, 4, 7, 0, 0, 0))
        record[18:30] = chord + bytes((0, 0, variation * 3 % 64, 96, 128))
        record[87:199] = chord * 16
    offset = COMMON[engine]
    record[offset:offset + 10] = bytes((1 + variation % 7 if filtered else 0,
                                      96 + variation * 7 % 128, variation * 5 % 64,
                                      0, 0, 128, 192, *sends))
    # AHD and ADSR fields from the format; each voice owns its envelope.
    if not anchor:
        record[63:69] = bytes((0x11, 255, 0, 8, 192, 8))
    return record


def build_project(spec, seed):
    """Construct one complete Song and all of its generated dependencies."""
    song = bytearray(SONG_BYTES)
    song[:14] = b"M8VERSION\0" + bytes((1, 6, 0, 0))
    song[14:21] = b"/Songs/"
    struct.pack_into("<f", song, 0x8F, spec["tempo_bpm"])
    song[0x94:0xA0] = b"PGO".ljust(12, b"\0")
    # MIDI channels and the default mixer are format values, not asset blobs.
    song[0xA0 + 12:0xA0 + 20] = bytes(range(1, 9))
    song[0xCE:0xDE] = bytes((224, 0, 224, 224, 224, 224, 224, 224,
                            224, 224, 224, 224, 224, 0, 255, 0))
    song[0xCE + 24] = 128
    song[0xCE + 31] = 50
    for i in range(32):
        ticks = (7, 5) if spec["variation"] % 4 == 3 else (6, 6)
        song[0xEE + i * 16:0xEE + (i + 1) * 16] = bytes(ticks) + b"\xff" * 14
    song[SONG:PHRASES] = b"\xff" * (PHRASES - SONG)
    song[PHRASES:CHAINS] = bytes((255, 255, 255, 255, 0, 255, 0, 255, 0)) * (255 * 16)
    song[CHAINS:TABLES] = bytes((255, 0)) * (255 * 16)
    song[TABLES:INSTRUMENTS] = bytes((0, 255, 255, 0, 255, 0, 255, 0)) * (256 * 16)
    for i in range(128):
        start = INSTRUMENTS + i * INSTRUMENT_BYTES
        song[start:start + INSTRUMENT_BYTES] = b"\xff" * INSTRUMENT_BYTES
        song[start + 87:start + INSTRUMENT_BYTES] = bytes(INSTRUMENT_BYTES - 87)
    # ModFX chorus, dotted delay and reverb with nonzero return gains.
    song[FX:FX + 22] = bytes((64, 128, 255, 0, 253, 175, 38, 64, 255,
                            48, 48, 128, 255, 0, 65, 16, 224, 255, 192, 16, 255, 255))
    for i in range(16):
        offset = SCALES + i * 46
        struct.pack_into("<H", song, offset, 0xFFF)
        song[offset + 26:offset + 42] = b"CHROMATIC".ljust(16, b"\xff")
    for i in range(132):
        for band, (kind, hz) in enumerate(((1, 100), (2, 1000), (4, 5000))):
            struct.pack_into("<BHbBB", song, EQ + i * 18 + band * 6, kind, hz, 0, 0, 50)
    anchor = spec["role"] == "anchor"
    sample_count = spec["tracks"].count("SAMPLER")
    samples, instruments = [], []
    sample_index = 0
    for track, engine in enumerate(spec["tracks"]):
        variation = spec["variation"] + track
        filtered = track in spec.get("filter_tracks", [])
        sends = tuple(level if track in spec.get(feature + "_tracks", []) else 0
                      for feature, level in (("mfx", 48), ("delay", 64), ("reverb", 48)))
        sample_path = ""
        if engine == "SAMPLER":
            path = f"Samples/S{sample_index:02d}.WAV"
            sample_path = "/" + path
            wav, sample_meta = sample_wav(seed ^ (spec["variation"] << 16) ^ track,
                                         spec["sample_bytes_target"] // sample_count, variation)
            samples.append((path, wav, sample_meta))
            sample_index += 1
        variants = spec.get("instrument_variants", [1] * len(spec["tracks"]))[track]
        numbers = [track] + [len(spec["tracks"]) + sum(n - 1 for n in
                   spec.get("instrument_variants", [])[:track]) + v for v in range(variants - 1)]
        for variant, number in enumerate(numbers):
            record = _instrument(engine, number, variation + variant * 5, sample_path, filtered, sends,
                                 spec["name"] == "simple")
            start = INSTRUMENTS + number * INSTRUMENT_BYTES
            song[start:start + INSTRUMENT_BYTES] = record
            instruments.append({"id": number, "track": track, "variant": variant,
                                "engine": engine, "filter": filtered,
                                "sends": dict(zip(("mfx", "delay", "reverb"), sends)),
                                "parameters_sha256": hashlib.sha256(record).hexdigest()})
        song[SONG + track] = track
        song[CHAINS + track * 32] = track
        # One 16-row phrase loops on the empty Song cell. Row F releases.
        stride = 4 if track % 3 == 0 else 2
        for row in range(0, 15, stride):
            note = 36 if engine == "SAMPLER" else (48, 52, 55, 60)[(row // stride + track) % 4]
            if spec["name"] == "simple":
                note = 69
            start = PHRASES + track * 144 + row * 9
            number = numbers[(row // stride) % variants]
            song[start:start + 9] = bytes((note, 48 if anchor else 32, number,
                                         255, 0, 255, 0, 255, 0))
        start = PHRASES + track * 144 + 15 * 9
        song[start:start + 9] = bytes((128, 255, 255, 255, 0, 255, 0, 255, 0))
    for event in spec.get("phrase_controls", []):
        start = PHRASES + event["track"] * 144 + event["row"] * 9
        for effect in event["effects"]:
            position = start + 3 + effect["slot"] * 2
            song[position:position + 2] = bytes((effect["code"], effect["value"]))
    instruments.sort(key=lambda instrument: instrument["id"])
    files = [("Songs/DEMO1.M8S", bytes(song))] + [(p, b) for p, b, _ in samples]
    return files, {"song_version": "6.0.1", "track_count": len(spec["tracks"]),
                   "instrument_count": len(instruments), "instruments": instruments,
                   "sample_bytes": sum(len(b) for _, b, _ in samples),
                   "samples": [{"path": p, **meta} for p, _, meta in samples]}


def _name83(name):
    base, _, ext = name.upper().partition(".")
    allowed = "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_-"
    if not 1 <= len(base) <= 8 or len(ext) > 3 or any(c not in allowed for c in base + ext):
        raise ValueError(f"Invalid 8.3 name: {name}")
    return (base.ljust(8) + ext.ljust(3)).encode("ascii")


def _entry(name, attr, cluster=0, size=0):
    row = bytearray(32)
    row[:11] = name
    row[11] = attr
    # Fixed timestamp: 2000-01-01 00:00:00.
    for offset in (16, 18, 24):
        struct.pack_into("<H", row, offset, 0x2821)
    struct.pack_into("<HI", row, 26, cluster, size)
    return row


def fat_image(files):
    """Return an MBR-partitioned FAT16 image with deterministic allocation."""
    size_mib = next((n for n in (16, 32, 64, 128)
                     if sum(len(b) for _, b in files) + 1024 * 1024 < n * 1024 * 1024), None)
    if size_mib is None:
        raise ValueError("Generated files exceed the 128 MiB card limit")
    sectors, reserved, root_sectors, spc = size_mib * 2048, 8, 32, 4
    partition, fat_sectors = sectors - 64, 1
    for _ in range(32):
        clusters = (partition - reserved - 2 * fat_sectors - root_sectors) // spc
        need = ((clusters + 2) * 2 + 511) // 512
        if need == fat_sectors:
            break
        fat_sectors = need
    if not 4085 <= clusters < 65525:
        raise ValueError("Generated card does not have FAT16 geometry")
    image = bytearray(sectors * 512)
    image[0x1BE:0x1CE] = struct.pack("<B3sB3sII", 0, bytes(3), 6, bytes(3), 64, partition)
    image[510:512] = b"\x55\xaa"
    vbr = bytearray(512)
    vbr[:11] = b"\xeb\x3c\x90MSDOS5.0"
    struct.pack_into("<HBHBHHBHHHII", vbr, 11, 512, spc, reserved, 2, 512,
                     partition if partition <= 65535 else 0, 0xF8, fat_sectors, 32, 64, 64,
                     partition if partition > 65535 else 0)
    vbr[36:39] = bytes((128, 0, 41))
    struct.pack_into("<I", vbr, 39, 0x4D385047)
    vbr[43:62] = b"M8TRAINING FAT16   "
    vbr[510:512] = b"\x55\xaa"
    image[64 * 512:65 * 512] = vbr
    fat_start, root_start = 64 + reserved, 64 + reserved + 2 * fat_sectors
    data_start = root_start + root_sectors
    directories = sorted({p.split("/")[0] for p, _ in files})
    if any(p.count("/") != 1 for p, _ in files):
        raise ValueError("Only one directory level is supported")
    allocations, next_cluster = {}, 2
    for path, length in [(d, 2048) for d in directories] + [(p, len(b)) for p, b in files]:
        count = (length + 2047) // 2048
        allocations[path] = list(range(next_cluster, next_cluster + count))
        next_cluster += count
    if next_cluster > clusters + 2:
        raise ValueError("Generated files exceed the available FAT clusters")
    fat = bytearray(fat_sectors * 512)
    struct.pack_into("<HH", fat, 0, 0xFFF8, 0xFFFF)
    for chain in allocations.values():
        for i, cluster in enumerate(chain):
            struct.pack_into("<H", fat, cluster * 2, chain[i + 1] if i + 1 < len(chain) else 0xFFFF)
    for i in range(2):
        offset = (fat_start + i * fat_sectors) * 512
        image[offset:offset + len(fat)] = fat
    root = bytearray(root_sectors * 512)
    root[:32] = _entry(b"M8TRAINING ", 8)
    for i, directory in enumerate(directories, 1):
        root[i * 32:(i + 1) * 32] = _entry(_name83(directory), 16, allocations[directory][0])
    image[root_start * 512:root_start * 512 + len(root)] = root
    records = []
    for directory in directories:
        entries = bytearray(2048)
        entries[:32] = _entry(b".          ", 16, allocations[directory][0])
        entries[32:64] = _entry(b"..         ", 16)
        children = [(p, b) for p, b in files if p.startswith(directory + "/")]
        if len(children) > 62:
            raise ValueError("Generated directory exceeds one cluster")
        for i, (path, data) in enumerate(children, 2):
            chain = allocations[path]
            entries[i * 32:(i + 1) * 32] = _entry(_name83(path.split("/")[1]), 32,
                                                 chain[0] if chain else 0, len(data))
        offset = (data_start + (allocations[directory][0] - 2) * spc) * 512
        image[offset:offset + len(entries)] = entries
    for path, data in files:
        chain = allocations[path]
        offset = (data_start + (chain[0] - 2) * spc) * 512 if chain else 0
        image[offset:offset + len(data)] = data
        records.append({"path": path, "bytes": len(data), "sha256": hashlib.sha256(data).hexdigest()})
    return image, {"image_mib": size_mib, "files": records}


def _sparse_write(path, data):
    with path.open("wb") as output:
        for start in range(0, len(data), 4096):
            block = data[start:start + 4096]
            if any(block):
                output.seek(start)
                output.write(block)
        output.truncate(len(data))


def calibration_report(rows):
    representative = [r for r in rows if r["role"] == "representative"]
    variants = [r.get("instrument_variants", [1] * len(r["tracks"])) for r in representative]
    instruments = [sum(v) for v in variants]
    active_rows = sum(len(range(0, 15, 4 if track % 3 == 0 else 2)) + 1
                      for r in representative for track in range(len(r["tracks"])))
    fx_histogram = collections.Counter(len(event["effects"]) for r in representative
                                      for event in r.get("phrase_controls", []))
    if active_rows:
        fx_histogram[0] = active_rows - sum(fx_histogram.values())
    fx_counts = collections.Counter(effect["command"] for r in representative
                                   for event in r.get("phrase_controls", []) for effect in event["effects"])
    return {"projects": len(representative),
            "track_count": dict(sorted(collections.Counter(len(r["tracks"]) for r in representative).items())),
            "instrument_count": {"total": sum(instruments),
                                 "mean": sum(instruments) / len(instruments) if instruments else 0.0,
                                 "minimum": min(instruments, default=0), "maximum": max(instruments, default=0),
                                 "histogram": dict(sorted(collections.Counter(instruments).items()))},
            "engines": dict(sorted(collections.Counter(e for r, v in zip(representative, variants)
                            for e, n in zip(r["tracks"], v) for _ in range(n)).items())),
            "sampler_projects": sum("SAMPLER" in r["tracks"] for r in representative),
            "features": {feature: {"projects": sum(bool(r[feature + "_tracks"]) for r in representative),
                                   "instruments": sum(sum(v[t] for t in r[feature + "_tracks"])
                                                      for r, v in zip(representative, variants))}
                         for feature in ("filter", "mfx", "delay", "reverb")},
            "phrase_fx": {"active_rows": active_rows, "commands": dict(sorted(fx_counts.items())),
                          "fx_slots": sum(fx_counts.values()),
                          "slots_per_active_row": sum(fx_counts.values()) / active_rows if active_rows else 0.0,
                          "active_row_fx_count_histogram": dict(sorted(fx_histogram.items()))},
            "tempo_bpm": {"minimum": min(r["tempo_bpm"] for r in representative),
                          "maximum": max(r["tempo_bpm"] for r in representative)} if representative else {}}


def generate(output, seed=DEFAULT_SEED, selected=None):
    output = Path(output)
    output.mkdir(parents=True, exist_ok=True)
    all_rows = specifications(seed)
    names = {r["name"] for r in all_rows}
    if selected is not None and (unknown := set(selected) - names):
        raise ValueError(f"Unknown training fixtures: {', '.join(sorted(unknown))}")
    rows = [r for r in all_rows if selected is None or r["name"] in selected]
    fixtures = []
    for spec in rows:
        files, musical = build_project(spec, seed)
        image, card = fat_image(files)
        name = spec["name"]
        _sparse_write(output / (name + ".img"), image)
        row = {**spec, **musical, **card, "image": name + ".img",
               "image_sha256": hashlib.sha256(image).hexdigest()}
        (output / (name + ".json")).write_text(json.dumps(row, sort_keys=True, indent=2) + "\n")
        fixtures.append(row)
        print(f"{name}: {len(image) // (1024 * 1024)} MiB, {row['image_sha256']}")
    manifest = {"schema": "m8emu-synthetic-training/1", "seed": seed,
                "generator_sha256": hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),
                "calibration_sha256": hashlib.sha256(Path(__file__).with_name("calibration.json").read_bytes()).hexdigest(),
                "calibration": calibration_report(rows), "fixtures": fixtures,
                "limits": ["These cards contain original generated content only.",
                           "Training anchors are not the protected benchmark fixtures.",
                           "Matching metadata does not establish matching instruction profiles or speed.",
                           "GEN/USE identity and corpus performance need runtime qualification."]}
    (output / "manifest.json").write_text(json.dumps(manifest, sort_keys=True, indent=2) + "\n")
    return manifest


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", required=True, type=Path)
    parser.add_argument("--seed", type=lambda text: int(text, 0), default=DEFAULT_SEED)
    parser.add_argument("--fixtures", nargs="+", choices=[r["name"] for r in specifications()])
    args = parser.parse_args()
    generate(args.output, args.seed, args.fixtures)


if __name__ == "__main__":
    main()
