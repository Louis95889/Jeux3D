#!/usr/bin/env python3
import struct, zlib, os, random

def chunk_png(name, data):
    c = name + data
    return struct.pack('>I', len(data)) + c + struct.pack('>I', zlib.crc32(c) & 0xFFFFFFFF)

def make_png(pixels, w, h):
    raw = b''
    for y in range(h):
        raw += b'\x00'
        for x in range(w):
            raw += bytes(pixels[y][x])
    return (b'\x89PNG\r\n\x1a\n'
        + chunk_png(b'IHDR', struct.pack('>IIBBBBB', w, h, 8, 6, 0, 0, 0))
        + chunk_png(b'IDAT', zlib.compress(raw, 9))
        + chunk_png(b'IEND', b''))

def save(name, pixels, w, h):
    path = os.path.join('textures', name)
    with open(path, 'wb') as f:
        f.write(make_png(pixels, w, h))
    print(f"  OK  {name}")

def clamp(v): return max(0, min(255, v))

def upscale(px, sw, sh, scale):
    dw, dh = sw * scale, sh * scale
    return [[px[y // scale][x // scale] for x in range(dw)] for y in range(dh)], dw, dh

N = 16
S = 4

def oak_log_side(n):
    rng = random.Random(42)
    px = []
    for y in range(n):
        row = []
        for x in range(n):
            if x in (0, n - 1):
                c = (67, 43, 20, 255)
            elif x in (1, n - 2):
                c = (85, 57, 26, 255)
            else:
                v = rng.randint(-8, 8)
                base = (100, 70, 33) if x % 4 == 2 else (125, 90, 48)
                c = (clamp(base[0]+v), clamp(base[1]+v), clamp(base[2]+v//2), 255)
            row.append(c)
        px.append(row)
    return px

def oak_log_top(n):
    rng = random.Random(43)
    cx, cy = n / 2 - 0.5, n / 2 - 0.5
    px = []
    for y in range(n):
        row = []
        for x in range(n):
            d = max(abs(x - cx), abs(y - cy))
            v = rng.randint(-6, 6)
            if d >= n / 2 - 0.5:
                c = (clamp(67+v//2), clamp(43+v//2), clamp(20+v//3), 255)
            elif d > n / 2 - 3:
                c = (clamp(90+v), clamp(62+v), clamp(30+v//2), 255)
            elif d > n / 2 - 5:
                c = (clamp(108+v), clamp(76+v), clamp(40+v//2), 255)
            else:
                c = (clamp(120+v), clamp(86+v), clamp(46+v//2), 255)
            row.append(c)
        px.append(row)
    return px

def oak_leaves(n):
    rng = random.Random(44)
    corners = set()
    for y in range(n):
        for x in range(n):
            d = abs(x - n//2 + 0.5) + abs(y - n//2 + 0.5)
            if d > n // 2 + 2 and rng.random() < 0.8:
                corners.add((x, y))
    px = []
    for y in range(n):
        row = []
        for x in range(n):
            if (x, y) in corners:
                row.append((0, 0, 0, 0))
            else:
                v = rng.randint(-20, 20)
                r = clamp(55 + v // 2)
                g = clamp(100 + v)
                b = clamp(30 + v // 4)
                if rng.random() < 0.1:
                    r, g, b = clamp(r+20), clamp(g+25), clamp(b+5)
                row.append((r, g, b, 255))
        px.append(row)
    return px

def grass_top(n):
    rng = random.Random(100)
    px = []
    for y in range(n):
        row = []
        for x in range(n):
            v = rng.randint(-25, 25)
            row.append((clamp(92+v//3), clamp(148+v), clamp(40+v//4), 255))
        px.append(row)
    return px

def grass_side(n):
    rng = random.Random(101)
    px = []
    for y in range(n):
        row = []
        for x in range(n):
            v = rng.randint(-15, 15)
            if y <= 3:
                row.append((clamp(88+v//2), clamp(145+v), clamp(38+v//4), 255))
            else:
                r, g, b = clamp(118+v), clamp(84+v//2), clamp(52+v//3)
                if rng.random() < 0.06:
                    r, g, b = 88, 60, 36
                row.append((r, g, b, 255))
        px.append(row)
    return px

def dirt(n):
    rng = random.Random(102)
    px = []
    for y in range(n):
        row = []
        for x in range(n):
            v = rng.randint(-20, 20)
            r, g, b = clamp(118+v), clamp(84+v//2), clamp(52+v//3)
            if rng.random() < 0.07:
                r, g, b = clamp(r-30), clamp(g-20), clamp(b-15)
            row.append((r, g, b, 255))
        px.append(row)
    return px

def stone(n):
    rng = random.Random(103)
    px = []
    for y in range(n):
        row = []
        for x in range(n):
            v = rng.randint(-28, 28)
            c = clamp(128 + v)
            if rng.random() < 0.05:
                c = clamp(c - 35)
            row.append((c, c, c, 255))
        px.append(row)
    return px

def wall_tex(n):
    rng = random.Random(200)
    px = []
    for y in range(n):
        row = []
        for x in range(n):
            joint_x = (x % 8 == 0)
            joint_y = (y % 4 == 0) or (y % 8 == 4 and x % 8 >= 4)
            if joint_x or joint_y:
                c = 90
            else:
                v = rng.randint(-20, 20)
                c = clamp(135 + v)
            row.append((c, c, c, 255))
        px.append(row)
    return px

print("Generation des textures style Minecraft...")
os.makedirs('textures', exist_ok=True)
textures = [
    ('trunk.png',      oak_log_side(N)),
    ('trunk_top.png',  oak_log_top(N)),
    ('leaves.png',     oak_leaves(N)),
    ('grass_top.png',  grass_top(N)),
    ('grass_side.png', grass_side(N)),
    ('dirt.png',       dirt(N)),
    ('rock.png',       stone(N)),
    ('rock_side.png',  stone(N)),
    ('wall_ns.png',    wall_tex(N)),
    ('wall_eo.png',    wall_tex(N)),
]
for name, px in textures:
    px64, w64, h64 = upscale(px, N, N, S)
    save(name, px64, w64, h64)
print(f"Termine ! {len(textures)} textures dans textures/")
