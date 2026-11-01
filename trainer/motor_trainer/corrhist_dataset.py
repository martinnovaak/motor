"""Reader for Motor's 32-byte material-phase corrhist traces."""

from __future__ import annotations

from dataclasses import dataclass
from pathlib import Path
import struct

import numpy as np


MAGIC = b"MCRHIST\0"
VERSION = 2
HEADER_SIZE = 64
RECORD_SIZE = 32
HEADER = struct.Struct("<8sHHHHQI36s")

BOUND_EXACT = 1
FLAG_PV = 1 << 0
FLAG_ROOT = 1 << 1
FLAG_MAIN_THREAD = 1 << 2
FLAG_QUIET_BEST_MOVE = 1 << 3
FLAG_BLACK_TO_MOVE = 1 << 4

RECORD_DTYPE = np.dtype([
    ("raw_eval", "<i2"),
    ("search_score", "<i2"),
    ("corrhist", "<i2", (7,)),
    ("material_phase", "u1"),
    ("depth", "u1"),
    ("search_id", "<u4"),
    ("game_id", "<u4"),
    ("bound", "u1"),
    ("flags", "u1"),
    ("reserved", "<u2"),
])

assert HEADER.size == HEADER_SIZE
assert RECORD_DTYPE.itemsize == RECORD_SIZE


@dataclass(frozen=True)
class CorrhistTraceHeader:
    record_count: int
    sample_rate: int


class CorrhistTraceFile:
    def __init__(self, path: str | Path):
        self.path = Path(path)
        size = self.path.stat().st_size
        if size < HEADER_SIZE or (size - HEADER_SIZE) % RECORD_SIZE:
            raise ValueError(f"truncated corrhist trace: {self.path}")

        with self.path.open("rb") as stream:
            values = HEADER.unpack(stream.read(HEADER_SIZE))
        (magic, version, header_size, record_size, flags, record_count,
         sample_rate, _reserved) = values
        physical_count = (size - HEADER_SIZE) // RECORD_SIZE
        if (magic != MAGIC or version != VERSION or header_size != HEADER_SIZE
                or record_size != RECORD_SIZE or flags != 0
                or record_count != physical_count or sample_rate == 0):
            raise ValueError(f"invalid corrhist trace: {self.path}")

        self.header = CorrhistTraceHeader(record_count, sample_rate)
        self.records = np.memmap(
            self.path,
            mode="r",
            dtype=RECORD_DTYPE,
            offset=HEADER_SIZE,
            shape=(record_count,),
        )

    def __len__(self) -> int:
        return self.header.record_count
