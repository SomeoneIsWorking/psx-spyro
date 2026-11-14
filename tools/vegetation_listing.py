"""Vegetation candidates of one level, for review (plan more-vegetation, step 2).

vegetation_listing.py <title> <ram.bin> <vram.bin> <out-dir>

Walks the near terrain sectors the terrain drawer walks (game/render/terrain/guest_terrain_mesh.h,
guest_terrain_polygon.h; Spyro 1 titles/spyro1/render/terrain_detail.cpp) and lists each texture record its
faces use: face count, area, flat share, two-sided and translucent faces. Each record's full-bright 32x32
tile is cut from VRAM under its CLUT into `<out-dir>/tex_<n>.png` and onto `<out-dir>/sheet.png`, so a
reviewer can mark the vegetation ones in the title's table. Inputs are `dumpram` and `vramraw` captures.
"""

from __future__ import annotations

import argparse
import math
import struct
from collections import defaultdict
from dataclasses import dataclass
from pathlib import Path

from PIL import Image, ImageDraw


@dataclass(frozen=True)
class Layout:
    sector_table: int  # word holding the near sector table
    sector_count: int
    textures: int  # word holding the texture records
    record_size: int
    full_bright: int  # offset of the undarkened (uv, page) pair in a record
    height_shift: int  # vertex height: (word << shift) & mask
    height_mask: int
    texture_byte: bool  # Spyro 1 names the texture by the links byte; Spyro 2/3 by the draw word


# Spyro 1: g_Environment 0x800785A8 (+0 sectors, +4 count, +0x18 detail textures); detail pass 80026788.
# Spyro 2/3: the detail pass's words (titles/spyro2|3/render/ terrain facts).
LAYOUTS = {
    "spyro1": Layout(0x800785A8, 0x800785AC, 0x800785C0, 16, 8, 2, 0xFFC, True),
    "spyro2": Layout(0x800673D4, 0x800673D8, 0x800673F4, 48, 8, 3, 0x1FFC, False),
    "spyro3": Layout(0x8006D048, 0x8006D04C, 0x8006D068, 48, 8, 3, 0x1FFC, False),
}


@dataclass
class Face:
    sector: int
    record: int  # guest address of the polygon record
    texture: int
    translucent: bool
    two_sided: bool
    flat: float  # |normal y| / |normal|
    area: float


