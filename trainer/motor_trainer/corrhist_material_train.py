"""Compare constant and material-phase correction-history mixer weights."""

from __future__ import annotations

import argparse
import json
from pathlib import Path

import numpy as np

from .corrhist_dataset import (
    BOUND_EXACT,
    FLAG_QUIET_BEST_MOVE,
    CorrhistTraceFile,
)


BASELINE_NUMERATORS = np.array(
    [200, 100, 200, 150, 120, 180, 180], dtype=np.float64,
)
BASELINE_WEIGHTS = BASELINE_NUMERATORS / 300.0
COMPONENT_NAMES = (
    "pawn", "threat", "nonpawn", "minor", "major",
    "continuation1", "continuation2",
)


def huber_values(error: np.ndarray, delta: float) -> np.ndarray:
    absolute = np.abs(error)
    return np.where(
        absolute <= delta,
        0.5 * error * error,
        delta * (absolute - 0.5 * delta),
    )


def split_bucket(game_id: np.ndarray, seed: int) -> np.ndarray:
    value = game_id.astype(np.uint64) ^ np.uint64(seed)
    with np.errstate(over="ignore"):
        value ^= value >> np.uint64(30)
        value *= np.uint64(0xbf58476d1ce4e5b9)
        value ^= value >> np.uint64(27)
        value *= np.uint64(0x94d049bb133111eb)
        value ^= value >> np.uint64(31)
    return value % np.uint64(10)


def load_samples(paths: list[str], min_depth: int, max_score: int,
                 split_seed: int) -> tuple[np.ndarray, ...]:
    features: list[np.ndarray] = []
    targets: list[np.ndarray] = []
    phases: list[np.ndarray] = []
    depths: list[np.ndarray] = []
    validation: list[np.ndarray] = []
    total_records = 0

    for path in paths:
        trace = CorrhistTraceFile(path)
        records = trace.records
        total_records += len(trace)
        mask = (
            (records["bound"] == BOUND_EXACT)
            & ((records["flags"] & FLAG_QUIET_BEST_MOVE) != 0)
            & (records["depth"] >= min_depth)
            & (records["material_phase"] <= 24)
            & (np.abs(records["raw_eval"].astype(np.int32)) <= max_score)
            & (np.abs(records["search_score"].astype(np.int32)) <= max_score)
            & (records["game_id"] != 0)
        )
        selected = records[mask]
        if len(selected) == 0:
            continue
        features.append(selected["corrhist"].astype(np.float64) / 256.0)
        targets.append(
            selected["search_score"].astype(np.float64)
            - selected["raw_eval"].astype(np.float64)
        )
        phases.append(selected["material_phase"].astype(np.float64))
        depths.append(selected["depth"].astype(np.float64))
        validation.append(split_bucket(selected["game_id"], split_seed) >= 8)

    if not features:
        raise ValueError("no usable corrhist samples")
    x = np.concatenate(features)
    y = np.concatenate(targets)
    phase = np.concatenate(phases)
    depth = np.concatenate(depths)
    is_validation = np.concatenate(validation)
    print(
        f"records={total_records:,} usable={len(y):,} "
        f"train={(~is_validation).sum():,} validation={is_validation.sum():,}"
    )
    return x, y, phase, depth, is_validation


def fit_irls(design: np.ndarray, target: np.ndarray,
             confidence: np.ndarray, prior: np.ndarray,
             delta: float, ridge: float, phase_ridge: float,
             iterations: int) -> np.ndarray:
    weights = prior.copy()
    normal_prior = ridge * np.eye(len(weights), dtype=np.float64)
    right_prior = ridge * prior
    if len(weights) == 14 and phase_ridge > 0:
        difference = np.concatenate((
            np.eye(7, dtype=np.float64),
            -np.eye(7, dtype=np.float64),
        ), axis=1)
        normal_prior += phase_ridge * difference.T @ difference

    for iteration in range(iterations):
        residual = target - design @ weights
        robust = np.ones_like(residual)
        outside = np.abs(residual) > delta
        robust[outside] = delta / np.abs(residual[outside])
        sample_weight = confidence * robust
        normal = design.T @ (sample_weight[:, None] * design) + normal_prior
        right = design.T @ (sample_weight * target) + right_prior
        weights = np.linalg.solve(normal, right)
        print(
            f"iteration={iteration + 1} weights="
            + ",".join(f"{value:.6f}" for value in weights)
        )
    return weights


def metrics(prediction: np.ndarray, target: np.ndarray,
            delta: float) -> dict[str, float]:
    error = prediction - target
    return {
        "mae": float(np.mean(np.abs(error))),
        "rmse": float(np.sqrt(np.mean(error * error))),
        "huber": float(np.mean(huber_values(error, delta))),
    }


