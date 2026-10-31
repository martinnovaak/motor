"""Quantize a trained model to Motor's native nnue.bin layout."""

from __future__ import annotations

from pathlib import Path

import numpy as np
import torch

from .architecture import HIDDEN_SIZE, QA, QB
from .model import MotorNnue


def _quantize(value: torch.Tensor, scale: int) -> np.ndarray:
    return (
        value.detach().float().cpu().mul(scale).round()
        .clamp(-32768, 32767).to(torch.int16).numpy().astype("<i2", copy=False)
    )


def export_nnue(model: MotorNnue, path: str | Path) -> None:
    if model.hidden_size != HIDDEN_SIZE:
        raise ValueError(f"Motor expects hidden size {HIDDEN_SIZE}")
    destination = Path(path)
    with destination.open("wb") as stream:
        # feature_weight is already flattened in C++ [bucket][color][piece][square] order.
        _quantize(model.feature_weight, QA).tofile(stream)
        _quantize(model.feature_bias, QA).tofile(stream)
        _quantize(model.output_weight[0], QB).tofile(stream)
        _quantize(model.output_weight[1], QB).tofile(stream)
        _quantize(model.output_bias, QA * QB).tofile(stream)