def faces(title: str, ram: bytes) -> list[Face]:
    lay = LAYOUTS[title]

    def w(a: int) -> int:
        return struct.unpack_from("<I", ram, a & 0x1FFFFF)[0]

    table, count = w(lay.sector_table), w(lay.sector_count)
    out: list[Face] = []
    for i in range(count):
        s = w(table + 4 * i)
        if s == 0:
            continue
        translucent = (w(s + 4) & 0x5000) == 0x5000
        zx, y, layout = w(s + 8), w(s + 0xC), w(s + 0x14)
        first = s + 0x1C + ((layout >> 22) & 0x3FC)
        n = layout & 0xFF
        verts = []
        for v in range(n):
            word = w(first + 4 * v)
            verts.append((((zx & 0xFFFF) << 2) + ((word >> 8) & 0x1FFC),
                          (y >> 14) + ((word << lay.height_shift) & lay.height_mask),
                          (zx >> 14) + ((word >> 19) & 0x1FFC)))
        records = first + 4 * n + 2 * ((layout >> 6) & 0x3FC)
        end = records + ((layout >> 12) & 0xFF0)
        for r in range(records, end, 16):
            idx, links, draw = w(r), w(r + 8), w(r + 0xC)
            ids = [((idx >> 22) & 0x3FC) // 4, ((idx >> 14) & 0x3FC) // 4, ((idx >> 6) & 0x3FC) // 4, ((idx << 2) & 0x3FC) // 4]
            if max(ids) >= len(verts):
                continue
            if lay.texture_byte:
                byte = links & 0xFF
                if byte == 0xFF:
                    continue
                texture = byte & 0x7F
            else:
                texture = draw & 0x7F
            a, b, c = verts[ids[0]], verts[ids[1]], verts[ids[2]]
            u = [b[k] - a[k] for k in range(3)]
            v = [c[k] - a[k] for k in range(3)]
            nx, ny, nz = u[1] * v[2] - u[2] * v[1], u[2] * v[0] - u[0] * v[2], u[0] * v[1] - u[1] * v[0]
            length = math.sqrt(nx * nx + ny * ny + nz * nz) or 1.0
            area = length / 2 * (1 if ids[2] == ids[3] else 2)
            out.append(Face(s, r, texture, translucent, bool(draw & 0x800), abs(ny) / length, area))
    return out


def tile(title: str, ram: bytes, vram: bytes, texture: int) -> tuple[Image.Image, dict]:
    """The record's full-bright 32x32 tile and its identity (page, uv, clut)."""
    lay = LAYOUTS[title]

    def w(a: int) -> int:
        return struct.unpack_from("<I", ram, a & 0x1FFFFF)[0]

    record = w(lay.textures) + texture * lay.record_size + lay.full_bright
    uv, page = w(record), w(record + 4)
    clut, tpage = uv >> 16, page >> 16
    u0, v0 = uv & 0xFF, (uv >> 8) & 0xFF
    depth = (tpage >> 7) & 3
    px, py = (tpage & 0xF) * 64, ((tpage >> 4) & 1) * 256
    cx, cy = (clut & 0x3F) * 16, (clut >> 6) & 0x1FF

    def pixel(x: int, y: int) -> int:
        return struct.unpack_from("<H", vram, ((y & 511) * 1024 + (x & 1023)) * 2)[0]

    img = Image.new("RGBA", (32, 32))
    for ty in range(32):
        for tx in range(32):
            u, v = u0 + tx, v0 + ty
            if depth == 0:
                index = (pixel(px + u // 4, py + v) >> ((u & 3) * 4)) & 0xF
                c = pixel(cx + index, cy)
            elif depth == 1:
                index = (pixel(px + u // 2, py + v) >> ((u & 1) * 8)) & 0xFF
                c = pixel(cx + index, cy)
            else:
                c = pixel(px + u, py + v)
            rgba = (((c & 31) * 255) // 31, (((c >> 5) & 31) * 255) // 31, (((c >> 10) & 31) * 255) // 31, 0 if c == 0 else 255)
            img.putpixel((tx, ty), rgba)
    return img, {"page": tpage, "uv": (u0, v0), "clut": clut}


def main() -> None:
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("title", choices=sorted(LAYOUTS))
    ap.add_argument("ram", type=Path)
    ap.add_argument("vram", type=Path)
    ap.add_argument("out", type=Path)
    a = ap.parse_args()
    ram, vram = a.ram.read_bytes(), a.vram.read_bytes()
    stats: dict[int, dict] = defaultdict(lambda: {"n": 0, "area": 0.0, "flat": 0.0, "two": 0, "translucent": 0})
    for f in faces(a.title, ram):
        st = stats[f.texture]
        st["n"] += 1
        st["area"] += f.area
        st["flat"] += f.flat * f.area
        st["two"] += f.two_sided
        st["translucent"] += f.translucent
    a.out.mkdir(parents=True, exist_ok=True)
    rows = sorted(stats.items(), key=lambda kv: -kv[1]["area"])
    cell = 32 * 4
    sheet = Image.new("RGB", (8 * (cell + 8), ((len(rows) + 7) // 8) * (cell + 24)), (40, 40, 40))
    draw = ImageDraw.Draw(sheet)
    print(f"{len(rows)} textures, {sum(s['n'] for s in stats.values())} near faces")
    for k, (texture, st) in enumerate(rows):
        img, ident = tile(a.title, ram, vram, texture)
        big = img.resize((cell, cell), Image.NEAREST)
        big.save(a.out / f"tex_{texture:03d}.png")
        x, y = (k % 8) * (cell + 8), (k // 8) * (cell + 24)
        sheet.paste(big, (x, y), big)
        draw.text((x + 2, y + cell + 4), f"{texture} n{st['n']}", fill=(255, 255, 255))
        print(f"tex {texture:3d}: faces {st['n']:5d} area {st['area'] / 1e6:7.2f}M flat {st['flat'] / max(st['area'], 1):.2f} "
              f"two-sided {st['two']:4d} translucent {st['translucent']:4d} page 0x{ident['page']:04X} uv {ident['uv']} clut 0x{ident['clut']:04X}")
    sheet.save(a.out / "sheet.png")


if __name__ == "__main__":
    main()