def metric_report(x: np.ndarray, y: np.ndarray, phase: np.ndarray,
                  weights: np.ndarray, delta: float) -> dict[str, object]:
    if len(weights) == 7:
        prediction = x @ weights
    else:
        t = phase / 24.0
        prediction = np.sum(
            x * ((1.0 - t[:, None]) * weights[:7]
                 + t[:, None] * weights[7:]),
            axis=1,
        )
    report: dict[str, object] = {"all": metrics(prediction, y, delta)}
    for lower, upper in ((0, 6), (7, 12), (13, 18), (19, 24)):
        mask = (phase >= lower) & (phase <= upper)
        if np.any(mask):
            report[f"phase_{lower}_{upper}"] = {
                "samples": int(mask.sum()),
                **metrics(prediction[mask], y[mask], delta),
            }
    return report


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("traces", nargs="+")
    parser.add_argument("--output", required=True)
    parser.add_argument("--delta", type=float, default=50.0)
    parser.add_argument("--ridge", type=float, default=1e6)
    parser.add_argument("--phase-ridge", type=float, default=1e6)
    parser.add_argument("--iterations", type=int, default=8)
    parser.add_argument("--min-depth", type=int, default=3)
    parser.add_argument("--max-score", type=int, default=8000)
    parser.add_argument("--split-seed", type=int, default=1)
    parser.add_argument("--depth-power", type=float, default=3.0)
    parser.add_argument("--depth-cap", type=float, default=24.0)
    return parser.parse_args()


def main() -> None:
    args = parse_args()
    x, y, phase, depth, is_validation = load_samples(
        args.traces, args.min_depth, args.max_score, args.split_seed,
    )
    if not np.any(is_validation) or np.all(is_validation):
        raise ValueError("game split produced an empty train or validation set")

    train_x = x[~is_validation]
    train_y = y[~is_validation]
    train_phase = phase[~is_validation]
    train_depth = depth[~is_validation]
    if args.depth_power < 0 or args.depth_cap <= 0:
        raise ValueError("depth power must be non-negative and depth cap positive")
    if args.depth_power == 0:
        confidence = np.ones_like(train_depth)
    else:
        confidence = np.minimum(train_depth, args.depth_cap) ** args.depth_power
        confidence /= confidence.mean()
    constant = fit_irls(
        train_x, train_y, confidence, BASELINE_WEIGHTS,
        args.delta, args.ridge, 0.0, args.iterations,
    )

    train_t = train_phase / 24.0
    phase_design = np.concatenate((
        train_x * (1.0 - train_t[:, None]),
        train_x * train_t[:, None],
    ), axis=1)
    interpolated = fit_irls(
        phase_design, train_y, confidence, np.tile(BASELINE_WEIGHTS, 2),
        args.delta, args.ridge, args.phase_ridge, args.iterations,
    )

    constant_numerators = np.rint(constant * 300.0).astype(int)
    interpolated_numerators = np.rint(interpolated * 300.0).astype(int)
    constant_quantized = constant_numerators.astype(np.float64) / 300.0
    interpolated_quantized = interpolated_numerators.astype(np.float64) / 300.0
    report = {
        "traces": args.traces,
        "sample_count": int(len(y)),
        "train_count": int((~is_validation).sum()),
        "validation_count": int(is_validation.sum()),
        "delta": args.delta,
        "ridge": args.ridge,
        "phase_ridge": args.phase_ridge,
        "iterations": args.iterations,
        "depth_power": args.depth_power,
        "depth_cap": args.depth_cap,
        "depth_weight_normalization": "mean training confidence = 1",
        "baseline": {
            "numerators": BASELINE_NUMERATORS.astype(int).tolist(),
            "validation": metric_report(
                x[is_validation], y[is_validation], phase[is_validation],
                BASELINE_WEIGHTS, args.delta,
            ),
        },
        "constant": {
            "numerators": constant_numerators.tolist(),
            "train": metric_report(
                train_x, train_y, train_phase, constant_quantized, args.delta,
            ),
            "validation": metric_report(
                x[is_validation], y[is_validation], phase[is_validation],
                constant_quantized, args.delta,
            ),
        },
        "interpolated": {
            "endgame_numerators": interpolated_numerators[:7].tolist(),
            "middlegame_numerators": interpolated_numerators[7:].tolist(),
            "train": metric_report(
                train_x, train_y, train_phase,
                interpolated_quantized, args.delta,
            ),
            "validation": metric_report(
                x[is_validation], y[is_validation], phase[is_validation],
                interpolated_quantized, args.delta,
            ),
        },
    }
    destination = Path(args.output)
    destination.parent.mkdir(parents=True, exist_ok=True)
    destination.write_text(json.dumps(report, indent=2) + "\n")
    print(json.dumps(report["constant"]["validation"], indent=2))
    print(json.dumps(report["interpolated"], indent=2))
    print(f"report={destination}")


if __name__ == "__main__":
    main()
