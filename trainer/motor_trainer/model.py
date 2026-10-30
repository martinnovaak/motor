"""Current Motor king-bucketed perspective NNUE."""

from __future__ import annotations

import math

import torch
from torch import nn
from torch.nn import functional as F

from .architecture import FEATURE_COUNT, HIDDEN_SIZE
from .dataset import FeatureBatch


class MotorNnue(nn.Module):
    def __init__(self, hidden_size: int = HIDDEN_SIZE):
        super().__init__()
        self.hidden_size = hidden_size
        self.feature_weight = nn.Parameter(torch.empty(FEATURE_COUNT, hidden_size))
        self.feature_bias = nn.Parameter(torch.zeros(hidden_size))
        self.output_weight = nn.Parameter(torch.empty(2, hidden_size))
        self.output_bias = nn.Parameter(torch.zeros(()))
        self.reset_parameters()

    def reset_parameters(self) -> None:
        nn.init.normal_(self.feature_weight, std=0.01)
        nn.init.uniform_(
            self.output_weight,
            -1.0 / math.sqrt(self.hidden_size),
            1.0 / math.sqrt(self.hidden_size),
        )

    def _accumulate(self, features: torch.Tensor,
                    offsets: torch.Tensor) -> torch.Tensor:
        return F.embedding_bag(
            features,
            self.feature_weight,
            offsets,
            mode="sum",
            include_last_offset=True,
        ) + self.feature_bias

    @staticmethod
    def _activate(accumulator: torch.Tensor) -> torch.Tensor:
        return accumulator.clamp(0.0, 1.0).square()

    def forward(self, batch: FeatureBatch) -> torch.Tensor:
        stm = self._activate(self._accumulate(batch.stm, batch.offsets))
        nstm = self._activate(self._accumulate(batch.nstm, batch.offsets))
        return (
            F.linear(stm, self.output_weight[0:1]).squeeze(1)
            + F.linear(nstm, self.output_weight[1:2]).squeeze(1)
            + self.output_bias
        )

    @torch.no_grad()
    def clamp_for_quantization(self) -> None:
        self.feature_weight.clamp_(-1.0, 1.0)
        self.feature_bias.clamp_(-1.0, 1.0)
        self.output_weight.clamp_(-1.0, 1.0)
        self.output_bias.clamp_(-1.0, 1.0)
