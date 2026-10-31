"""Command-line training loop for Motor NNUE networks."""

from __future__ import annotations

import argparse
from contextlib import nullcontext
from pathlib import Path
import time

import torch

from .dataset import BlockShuffleLoader, FeatureBatch, PrefetchLoader
from .export import export_nnue
from .model import MotorNnue


def select_device(requested: str) -> torch.device:
    if requested != "auto":
        return torch.device(requested)
    if torch.cuda.is_available():
        return torch.device("cuda")
    if torch.backends.mps.is_available():
        return torch.device("mps")
    return torch.device("cpu")


def target_for(batch: FeatureBatch, score_scale: float,
               wdl_weight: float) -> torch.Tensor:
    evaluation = torch.sigmoid(batch.score / score_scale)
    result = batch.result.to(torch.float32) * 0.5
    return evaluation.lerp(result, wdl_weight)


def device_batches(batches: PrefetchLoader, device: torch.device):
    """Overlap pinned host-to-device copies with the previous CUDA step."""
    if device.type != "cuda":
        for batch in batches:
            yield batch.to(device, non_blocking=False)
        return

    iterator = iter(batches)
    copy_stream = torch.cuda.Stream(device=device)

    def load_next():
        try:
            cpu_batch = next(iterator)
        except StopIteration:
            return None
        with torch.cuda.stream(copy_stream):
            return cpu_batch.to(device, non_blocking=True)

    next_batch = load_next()
    while next_batch is not None:
        compute_stream = torch.cuda.current_stream(device)
        compute_stream.wait_stream(copy_stream)
        batch = next_batch
        batch.record_stream(compute_stream)
        next_batch = load_next()
        yield batch


def train(args: argparse.Namespace) -> None:
    device = select_device(args.device)
    torch.manual_seed(args.seed)
    if device.type == "cuda":
        torch.cuda.manual_seed_all(args.seed)
        torch.set_float32_matmul_precision("high")
    loader = BlockShuffleLoader(
        args.dataset,
        batch_size=args.batch_size,
        block_records=args.block_records,
        seed=args.seed,
    )
    model = MotorNnue().to(device)
    schedule_free = args.optimizer == "schedule-free-adamw"
    if schedule_free:
        try:
            from schedulefree import AdamWScheduleFree
        except ImportError as error:
            raise RuntimeError(
                "schedule-free-adamw requires the schedulefree package"
            ) from error
        optimizer = AdamWScheduleFree(
            model.parameters(),
            lr=args.learning_rate,
            weight_decay=args.weight_decay,
            warmup_steps=args.warmup_steps,
            foreach=False,
        )
    else:
        optimizer = torch.optim.AdamW(
            model.parameters(), lr=args.learning_rate,
            weight_decay=args.weight_decay,
        )
    first_epoch = 0
    steps = 0
    if args.resume:
        checkpoint = torch.load(args.resume, map_location=device, weights_only=True)
        model.load_state_dict(checkpoint["model"])
        optimizer.load_state_dict(checkpoint["optimizer"])
        first_epoch = checkpoint["epoch"] + 1
        steps = checkpoint["steps"]
        loader.epoch = first_epoch

    use_amp = device.type == "cuda" and not args.no_amp
    amp_dtype = torch.bfloat16 if args.amp_dtype == "bfloat16" else torch.float16
    scale_gradients = use_amp and amp_dtype == torch.float16
    try:
        scaler = torch.amp.GradScaler("cuda", enabled=scale_gradients)
    except (AttributeError, TypeError):  # PyTorch 2.2 compatibility
        scaler = torch.cuda.amp.GradScaler(enabled=scale_gradients)
    print(
        f"device={device} records={loader.record_count:,} "
        f"batch={args.batch_size:,} amp={use_amp} optimizer={args.optimizer}"
    )

    for epoch in range(first_epoch, args.epochs):
        model.train()
        if schedule_free:
            optimizer.train()
        started = time.perf_counter()
        positions = 0
        loss_total = torch.zeros((), device=device)
        batches = PrefetchLoader(
            loader, depth=args.prefetch, pin_memory=device.type == "cuda",
        )
        batch_count = 0
        for batch_index, batch in enumerate(device_batches(batches, device), start=1):
            batch_count = batch_index
            optimizer.zero_grad(set_to_none=True)
            amp = (
                torch.autocast(device_type="cuda", dtype=amp_dtype)
                if use_amp else nullcontext()
            )
            with amp:
                prediction = torch.sigmoid(model(batch))
                target = target_for(batch, args.score_scale, args.wdl_weight)
                loss = (prediction - target).abs().pow(args.loss_power).mean()
            scaler.scale(loss).backward()
            scaler.step(optimizer)
            scaler.update()
            model.clamp_for_quantization()
            if schedule_free:
                with torch.no_grad():
                    for parameter in model.parameters():
                        z = optimizer.state.get(parameter, {}).get("z")
                        if z is not None:
                            z.clamp_(-1.0, 1.0)

            steps += 1
            positions += batch.size
            loss_total += loss.detach()
            if batch_index % args.log_interval == 0:
                average_loss = loss_total.item() / batch_index
                elapsed = time.perf_counter() - started
                print(
                    f"epoch={epoch + 1} batch={batch_index} "
                    f"loss={average_loss:.7f} "
                    f"positions/s={positions / elapsed:,.0f}"
                )

        if batch_count == 0:
            raise ValueError("dataset does not contain one complete batch")
        average_loss = loss_total.item() / batch_count
        training_elapsed = time.perf_counter() - started
        if schedule_free:
            optimizer.eval()
            model.clamp_for_quantization()
        checkpoint_path = Path(args.output).with_suffix(".pt")
        torch.save(
            {
                "model": model.state_dict(),
                "optimizer": optimizer.state_dict(),
                "epoch": epoch,
                "steps": steps,
                "arguments": vars(args),
            },
            checkpoint_path,
        )
        export_nnue(model, args.output)
        print(
            f"epoch={epoch + 1} complete loss={average_loss:.7f} "
            f"positions/s={positions / training_elapsed:,.0f} nnue={args.output}"
        )


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("dataset", nargs="+", help="Motor dataset file(s)")
    parser.add_argument("--output", default="nnue-trained.bin")
    parser.add_argument("--resume")
    parser.add_argument("--device", default="auto", help="auto, cuda, mps, or cpu")
    parser.add_argument("--epochs", type=int, default=100)
    parser.add_argument("--batch-size", type=int, default=16384)
    parser.add_argument("--block-records", type=int, default=1 << 20)
    parser.add_argument("--learning-rate", type=float, default=1e-3)
    parser.add_argument("--weight-decay", type=float, default=1e-2)
    parser.add_argument(
        "--optimizer", choices=("adamw", "schedule-free-adamw"),
        default="schedule-free-adamw",
    )
    parser.add_argument("--warmup-steps", type=int, default=0)
    parser.add_argument("--score-scale", type=float, default=400.0)
    parser.add_argument("--wdl-weight", type=float, default=0.5)
    parser.add_argument("--loss-power", type=float, default=2.0)
    parser.add_argument("--seed", type=int, default=1)
    parser.add_argument("--log-interval", type=int, default=100)
    parser.add_argument("--prefetch", type=int, default=2)
    parser.add_argument("--no-amp", action="store_true")
    parser.add_argument("--amp-dtype", choices=("bfloat16", "float16"), default="bfloat16")
    return parser.parse_args()


def main() -> None:
    train(parse_args())


if __name__ == "__main__":
    main()
