"""Motor dataset reader, block shuffler, and NNUE feature decoder."""

from __future__ import annotations

from dataclasses import dataclass
from pathlib import Path
import queue
import random
import struct
import threading
from typing import Iterable, Iterator, Sequence

import numpy as np
import torch

from .architecture import (
    FEATURE_COUNT,
    HORIZONTAL_MIRRORING,
    KING_BUCKET_COUNT,
    KING_BUCKETS,
    MAX_ACTIVE_FEATURES,
    feature_index,
)

MAGIC = b"MOTORDS\0"
VERSION = 1
HEADER_SIZE = 64
RECORD_SIZE = 32
INCOMPLETE = 1

HEADER = struct.Struct("<8sHHHHQQQQQ8s")
RECORD_DTYPE = np.dtype(
    [
        ("occupancy", "<u8"),
        ("pieces", "u1", (15,)),
        ("result", "u1"),
        ("best_move", "<u2"),
        ("score", "<i2"),
        ("king_stm", "u1"),
        ("king_nstm", "u1"),
        ("extra", "u1", (2,)),
    ],
    align=False,
)
assert HEADER.size == HEADER_SIZE
assert RECORD_DTYPE.itemsize == RECORD_SIZE


@dataclass(frozen=True)
class DatasetHeader:
    version: int
    flags: int
    record_count: int
    game_count: int
    seed: int
    nodes_per_move: int
    created_unix_seconds: int


class DatasetFile:
    """Memory-mapped Motor dataset with a validated header."""

    def __init__(self, path: str | Path):
        self.path = Path(path)
        size = self.path.stat().st_size
        if size < HEADER_SIZE or (size - HEADER_SIZE) % RECORD_SIZE:
            raise ValueError(f"truncated Motor dataset: {self.path}")

        with self.path.open("rb") as stream:
            values = HEADER.unpack(stream.read(HEADER_SIZE))
        (magic, version, header_size, record_size, flags, record_count,
         game_count, seed, nodes_per_move, created, _reserved) = values
        if (magic != MAGIC or version != VERSION or header_size != HEADER_SIZE
                or record_size != RECORD_SIZE):
            raise ValueError(f"unsupported Motor dataset: {self.path}")

        records_in_file = (size - HEADER_SIZE) // RECORD_SIZE
        if flags & INCOMPLETE:
            record_count = records_in_file
        elif record_count != records_in_file:
            raise ValueError(f"invalid record count in {self.path}")

        self.header = DatasetHeader(
            version, flags, record_count, game_count, seed,
            nodes_per_move, created,
        )
        self.records = np.memmap(
            self.path,
            mode="r",
            dtype=RECORD_DTYPE,
            offset=HEADER_SIZE,
            shape=(record_count,),
        )

    def __len__(self) -> int:
        return self.header.record_count


@dataclass(frozen=True)
class FeatureBatch:
    stm: torch.Tensor
    nstm: torch.Tensor
    offsets: torch.Tensor
    score: torch.Tensor
    result: torch.Tensor

    @property
    def size(self) -> int:
        return self.score.shape[0]

    def to(self, device: torch.device, non_blocking: bool = True) -> "FeatureBatch":
        return FeatureBatch(
            self.stm.to(device, non_blocking=non_blocking),
            self.nstm.to(device, non_blocking=non_blocking),
            self.offsets.to(device, non_blocking=non_blocking),
            self.score.to(device, non_blocking=non_blocking),
            self.result.to(device, non_blocking=non_blocking),
        )

    def pin_memory(self) -> "FeatureBatch":
        return FeatureBatch(
            self.stm.pin_memory(),
            self.nstm.pin_memory(),
            self.offsets.pin_memory(),
            self.score.pin_memory(),
            self.result.pin_memory(),
        )

    def record_stream(self, stream: torch.cuda.Stream) -> None:
        for tensor in (self.stm, self.nstm, self.offsets, self.score, self.result):
            tensor.record_stream(stream)


def _piece_codes(records: np.ndarray) -> np.ndarray:
    packed = records["pieces"]
    codes = np.empty((len(records), 30), dtype=np.uint8)
    codes[:, 0::2] = packed & 0x0f
    codes[:, 1::2] = packed >> 4
    return codes


