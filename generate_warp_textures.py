import os
import math
import random
from PIL import Image, ImageDraw, ImageFilter

random.seed(42)

os.makedirs("project/resources", exist_ok=True)

# 1. warp_nebula_stars.png (外側シリンダー: 星屑 + 宇宙ネビュラ + 光の筋)
w, h = 1024, 1024
img_stars = Image.new("RGBA", (w, h), (3, 6, 18, 255))
draw_stars = ImageDraw.Draw(img_stars)

# 背景のエネルギーネビュラ（縦のグラデーション帯）
for i in range(16):
    center_x = (i * w // 16 + 32) % w
    strip_w = 40 + (i % 5) * 15
    col_r = int(10 + math.sin(i * 0.7) * 8)
    col_g = int(25 + math.sin(i * 0.5) * 15)
    col_b = int(70 + math.cos(i * 0.8) * 30)
    for y in range(0, h, 4):
        # シームレスな揺らぎ
        phase = y / h * math.pi * 2
        offset_x = math.sin(phase * 2 + i) * 20
        x1 = int(center_x + offset_x - strip_w // 2) % w
        x2 = int(center_x + offset_x + strip_w // 2) % w
        alpha = int(40 + math.sin(phase * 3 + i) * 20)
        draw_stars.line([(x1, y), (x2, y)], fill=(col_r, col_g, col_b, alpha), width=4)

# 縦に伸びる光の筋 (ライトストリーム)
for i in range(60):
    lx = random.randint(0, w - 1)
    lw = random.randint(1, 3)
    alpha = random.randint(50, 160)
    length = random.randint(100, 400)
    y_start = random.randint(0, h - 1)
    col_cyan = (random.randint(40, 100), random.randint(140, 220), 255, alpha)
    for seg in range(length):
        cy = (y_start + seg) % h
        seg_alpha = int(alpha * (1.0 - math.cos(seg / length * math.pi * 2)) * 0.5)
        img_stars.putpixel((lx, cy), (col_cyan[0], col_cyan[1], col_cyan[2], max(seg_alpha, img_stars.getpixel((lx, cy))[3])))

# 星屑パーティクル (無数の光点)
for _ in range(800):
    sx = random.randint(0, w - 1)
    sy = random.randint(0, h - 1)
    brightness = random.randint(120, 255)
    size = random.choice([1, 1, 1, 2, 2, 3])
    # 青白〜シアン
    r = int(brightness * 0.7)
    g = int(brightness * 0.9)
    b = 255
    if size == 1:
        img_stars.putpixel((sx, sy), (r, g, b, 255))
    else:
        for dx in range(-size + 1, size):
            for dy in range(-size + 1, size):
                if dx * dx + dy * dy <= size * size:
                    px = (sx + dx) % w
                    py = (sy + dy) % h
                    factor = 1.0 - (dx * dx + dy * dy) ** 0.5 / size
                    img_stars.putpixel((px, py), (r, g, b, max(int(255 * factor), img_stars.getpixel((px, py))[3])))

img_stars = img_stars.filter(ImageFilter.GaussianBlur(radius=0.6))
img_stars.save("project/resources/warp_nebula_stars.png", "PNG")
print("Saved warp_nebula_stars.png")


# 2. warp_stream_lines.png (内側シリンダー: 高速で流れる加算ブレンドのストリームライン)
img_stream = Image.new("RGBA", (w, h), (0, 0, 0, 0))
draw_stream = ImageDraw.Draw(img_stream)

for i in range(120):
    lx = random.randint(0, w - 1)
    lw = random.randint(1, 4)
    length = random.randint(150, 600)
    y_start = random.randint(0, h - 1)
    bright = random.randint(120, 255)
    r = int(bright * 0.4)
    g = int(bright * 0.85)
    b = 255
    for seg in range(length):
        cy = (y_start + seg) % h
        fade = math.sin(seg / length * math.pi)
        alpha = int(bright * fade * 0.7)
        for w_offset in range(lw):
            cx = (lx + w_offset) % w
            curr = img_stream.getpixel((cx, cy))
            img_stream.putpixel((cx, cy), (r, g, b, min(255, curr[3] + alpha)))

img_stream = img_stream.filter(ImageFilter.GaussianBlur(radius=0.8))
img_stream.save("project/resources/warp_stream_lines.png", "PNG")
print("Saved warp_stream_lines.png")


# 3. warp_flare_core.png (画面奥の中心点: 眩しいワープコア・光の収束点)
core_w, core_h = 512, 512
img_core = Image.new("RGBA", (core_w, core_h), (0, 0, 0, 0))
cx, cy = core_w // 2, core_h // 2
max_r = core_w // 2

for y in range(core_h):
    for x in range(core_w):
        dx = x - cx
        dy = y - cy
        dist = math.hypot(dx, dy)
        if dist < max_r:
            norm_dist = dist / max_r
            # 強い中心グロー
            glow = math.exp(-norm_dist * 4.5)
            # 十字フレア
            flare_x = math.exp(-abs(dy) * 0.15) * math.exp(-abs(dx) * 0.02)
            flare_y = math.exp(-abs(dx) * 0.15) * math.exp(-abs(dy) * 0.02)
            val = min(1.0, glow + (flare_x + flare_y) * 0.4)
            r = int(min(255, val * 220 + glow * 35))
            g = int(min(255, val * 240 + glow * 15))
            b = 255
            alpha = int(min(255, val * 255))
            img_core.putpixel((x, y), (r, g, b, alpha))

img_core.save("project/resources/warp_flare_core.png", "PNG")
print("Saved warp_flare_core.png")


# 4. warp_speedline_particle.png (スピードライン・プラズマパーティクル用)
part_w, part_h = 64, 256
img_part = Image.new("RGBA", (part_w, part_h), (0, 0, 0, 0))
pcx = part_w // 2
for y in range(part_h):
    ny = y / part_h
    # 縦方向フェード
    v_fade = math.sin(ny * math.pi)
    for x in range(part_w):
        nx = abs(x - pcx) / (part_w // 2)
        h_fade = max(0.0, 1.0 - nx) ** 2
        val = v_fade * h_fade
        r = int(120 * val + 135 * (val ** 2))
        g = int(200 * val + 55 * (val ** 2))
        b = 255
        alpha = int(255 * val)
        img_part.putpixel((x, y), (r, g, b, alpha))

img_part.save("project/resources/warp_speedline_particle.png", "PNG")
print("Saved warp_speedline_particle.png")
