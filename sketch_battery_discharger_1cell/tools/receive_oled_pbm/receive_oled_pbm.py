#!/usr/bin/env python3
from __future__ import annotations

import argparse
from datetime import datetime
from io import BytesIO
from pathlib import Path
from typing import Final

import serial
from PIL import Image

INVERT_IMAGE = True


def read_exact(ser: serial.Serial, size: int) -> bytes:
    data = ser.read(size)
    if len(data) != size:
        raise RuntimeError(f"short read: {len(data)} / {size}")
    return data


def read_pbm_frame(ser: serial.Serial) -> bytes:
    magic = ser.readline()
    if magic.strip() != b"P4":
        raise RuntimeError(f"unexpected magic: {magic!r}")

    size_line = ser.readline().strip()
    try:
        width_str, height_str = size_line.split()
        width = int(width_str)
        height = int(height_str)
    except Exception as exc:
        raise RuntimeError(f"invalid size line: {size_line!r}") from exc

    row_bytes = (width + 7) // 8
    image_size = row_bytes * height
    image_data = read_exact(ser, image_size)

    return b"P4\n" + f"{width} {height}\n".encode("ascii") + image_data


def save_png_from_pbm(pbm_bytes: bytes, output_dir: Path, prefix: str) -> Path:
    timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
    png_path = output_dir / f"{prefix}_{timestamp}.png"

    with Image.open(BytesIO(pbm_bytes)) as image:
        if INVERT_IMAGE:
            image = image.point(lambda value: 255 - value)
        image.save(png_path, format="PNG")

    return png_path


TEXT_ENCODING: Final[str] = "utf-8"


def print_text_line(line: bytes) -> None:
    text = line.decode(TEXT_ENCODING, errors="replace").rstrip("\r\n")
    if text:
        print(text)


def receive_loop(ser: serial.Serial, output_dir: Path, prefix: str, keep_pbm: bool) -> None:
    print(f"listening on {ser.port} @ {ser.baudrate}...")

    while True:
        line = ser.readline()
        if not line:
            continue

        if line.strip() == b"P4":
            try:
                size_line = ser.readline()
                if not size_line:
                    raise RuntimeError("timed out while reading PBM size line")

                size_line = size_line.strip()
                width_str, height_str = size_line.split()
                width = int(width_str)
                height = int(height_str)
                row_bytes = (width + 7) // 8
                image_size = row_bytes * height
                image_data = read_exact(ser, image_size)
                pbm_bytes = b"P4\n" + f"{width} {height}\n".encode("ascii") + image_data
            except Exception as exc:
                print(f"PBM receive error: {exc}")
                continue

            png_path = save_png_from_pbm(pbm_bytes, output_dir, prefix)
            print(f"saved png: {png_path}")

            if keep_pbm:
                pbm_path = png_path.with_suffix(".pbm")
                pbm_path.write_bytes(pbm_bytes)
                print(f"saved pbm: {pbm_path}")

            continue

        print_text_line(line)


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Receive an OLED PBM frame over serial and save it as a timestamped PNG."
    )
    parser.add_argument("port", help="Serial port name, for example COM6")
    parser.add_argument("--baud", type=int, default=115200, help="Serial baud rate")
    parser.add_argument(
        "--output-dir",
        type=Path,
        default=Path("captures"),
        help="Directory to save PNG files into",
    )
    parser.add_argument(
        "--prefix",
        default="oled_capture",
        help="Filename prefix for saved PNG files",
    )
    parser.add_argument(
        "--timeout",
        type=float,
        default=10.0,
        help="Serial read timeout in seconds",
    )
    parser.add_argument(
        "--keep-pbm",
        action="store_true",
        help="Also save the received PBM alongside the PNG",
    )
    return parser.parse_args()


def main() -> None:
    args = parse_args()
    args.output_dir.mkdir(parents=True, exist_ok=True)

    with serial.Serial(args.port, args.baud, timeout=args.timeout) as ser:
        receive_loop(ser, args.output_dir, args.prefix, args.keep_pbm)


if __name__ == "__main__":
    main()