def record_to_fen(record: np.void) -> str:
    """Reconstruct a complete canonical FEN for relabeling or debugging."""
    squares = [""] * 64
    king_stm = int(record["king_stm"])
    king_nstm = int(record["king_nstm"])
    occupancy = int(record["occupancy"])
    if (king_stm >= 64 or king_nstm >= 64 or king_stm == king_nstm
            or occupancy & ((1 << king_stm) | (1 << king_nstm))):
        raise ValueError("invalid king squares in position record")
    squares[king_stm] = "K"
    squares[king_nstm] = "k"

    piece_chars = ("P", "N", "B", "R", "Q", "", "", "",
                   "p", "n", "b", "r", "q", "", "", "")
    piece_index = 0
    while occupancy:
        if piece_index >= 30:
            raise ValueError("too many pieces in position record")
        square = (occupancy & -occupancy).bit_length() - 1
        occupancy &= occupancy - 1
        packed = int(record["pieces"][piece_index // 2])
        code = (packed >> (4 * (piece_index & 1))) & 0x0f
        if not piece_chars[code]:
            raise ValueError("invalid piece code in position record")
        squares[square] = piece_chars[code]
        piece_index += 1

    ranks = []
    for rank in range(7, -1, -1):
        output = ""
        empty = 0
        for square in squares[rank * 8:(rank + 1) * 8]:
            if not square:
                empty += 1
            else:
                if empty:
                    output += str(empty)
                    empty = 0
                output += square
        if empty:
            output += str(empty)
        ranks.append(output)

    state = int(record["extra"][0])
    rights = state & 0x0f
    castling = "".join(
        symbol for bit, symbol in ((1, "K"), (2, "Q"), (4, "k"), (8, "q"))
        if rights & bit
    ) or "-"
    encoded_ep = state >> 4
    if encoded_ep > 8:
        raise ValueError("invalid en-passant file in position record")
    en_passant = "-" if encoded_ep == 0 else f"{chr(ord('a') + encoded_ep - 1)}6"
    return (f"{'/'.join(ranks)} w {castling} {en_passant} "
            f"{int(record['extra'][1])} 1")


def decode_features(records: np.ndarray) -> FeatureBatch:
    """Decode canonical records to the two sparse HalfKAv2-style views."""
    batch_size = len(records)
    if batch_size == 0:
        raise ValueError("cannot decode an empty batch")
    if np.any(records["result"] > 2):
        raise ValueError("training dataset contains positions with unknown results")

    occupancy = np.ascontiguousarray(records["occupancy"])
    occupancy_bytes = occupancy.view(np.uint8).reshape(batch_size, 8)
    occupied = np.unpackbits(occupancy_bytes, axis=1, bitorder="little").astype(bool)
    rows, occupied_squares = np.nonzero(occupied)
    counts = occupied.sum(axis=1, dtype=np.int64)
    if np.any(counts > 30):
        raise ValueError("record contains more than 30 non-king pieces")

    starts = np.cumsum(counts, dtype=np.int64) - counts
    slots = np.arange(len(rows), dtype=np.int64) - starts[rows]
    squares = np.empty((batch_size, MAX_ACTIVE_FEATURES), dtype=np.int64)
    codes = np.empty((batch_size, MAX_ACTIVE_FEATURES), dtype=np.int64)
    squares[rows, slots] = occupied_squares
    codes[rows, slots] = _piece_codes(records)[rows, slots]

    record_rows = np.arange(batch_size, dtype=np.int64)
    squares[record_rows, counts] = records["king_stm"]
    codes[record_rows, counts] = 5
    squares[record_rows, counts + 1] = records["king_nstm"]
    codes[record_rows, counts + 1] = 13
    lengths = counts + 2
    valid = np.arange(MAX_ACTIVE_FEATURES)[None, :] < lengths[:, None]

    flat_rows = np.repeat(record_rows, lengths)
    flat_squares = squares[valid]
    flat_codes = codes[valid]
    if np.any((flat_codes & 7) > 5):
        raise ValueError("record contains an invalid piece code")

    stm_king = records["king_stm"].astype(np.int64)
    nstm_king = records["king_nstm"].astype(np.int64)
    if np.any(stm_king >= 64) or np.any(nstm_king >= 64):
        raise ValueError("record contains an invalid king square")
    stm_bucket = KING_BUCKETS[stm_king] % KING_BUCKET_COUNT
    stm_square = flat_squares.copy()
    if HORIZONTAL_MIRRORING:
        stm_mirror = (stm_king[flat_rows] & 7) > 3
        stm_square[stm_mirror] ^= 7
    stm = feature_index(
        stm_bucket[flat_rows], flat_codes >> 3, flat_codes & 7, stm_square,
    )

    nstm_bucket = KING_BUCKETS[nstm_king ^ 56] % KING_BUCKET_COUNT
    nstm_square = flat_squares.copy()
    if HORIZONTAL_MIRRORING:
        nstm_mirror = (nstm_king[flat_rows] & 7) > 3
        nstm_square[nstm_mirror] ^= 7
    nstm_square ^= 56
    nstm = feature_index(
        nstm_bucket[flat_rows], (flat_codes >> 3) ^ 1,
        flat_codes & 7, nstm_square,
    )

    if np.any(stm >= FEATURE_COUNT) or np.any(nstm >= FEATURE_COUNT):
        raise ValueError("decoded feature outside the network input")

    offsets = np.empty(batch_size + 1, dtype=np.int64)
    offsets[0] = 0
    np.cumsum(lengths, out=offsets[1:])
    return FeatureBatch(
        stm=torch.from_numpy(stm.astype(np.int32)),
        nstm=torch.from_numpy(nstm.astype(np.int32)),
        offsets=torch.from_numpy(offsets.astype(np.int32)),
        score=torch.from_numpy(records["score"].astype(np.float32)),
        result=torch.from_numpy(records["result"].astype(np.int64)),
    )


class BlockShuffleLoader:
    """Bounded-memory shuffle over one or more fixed-record datasets."""

    def __init__(self, paths: Sequence[str | Path], batch_size: int,
                 block_records: int = 1 << 20, seed: int = 1,
                 drop_last: bool = True):
        if not paths:
            raise ValueError("at least one dataset is required")
        if batch_size <= 0 or block_records < batch_size:
            raise ValueError("block_records must be at least batch_size")
        self.files = [DatasetFile(path) for path in paths]
        self.batch_size = batch_size
        self.block_records = block_records
        self.seed = seed
        self.drop_last = drop_last
        self.epoch = 0

    @property
    def record_count(self) -> int:
        return sum(len(file) for file in self.files)

    def __iter__(self) -> Iterator[FeatureBatch]:
        rng = np.random.default_rng(self.seed + self.epoch)
        block_rng = random.Random(self.seed + self.epoch)
        self.epoch += 1
        blocks: list[tuple[int, int, int]] = []
        for file_index, file in enumerate(self.files):
            for start in range(0, len(file), self.block_records):
                blocks.append((file_index, start, min(start + self.block_records, len(file))))
        block_rng.shuffle(blocks)

        carry = np.empty(0, dtype=RECORD_DTYPE)
        for file_index, start, end in blocks:
            records = np.array(self.files[file_index].records[start:end], copy=True)
            rng.shuffle(records)
            if len(carry):
                records = np.concatenate((carry, records))
            complete = len(records) // self.batch_size * self.batch_size
            for offset in range(0, complete, self.batch_size):
                yield decode_features(records[offset:offset + self.batch_size])
            carry = records[complete:]

        if len(carry) and not self.drop_last:
            yield decode_features(carry)


class PrefetchLoader:
    """Decode upcoming batches concurrently with GPU training."""

    def __init__(self, source: Iterable[FeatureBatch], depth: int = 2,
                 pin_memory: bool = False):
        if depth <= 0:
            raise ValueError("prefetch depth must be positive")
        self.source = source
        self.depth = depth
        self.should_pin = pin_memory

    def __iter__(self) -> Iterator[FeatureBatch]:
        batches: queue.Queue[FeatureBatch | BaseException | None] = queue.Queue(self.depth)

        def produce() -> None:
            try:
                for batch in self.source:
                    batches.put(batch.pin_memory() if self.should_pin else batch)
            except BaseException as error:
                batches.put(error)
            finally:
                batches.put(None)

        threading.Thread(target=produce, name="motor-dataset", daemon=True).start()
        while True:
            item = batches.get()
            if item is None:
                return
            if isinstance(item, BaseException):
                raise item
            yield item
