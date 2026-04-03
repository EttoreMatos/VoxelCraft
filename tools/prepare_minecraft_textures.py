#!/usr/bin/env python3

from __future__ import annotations

import argparse
import io
import pathlib
import sys
import zipfile

from PIL import Image

TILE_SIZE = 16
ATLAS_GRID = 4

TEXTURES = [
    ("grass_top", "grass_block_top.png"),
    ("grass_side", "grass_block_side.png"),
    ("dirt", "dirt.png"),
    ("stone", "stone.png"),
    ("sand", "sand.png"),
    ("oak_log", "oak_log.png"),
    ("oak_log_top", "oak_log_top.png"),
    ("oak_leaves", "oak_leaves.png"),
    ("bricks", "bricks.png"),
    ("cobblestone", "cobblestone.png"),
    ("oak_planks", "oak_planks.png"),
    ("gravel", "gravel.png"),
    ("snow_block", "snow_block.png"),
]


def candidate_paths(filename: str) -> list[str]:
    return [
        f"assets/minecraft/textures/block/{filename}",
        f"minecraft/textures/block/{filename}",
        f"textures/block/{filename}",
        filename,
    ]


def read_bytes_from_zip(path: pathlib.Path, filename: str) -> bytes | None:
    with zipfile.ZipFile(path) as archive:
        for candidate in candidate_paths(filename):
            try:
                return archive.read(candidate)
            except KeyError:
                continue
    return None


def read_bytes_from_dir(path: pathlib.Path, filename: str) -> bytes | None:
    for candidate in candidate_paths(filename):
        full = path / candidate
        if full.is_file():
            return full.read_bytes()
    return None


def load_image(source: pathlib.Path, filename: str) -> Image.Image:
    payload: bytes | None = None

    if source.is_file():
        payload = read_bytes_from_zip(source, filename)
    elif source.is_dir():
        payload = read_bytes_from_dir(source, filename)
        if payload is None:
            jars = sorted(source.glob("*.jar"))
            if len(jars) == 1:
                payload = read_bytes_from_zip(jars[0], filename)

    if payload is None:
        raise FileNotFoundError(f"Texture not found: {filename}")

    image = Image.open(io.BytesIO(payload)).convert("RGBA")
    resampling = getattr(Image, "Resampling", Image)
    return image.resize((TILE_SIZE, TILE_SIZE), resampling.NEAREST)


def build_atlas(source: pathlib.Path, output: pathlib.Path) -> None:
    atlas = Image.new("RGBA", (ATLAS_GRID * TILE_SIZE, ATLAS_GRID * TILE_SIZE), (0, 0, 0, 0))
    for index, (_, filename) in enumerate(TEXTURES):
        tile = load_image(source, filename)
        x = (index % ATLAS_GRID) * TILE_SIZE
        y = (index // ATLAS_GRID) * TILE_SIZE
        atlas.paste(tile, (x, y))

    output.parent.mkdir(parents=True, exist_ok=True)
    flipped = atlas.transpose(Image.FLIP_TOP_BOTTOM)
    output.write_bytes(flipped.tobytes())


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Prepare a tiny raw atlas from official Minecraft textures.")
    parser.add_argument("--source", required=True, help="Path to a Minecraft jar/zip or extracted assets directory.")
    parser.add_argument("--out", required=True, help="Output raw RGBA atlas path.")
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    source = pathlib.Path(args.source).expanduser().resolve()
    output = pathlib.Path(args.out).expanduser().resolve()

    if not source.exists():
        print(f"Source path does not exist: {source}", file=sys.stderr)
        return 1

    try:
        build_atlas(source, output)
    except Exception as exc:  # pragma: no cover - best effort helper
        print(str(exc), file=sys.stderr)
        return 1

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
