"""The NNUE layout shared conceptually with evaluation/nnue.hpp."""

from __future__ import annotations

import numpy as np

HIDDEN_SIZE = 1536
QA = 403
QB = 81
KING_BUCKET_COUNT = 8
COLOR_COUNT = 2
PIECE_COUNT = 6
SQUARE_COUNT = 64
FEATURE_COUNT = KING_BUCKET_COUNT * COLOR_COUNT * PIECE_COUNT * SQUARE_COUNT
MAX_ACTIVE_FEATURES = 32

# Values 8..15 encode horizontally mirrored versions of buckets 0..7. The
# engine applies the mirror separately and selects the physical bucket with % 8.
KING_BUCKETS = np.asarray(
    [
        0, 1, 2, 3, 11, 10, 9, 8,
        4, 4, 5, 5, 13, 13, 12, 12,
        6, 6, 6, 6, 14, 14, 14, 14,
        6, 6, 6, 6, 14, 14, 14, 14,
        7, 7, 7, 7, 15, 15, 15, 15,
        7, 7, 7, 7, 15, 15, 15, 15,
        7, 7, 7, 7, 15, 15, 15, 15,
        7, 7, 7, 7, 15, 15, 15, 15,
    ],
    dtype=np.int64,
)


def feature_index(bucket: np.ndarray, color: np.ndarray,
                  piece: np.ndarray, square: np.ndarray) -> np.ndarray:
    """Flatten [bucket][color][piece][square] exactly like C++ Weights."""
    return (((bucket * COLOR_COUNT + color) * PIECE_COUNT + piece)
            * SQUARE_COUNT + square)
