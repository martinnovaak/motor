"""The NNUE layout shared conceptually with evaluation/nnue.hpp."""

from __future__ import annotations

import numpy as np

HIDDEN_SIZE = 16
QA = 403
QB = 81
KING_BUCKET_COUNT = 1
HORIZONTAL_MIRRORING = False
COLOR_COUNT = 2
PIECE_COUNT = 6
SQUARE_COUNT = 64
FEATURE_COUNT = KING_BUCKET_COUNT * COLOR_COUNT * PIECE_COUNT * SQUARE_COUNT
MAX_ACTIVE_FEATURES = 32

KING_BUCKETS = np.zeros(64, dtype=np.int64)


def feature_index(bucket: np.ndarray, color: np.ndarray,
                  piece: np.ndarray, square: np.ndarray) -> np.ndarray:
    """Flatten [bucket][color][piece][square] exactly like C++ Weights."""
    return (((bucket * COLOR_COUNT + color) * PIECE_COUNT + piece)
            * SQUARE_COUNT + square)
