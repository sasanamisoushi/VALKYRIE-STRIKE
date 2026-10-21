"""Generate the restrained title-menu labels and compact control guide."""

from pathlib import Path

from PIL import Image, ImageDraw, ImageFont


OUTPUT_DIR = Path(__file__).resolve().parents[1] / "project" / "resources"
FONT_PATH = Path("C:/Windows/Fonts/seguisb.ttf")


def get_font(size: int) -> ImageFont.FreeTypeFont:
    return ImageFont.truetype(str(FONT_PATH), size)


def draw_centered_label(
    draw: ImageDraw.ImageDraw,
    text: str,
    region: tuple[int, int, int, int],
    font_size: int,
) -> None:
    left, top, width, height = region
    label_font = get_font(font_size)
    bounds = draw.textbbox((0, 0), text, font=label_font)
    text_width = bounds[2] - bounds[0]
    text_height = bounds[3] - bounds[1]
    x = left + (width - text_width) / 2 - bounds[0]
    y = top + (height - text_height) / 2 - bounds[1]
    # A subtle navy edge keeps the white type readable over the terrain.
    draw.text(
        (x, y), text, font=label_font,
        fill=(235, 247, 255, 255),
        stroke_width=1, stroke_fill=(9, 24, 42, 255),
    )


def save_menu_labels() -> None:
    image = Image.new("RGBA", (1214, 1295), (0, 0, 0, 0))
    draw = ImageDraw.Draw(image)
    labels = (
        ("START GAME", (75, 45, 1065, 140), 104),
        ("SETTINGS", (170, 195, 880, 130), 98),
        ("EXIT GAME", (145, 345, 930, 140), 102),
        ("MASTER VOLUME", (35, 500, 1145, 140), 92),
        ("MOUSE SENSITIVITY", (10, 650, 1195, 140), 84),
        ("CONTROL GUIDE", (75, 805, 1060, 140), 92),
        ("BACK", (330, 960, 555, 135), 100),
        ("ON / OFF", (245, 1100, 725, 145), 94),
    )
    for text, region, font_size in labels:
        draw_centered_label(draw, text, region, font_size)
    image.save(OUTPUT_DIR / "title_menu_labels_simple.png")


def save_control_guide() -> None:
    image = Image.new("RGBA", (720, 224), (0, 0, 0, 0))
    draw = ImageDraw.Draw(image)
    # A quiet navy card with one cyan rule, rather than a second logo-like frame.
    draw.rounded_rectangle((2, 2, 717, 221), radius=14, fill=(5, 20, 35, 224), outline=(57, 190, 222, 230), width=3)
    draw.rectangle((28, 48, 32, 190), fill=(72, 221, 244, 255))
    small_font = get_font(29)
    key_font = get_font(38)
    draw.text((54, 20), "CONTROLS", font=small_font, fill=(102, 216, 239, 255))
    draw.text((56, 79), "W / S", font=key_font, fill=(239, 249, 255, 255))
    draw.text((286, 86), "SELECT", font=small_font, fill=(183, 204, 216, 255))
    draw.text((56, 141), "SPACE", font=key_font, fill=(239, 249, 255, 255))
    draw.text((286, 148), "CONFIRM", font=small_font, fill=(183, 204, 216, 255))
    image.save(OUTPUT_DIR / "title_controls_simple.png")


OUTPUT_DIR.mkdir(parents=True, exist_ok=True)
save_menu_labels()
save_control_guide()
