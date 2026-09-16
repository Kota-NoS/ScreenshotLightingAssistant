from __future__ import annotations

from collections import deque
import math
from pathlib import Path
import random

from PIL import Image, ImageChops, ImageDraw, ImageFilter


ROOT = Path(__file__).resolve().parents[1]
MOTE_SOURCE = ROOT / "assets" / "source" / "luminous_mote.png"
MOTE_PREVIEW = ROOT / "assets" / "source" / "luminous_mote_twinkle_atlas.png"
MOTE_DDS = ROOT / "textures" / "ScreenshotLightingAssistant" / "effects" / "luminous_mote_twinkle_atlas.dds"

SOURCE = ROOT / "assets" / "source" / "sakura_petal.png"
PREVIEW = ROOT / "assets" / "source" / "sakura_petal_particle.png"
DDS = ROOT / "textures" / "ScreenshotLightingAssistant" / "effects" / "sakura_petal.dds"

ATLAS_SOURCE = ROOT / "assets" / "source" / "sakura_petal_atlas_raw.png"
ATLAS_PREVIEW = ROOT / "assets" / "source" / "sakura_petal_atlas.png"
ATLAS_DDS = ROOT / "textures" / "ScreenshotLightingAssistant" / "effects" / "sakura_petal_atlas.dds"

STAR_SOURCE = ROOT / "assets" / "source" / "magic_star.png"
STAR_PREVIEW = ROOT / "assets" / "source" / "magic_star_particle.png"
STAR_DDS = ROOT / "textures" / "ScreenshotLightingAssistant" / "effects" / "magic_star.dds"

LEAF_SOURCE = ROOT / "assets" / "source" / "autumn_leaf_tumble_atlas_raw.png"
LEAF_PREVIEW = ROOT / "assets" / "source" / "autumn_leaf_atlas.png"
LEAF_DDS = ROOT / "textures" / "ScreenshotLightingAssistant" / "effects" / "autumn_leaf_atlas.dds"

FEATHER_SOURCE = ROOT / "assets" / "source" / "white_feather_tumble_atlas_raw.png"
FEATHER_PREVIEW = ROOT / "assets" / "source" / "white_feather_atlas.png"
FEATHER_DDS = ROOT / "textures" / "ScreenshotLightingAssistant" / "effects" / "white_feather_atlas.dds"
BLACK_FEATHER_PREVIEW = ROOT / "assets" / "source" / "black_feather_atlas.png"
BLACK_FEATHER_DDS = ROOT / "textures" / "ScreenshotLightingAssistant" / "effects" / "black_feather_atlas.dds"

POP_STAR_SOURCE = ROOT / "assets" / "source" / "pop_star_set_raw.png"
POP_STAR_PREVIEW = ROOT / "assets" / "source" / "pop_star_atlas.png"
POP_STAR_DDS = ROOT / "textures" / "ScreenshotLightingAssistant" / "effects" / "pop_star_atlas.dds"

SNOW_CRYSTAL_PREVIEW = ROOT / "assets" / "source" / "snow_crystal_atlas.png"
SNOW_CRYSTAL_DARK_PREVIEW = ROOT / "assets" / "source" / "snow_crystal_atlas_dark_preview.png"
SNOW_CRYSTAL_LIGHT_PREVIEW = ROOT / "assets" / "source" / "snow_crystal_atlas_light_preview.png"
SNOW_CRYSTAL_REFERENCE = ROOT / "assets" / "reference" / "snow_crystal_material_reference.png"
SNOW_CRYSTAL_DDS = ROOT / "textures" / "ScreenshotLightingAssistant" / "effects" / "snow_crystal_atlas.dds"


def largest_alpha_component(image: Image.Image, threshold: int = 20) -> Image.Image:
    """Keep the petal and discard disconnected generation specks."""
    rgba = image.convert("RGBA")
    alpha = rgba.getchannel("A")
    width, height = rgba.size
    visible = bytearray(1 if value >= threshold else 0 for value in alpha.getdata())
    seen = bytearray(width * height)
    largest: list[int] = []

    for start in range(width * height):
        if not visible[start] or seen[start]:
            continue
        component: list[int] = []
        queue = deque([start])
        seen[start] = 1
        while queue:
            index = queue.popleft()
            component.append(index)
            x, y = index % width, index // width
            for ny in range(max(0, y - 1), min(height, y + 2)):
                for nx in range(max(0, x - 1), min(width, x + 2)):
                    neighbour = ny * width + nx
                    if visible[neighbour] and not seen[neighbour]:
                        seen[neighbour] = 1
                        queue.append(neighbour)
        if len(component) > len(largest):
            largest = component

    if not largest:
        raise RuntimeError("Atlas cell contains no visible petal")
    mask = Image.new("L", rgba.size, 0)
    mask_data = bytearray(width * height)
    source_alpha = bytes(alpha.getdata())
    for index in largest:
        mask_data[index] = source_alpha[index]
    mask.frombytes(bytes(mask_data))
    result = rgba.copy()
    result.putalpha(mask)
    return result


def prepare_single() -> None:
    source = Image.open(SOURCE).convert("RGBA")
    alpha = source.getchannel("A")
    bounds = alpha.getbbox()
    if bounds is None:
        raise RuntimeError("The generated petal has no visible pixels")

    petal = source.crop(bounds)
    petal_alpha = petal.getchannel("A").point(lambda value: 0 if value < 10 else value)
    petal.putalpha(petal_alpha)

    canvas = Image.new("RGBA", (256, 256), (0, 0, 0, 0))
    petal.thumbnail((206, 206), Image.Resampling.LANCZOS)
    canvas.alpha_composite(petal, ((canvas.width - petal.width) // 2,
        (canvas.height - petal.height) // 2))

    final_alpha = canvas.getchannel("A").filter(ImageFilter.GaussianBlur(0.35))
    rgb = Image.new("RGB", canvas.size, (255, 214, 226))
    detail = ImageChops.multiply(canvas.convert("RGB"), Image.new("RGB", canvas.size, (255, 255, 255)))
    rgb.paste(detail, mask=final_alpha)
    prepared = Image.merge("RGBA", (*rgb.split(), final_alpha))
    prepared.save(PREVIEW, optimize=True)
    prepared.save(DDS, format="DDS", pixel_format="DXT5")


def prepare_atlas(source_path: Path, preview_path: Path, dds_path: Path, label: str,
    columns: int = 4, rows: int = 2) -> None:
    source = Image.open(source_path).convert("RGBA")
    cell_width = source.width // columns
    cell_height = source.height // rows
    atlas = Image.new("RGBA", (columns * 256, rows * 256), (0, 0, 0, 0))

    for row in range(rows):
        for column in range(columns):
            left = column * cell_width
            top = row * cell_height
            right = source.width if column == columns - 1 else (column + 1) * cell_width
            bottom = source.height if row == rows - 1 else (row + 1) * cell_height
            element = largest_alpha_component(source.crop((left, top, right, bottom)))
            bounds = element.getchannel("A").getbbox()
            if bounds is None:
                raise RuntimeError(f"{label} atlas cell {column},{row} has no visible element")
            element = element.crop(bounds)
            element.thumbnail((212, 212), Image.Resampling.LANCZOS)
            if label == "feather":
                element = add_long_feather_separations(element, column, row)
            x = column * 256 + (256 - element.width) // 2
            y = row * 256 + (256 - element.height) // 2
            atlas.alpha_composite(element, (x, y))

    alpha = atlas.getchannel("A").point(lambda value: 0 if value < 7 else value)
    alpha = alpha.filter(ImageFilter.GaussianBlur(0.25))
    atlas.putalpha(alpha)
    rgb = Image.new("RGB", atlas.size, (255, 220, 230))
    rgb.paste(atlas.convert("RGB"), mask=alpha)
    prepared = Image.merge("RGBA", (*rgb.split(), alpha))
    prepared.save(preview_path, optimize=True)
    prepared.save(dds_path, format="DDS", pixel_format="DXT5")


def prepare_sakura_material() -> None:
    """Preserve the hand-finished Sakura atlas and export a rebuild fallback."""
    source = Image.open(ATLAS_PREVIEW).convert("RGBA")
    alpha = source.getchannel("A")
    if source.size != (1024, 512) or alpha.getextrema()[1] == 0:
        raise RuntimeError("The hand-finished Sakura atlas is invalid")

    # Alpha43 promotes the manually repaired, colour-approved atlas to the
    # source of truth. Do not rebuild it from the earlier generated sample:
    # doing so would restore the missing right edges on the first two poses and
    # discard the approved white-to-blush grading. Pillow cannot encode BC7,
    # so this tool writes a visually equivalent DXT5 fallback; the packaged
    # runtime asset remains the artist-exported BC7 DDS tested in game.
    for row in range(2):
        for column in range(4):
            cell = alpha.crop((column * 256, row * 256,
                (column + 1) * 256, (row + 1) * 256))
            bounds = cell.point(lambda value: 255 if value > 3 else 0).getbbox()
            if bounds is None or bounds[0] == 0 or bounds[1] == 0 or bounds[2] == 256 or bounds[3] == 256:
                raise RuntimeError(f"Sakura atlas cell {column},{row} is empty or touches its UV boundary")

    source.save(ATLAS_DDS, format="DDS", pixel_format="DXT5")


def add_long_feather_separations(element: Image.Image, column: int, row: int) -> Image.Image:
    """Add sparse, directional vane breaks to only the smooth long-feather row."""
    if row != 1:
        return element

    result = element.copy()
    original_alpha = result.getchannel("A")
    bounds = original_alpha.getbbox()
    if bounds is None:
        raise RuntimeError(f"Long-feather frame {column} has no visible pixels")

    left, top, right, bottom = bounds
    width = right - left
    height = bottom - top
    supersample = 4
    cut_high = Image.new("L", (result.width * supersample,
        result.height * supersample), 255)
    draw = ImageDraw.Draw(cut_high)

    # Keep the cuts sparse and asymmetric. Their vertical positions remain
    # related across the eight tumble frames, while small per-frame offsets
    # prevent a regular saw-tooth silhouette. The edge-on frame is deliberately
    # shallower so its narrow body never breaks apart.
    frame_offsets = (-0.012, 0.008, -0.004, 0.014, -0.010, 0.006, 0.016, -0.006)
    specifications = (
        ("left", 0.24, 0.14, -0.020),
        ("right", 0.34, 0.12, 0.018),
        ("left", 0.48, 0.16, 0.024),
        ("right", 0.61, 0.14, -0.018),
        ("left", 0.73, 0.12, 0.020),
        ("right", 0.82, 0.10, -0.014),
    )

    def scaled(points: list[tuple[float, float]]) -> list[tuple[int, int]]:
        return [(round(x * supersample), round(y * supersample)) for x, y in points]

    for index, (side, fraction, depth_fraction, slant_fraction) in enumerate(specifications):
        # Alternate a few cuts between sides from frame to frame without making
        # the two edges mirror each other.
        if (column + index) % 5 == 3:
            side = "right" if side == "left" else "left"
        y = round(top + height * (fraction + frame_offsets[column]))
        row_alpha = list(original_alpha.crop((0, y, result.width, y + 1)).getdata())
        visible = [x for x, value in enumerate(row_alpha) if value >= 24]
        if len(visible) < 4:
            continue

        edge = min(visible) if side == "left" else max(visible)
        span = max(visible) - min(visible) + 1
        depth = max(2.5, min(10.0, span * depth_fraction))
        if width < 44:
            depth *= 0.58
        half_height = 1.55 + ((column * 3 + index) % 3) * 0.45
        slant = height * slant_fraction
        if side == "left":
            points = [(edge - 1.5, y - half_height),
                (edge - 1.5, y + half_height),
                (edge + depth, y + slant)]
        else:
            points = [(edge + 1.5, y - half_height),
                (edge + 1.5, y + half_height),
                (edge - depth, y + slant)]
        draw.polygon(scaled(points), fill=0)

    # A narrow V at the distal end prevents the broad views from finishing in
    # one hard, continuous cap. Keep the edge-on frame intact: at that angle a
    # split would read as a missing chunk instead of two soft tips.
    if width >= 44:
        probe_y = min(bottom - 1, top + max(3, round(height * 0.025)))
        probe = list(original_alpha.crop((0, probe_y, result.width, probe_y + 1)).getdata())
        visible = [x for x, value in enumerate(probe) if value >= 18]
        if visible:
            tip_x = (min(visible) + max(visible)) / 2 + (-1.0 if column % 2 else 1.0)
            tip_depth = min(11.0, max(7.0, height * 0.050))
            draw.polygon(scaled([
                (tip_x - 1.4, top - 1.0),
                (tip_x + 1.4, top - 1.0),
                (tip_x + (1.8 if column % 2 else -1.8), top + tip_depth),
            ]), fill=0)

    cut = cut_high.resize(result.size, Image.Resampling.LANCZOS)
    separated_alpha = ImageChops.multiply(original_alpha, cut)
    original_coverage = sum(original_alpha.getdata())
    separated_coverage = sum(separated_alpha.getdata())
    removed = original_coverage - separated_coverage
    if removed <= 0 or removed > original_coverage * 0.055:
        raise RuntimeError(f"Long-feather frame {column} separation coverage is unsafe")
    result.putalpha(separated_alpha)
    return result


def prepare_black_feather_atlas() -> None:
    """Derive neutral black feathers without changing white-feather coverage."""
    source = Image.open(FEATHER_PREVIEW).convert("RGBA")
    alpha = source.getchannel("A")
    luminance = source.convert("L").filter(ImageFilter.UnsharpMask(
        radius=1.6, percent=145, threshold=3))

    # The white atlas stores most visible detail between roughly 170 and 250.
    # Keep Black feathers black at the normal 1.00 brightness setting. Alpha27
    # still read as silver in game; its 0.26-0.36 brightness range was the
    # preferred target. Preserve only dim graphite detail in the shaft and a
    # few authored barb tips so the silhouette and flow remain legible.
    def remap(value: int) -> int:
        normalized = max(0.0, min(1.0, (value - 165.0) / 90.0))
        return round(5.0 + 65.0 * math.pow(normalized, 3.2))

    graphite = luminance.point(remap)
    # Fully transparent padding must be neutral black. This prevents the pink
    # hidden RGB used by the white atlas from bleeding into filtered black edges.
    padding = alpha.point(lambda value: 255 if value == 0 else 0)
    graphite.paste(5, mask=padding)
    prepared = Image.merge("RGBA", (graphite, graphite, graphite, alpha))
    prepared.save(BLACK_FEATHER_PREVIEW, optimize=True)
    prepared.save(BLACK_FEATHER_DDS, format="DDS", pixel_format="DXT5")


def prepare_white_feather_material() -> None:
    """Keep white feathers white while preserving shaft and vane definition."""
    source = Image.open(FEATHER_PREVIEW).convert("RGBA")
    alpha = source.getchannel("A")
    luminance = source.convert("L").filter(ImageFilter.UnsharpMask(
        radius=1.35, percent=175, threshold=2))

    # Skyrim's effect-shader brightness multiplier can drive a nearly-white
    # atlas into one flat value. Reserve a wider cool-gray-to-white range in
    # the authored texture so shafts, overlapping barbs and vane curvature
    # survive at 1.00 and remain visible longer when brightness is raised.
    def remap(value: int) -> int:
        normalized = max(0.0, min(1.0, (value - 125.0) / 130.0))
        return round(82.0 + 164.0 * math.pow(normalized, 1.28))

    value = luminance.point(remap)
    red = value.point(lambda level: max(0, level - 3))
    green = value.point(lambda level: min(255, level + 1))
    blue = value.point(lambda level: min(255, level + 6))
    prepared = Image.merge("RGBA", (red, green, blue, alpha))

    # Neutral hidden RGB prevents the former pink atlas padding from tinting
    # filtered edges. Coverage remains byte-for-byte identical.
    transparent = alpha.point(lambda level: 255 if level == 0 else 0)
    neutral_padding = Image.new("RGBA", prepared.size, (242, 246, 252, 0))
    prepared.paste(neutral_padding, mask=transparent)
    prepared.putalpha(alpha)
    prepared.save(FEATHER_PREVIEW, optimize=True)
    prepared.save(FEATHER_DDS, format="DDS", pixel_format="DXT5")


def prepare_star() -> None:
    source = largest_alpha_component(Image.open(STAR_SOURCE).convert("RGBA"), threshold=4)
    bounds = source.getchannel("A").getbbox()
    if bounds is None:
        raise RuntimeError("The generated star has no visible pixels")
    star = source.crop(bounds)
    star.thumbnail((218, 218), Image.Resampling.LANCZOS)

    canvas = Image.new("RGBA", (256, 256), (0, 0, 0, 0))
    canvas.alpha_composite(star, ((canvas.width - star.width) // 2,
        (canvas.height - star.height) // 2))
    alpha = canvas.getchannel("A").point(lambda value: 0 if value < 3 else value)
    alpha = alpha.filter(ImageFilter.GaussianBlur(0.20))
    canvas.putalpha(alpha)
    canvas.save(STAR_PREVIEW, optimize=True)
    canvas.save(STAR_DDS, format="DDS", pixel_format="DXT5")


def prepare_mote_twinkle_atlas() -> None:
    source = largest_alpha_component(Image.open(MOTE_SOURCE).convert("RGBA"), threshold=4)
    bounds = source.getchannel("A").getbbox()
    if bounds is None:
        raise RuntimeError("The luminous mote has no visible pixels")
    mote = source.crop(bounds)
    mote.thumbnail((212, 212), Image.Resampling.LANCZOS)

    # Keep a faint core at both ends rather than blinking fully off. The NIF
    # gives every particle a random start frame and cycle length, so nearby
    # motes pulse independently instead of flashing as one group.
    scales = (0.48, 0.57, 0.72, 0.90, 1.00, 0.86, 0.66, 0.53)
    opacities = (0.22, 0.34, 0.58, 0.86, 1.00, 0.74, 0.43, 0.27)
    atlas = Image.new("RGBA", (8 * 256, 256), (0, 0, 0, 0))
    for column, (scale, opacity) in enumerate(zip(scales, opacities)):
        frame = mote.resize((max(1, round(mote.width * scale)),
            max(1, round(mote.height * scale))), Image.Resampling.LANCZOS)
        alpha = frame.getchannel("A").point(
            lambda value, factor=opacity: round(value * factor))
        frame.putalpha(alpha)
        atlas.alpha_composite(frame, (column * 256 + (256 - frame.width) // 2,
            (256 - frame.height) // 2))

    alpha = atlas.getchannel("A").point(lambda value: 0 if value < 3 else value)
    alpha = alpha.filter(ImageFilter.GaussianBlur(0.20))
    atlas.putalpha(alpha)
    atlas.save(MOTE_PREVIEW, optimize=True)
    atlas.save(MOTE_DDS, format="DDS", pixel_format="DXT5")


def prepare_pop_star_atlas() -> None:
    # The generated sheet is retained as the design reference. Render the
    # shipping sprites deterministically so their corners stay sharp, their
    # fills stay genuinely flat, and no preview checkerboard enters the alpha.
    Image.open(POP_STAR_SOURCE).verify()

    def points(size: int, outer_radius: float, inner_radius: float) -> list[tuple[float, float]]:
        center = size / 2
        result: list[tuple[float, float]] = []
        for index in range(10):
            angle = -math.pi / 2 + index * math.pi / 5
            radius = outer_radius if index % 2 == 0 else inner_radius
            result.append((center + math.cos(angle) * radius,
                center + math.sin(angle) * radius))
        return result

    def icon(style: int) -> Image.Image:
        size = 512
        # Slightly fuller than a thin geometric star, while keeping every tip
        # crisp and pointed as in the approved shape direction.
        outer = points(size, 230.0, 112.0)
        mask = Image.new("L", (size, size), 0)
        ImageDraw.Draw(mask).polygon(outer, fill=255)
        result = Image.new("RGBA", (size, size), (0, 0, 0, 0))

        # Alpha22 uses the requested, clearly separated flat palette.  These
        # colours are rendered from numeric RGB values; no reference-image
        # pixels are copied into the shipping asset.
        if style == 0:
            ImageDraw.Draw(result).polygon(outer, fill=(255, 255, 102, 255))
            inner = points(size, 181.0, 86.0)
            ImageDraw.Draw(result).polygon(inner, fill=(0, 0, 0, 0))
        elif style == 1:
            ImageDraw.Draw(result).polygon(outer, fill=(220, 120, 133, 255))
        elif style == 2:
            ImageDraw.Draw(result).polygon(outer, fill=(251, 240, 122, 255))
        else:
            ImageDraw.Draw(result).polygon(outer, fill=(153, 204, 255, 255))
        return result

    palette = (
        (255, 255, 102),
        (220, 120, 133),
        (251, 240, 122),
        (153, 204, 255),
    )
    atlas = Image.new("RGBA", (8 * 256, 4 * 256), (0, 0, 0, 0))
    # Keep every frame visually identical.  The per-particle atlas-row lock is
    # retained, but Pop stars no longer pulse in size or opacity.
    scales = (1.00,) * 8
    opacities = (1.00,) * 8

    for row in range(4):
        # Give every invisible texel in a row the same RGB as its visible star.
        # Linear filtering can sample these texels at the silhouette, so keeping
        # them colour-consistent prevents unrelated RGB blocks from leaking into
        # a magnified edge.  Their alpha remains zero.
        row_colour = palette[row]
        atlas.paste((*row_colour, 0), (0, row * 256, atlas.width, (row + 1) * 256))
        star = icon(row)
        bounds = star.getchannel("A").getbbox()
        if bounds is None:
            raise RuntimeError(f"Pop-star style {row} is empty")
        star = star.crop(bounds)
        star.thumbnail((202, 202), Image.Resampling.LANCZOS)

        for column, (scale, opacity) in enumerate(zip(scales, opacities)):
            frame = star.resize((max(1, round(star.width * scale)),
                max(1, round(star.height * scale))), Image.Resampling.LANCZOS)
            alpha = frame.getchannel("A").point(
                lambda value, factor=opacity: round(value * factor))
            # Preserve the chosen flat colour below every alpha value, including
            # the hollow centre and fully transparent resampling fringe.
            frame = Image.new("RGBA", frame.size, (*row_colour, 0))
            frame.putalpha(alpha)
            atlas.paste(frame, (column * 256 + (256 - frame.width) // 2,
                row * 256 + (256 - frame.height) // 2))

    alpha = atlas.getchannel("A").point(lambda value: 0 if value < 4 else value)
    alpha = alpha.filter(ImageFilter.GaussianBlur(0.18))
    atlas.putalpha(alpha)
    # Keep the human-facing PNG conventional while retaining the hidden-RGB
    # padding exclusively in the shipping DDS.
    visible = alpha.point(lambda value: 255 if value > 0 else 0)
    preview = Image.composite(atlas, Image.new("RGBA", atlas.size), visible)
    preview.save(POP_STAR_PREVIEW, optimize=True)
    # Store Stars 2 as uncompressed RGBA8.  BC3/DXT5's shared 4x4 colour blocks
    # produced red/cyan/yellow speckles along highly magnified flat-colour edges.
    atlas.save(POP_STAR_DDS, format="DDS")


def _prepare_snow_crystal_atlas_vector_draft() -> None:
    """Draw four clearly different, photographic clear-ice crystal families.

    Alpha30 put the same rounded rime lobes on every family.  At gameplay scale
    those lobes erased the structural differences and its high-alpha body read
    as milky sugar.  Alpha31 instead uses four distinct silhouettes, broad but
    very low-alpha ice faces, angular facets and restrained brighter ice veins.
    It deliberately contains no circular rime granules or opaque white fill.
    """
    supersample = 4
    high_size = 256 * supersample
    origin = (high_size / 2, high_size / 2)

    def point(angle: float, radius: float,
        centre: tuple[float, float] = origin) -> tuple[float, float]:
        return (centre[0] + math.cos(angle) * radius,
            centre[1] + math.sin(angle) * radius)

    def tapered_quad(start: tuple[float, float], end: tuple[float, float],
        start_width: float, end_width: float) -> list[tuple[float, float]]:
        dx, dy = end[0] - start[0], end[1] - start[1]
        length = max(1.0, math.hypot(dx, dy))
        nx, ny = -dy / length, dx / length
        return [(start[0] + nx * start_width / 2, start[1] + ny * start_width / 2),
            (end[0] + nx * end_width / 2, end[1] + ny * end_width / 2),
            (end[0] - nx * end_width / 2, end[1] - ny * end_width / 2),
            (start[0] - nx * start_width / 2, start[1] - ny * start_width / 2)]

    def polygon_on(draw: ImageDraw.ImageDraw, vertices: list[tuple[float, float]],
        fill: int, outline: int | None = None, width: int = 1) -> None:
        draw.polygon(vertices, fill=fill)
        if outline is not None:
            draw.line(vertices + [vertices[0]], fill=outline, width=width,
                joint="curve")

    def crystal_layers(style: int) -> tuple[Image.Image, Image.Image,
        Image.Image, Image.Image]:
        face = Image.new("L", (high_size, high_size), 0)
        facet = Image.new("L", (high_size, high_size), 0)
        vein = Image.new("L", (high_size, high_size), 0)
        rim = Image.new("L", (high_size, high_size), 0)
        face_draw, facet_draw = ImageDraw.Draw(face), ImageDraw.Draw(facet)
        vein_draw, rim_draw = ImageDraw.Draw(vein), ImageDraw.Draw(rim)
        rng = random.Random(0xA131 + style * 977)

        def ice_blade(start: tuple[float, float], end: tuple[float, float],
            base_width: float, tip_width: float = 3.0,
            face_alpha: int = 23) -> None:
            shape = tapered_quad(start, end, base_width, tip_width)
            polygon_on(face_draw, shape, face_alpha)
            rim_draw.line(shape + [shape[0]], fill=90, width=7, joint="curve")
            vein_draw.line((start, end), fill=67, width=6)
            # Two offset lines turn the flat billboard into a faceted ice prism.
            dx, dy = end[0] - start[0], end[1] - start[1]
            length = max(1.0, math.hypot(dx, dy))
            nx, ny = -dy / length, dx / length
            offset = base_width * 0.22
            facet_draw.line(((start[0] + nx * offset, start[1] + ny * offset),
                (end[0] + nx * tip_width * 0.12,
                 end[1] + ny * tip_width * 0.12)), fill=43, width=4)

        def dendrite(radius: float, main_width: float,
            branch_specs: tuple[tuple[float, float, float, float], ...],
            hub_radius: float, face_alpha: int) -> None:
            hub = [point(-math.pi / 2 + index * math.pi / 3, hub_radius)
                for index in range(6)]
            polygon_on(face_draw, hub, face_alpha + 4)
            rim_draw.line(hub + [hub[0]], fill=96, width=7, joint="curve")
            inner = [point(-math.pi / 2 + index * math.pi / 3,
                hub_radius * 0.55) for index in range(6)]
            vein_draw.line(inner + [inner[0]], fill=63, width=5,
                joint="curve")

            for arm in range(6):
                angle = -math.pi / 2 + arm * math.pi / 3
                start = point(angle, hub_radius * 0.26)
                tip = point(angle, radius)
                ice_blade(start, tip, main_width, 5.0, face_alpha)
                for fraction, branch_length, spread, branch_width in branch_specs:
                    base = point(angle, radius * fraction)
                    for sign in (-1.0, 1.0):
                        branch_angle = angle + sign * spread
                        branch_tip = (base[0] + math.cos(branch_angle) * branch_length,
                            base[1] + math.sin(branch_angle) * branch_length)
                        ice_blade(base, branch_tip, branch_width, 3.0,
                            max(15, face_alpha - 2))

        if style == 0:
            # Dense photographic fern: the hero family selected by the user.
            dendrite(430.0, 48.0, ((0.27, 112.0, 0.70, 35.0),
                (0.42, 137.0, 0.66, 33.0), (0.57, 143.0, 0.62, 30.0),
                (0.71, 130.0, 0.59, 27.0), (0.83, 93.0, 0.55, 23.0)),
                66.0, 22)
        elif style == 1:
            # Compact balanced fern with short arms and a strong centre.
            dendrite(350.0, 61.0, ((0.39, 123.0, 0.71, 43.0),
                (0.59, 128.0, 0.65, 38.0), (0.76, 90.0, 0.59, 31.0)),
                92.0, 23)
        elif style == 2:
            # Open stellar dendrite: long bare shafts and very large gaps.
            dendrite(421.0, 35.0, ((0.43, 128.0, 0.70, 25.0),
                (0.68, 136.0, 0.62, 22.0), (0.83, 76.0, 0.56, 18.0)),
                53.0, 19)
        else:
            # A continuous hexagonal stellar plate, unmistakable at a distance.
            outer_radius = 314.0
            outer = [point(-math.pi / 2 + index * math.pi / 3, outer_radius)
                for index in range(6)]
            polygon_on(face_draw, outer, 20)
            rim_draw.line(outer + [outer[0]], fill=99, width=10,
                joint="curve")
            for scale, alpha in ((0.76, 51), (0.49, 43), (0.24, 54)):
                ring = [point(-math.pi / 2 + index * math.pi / 3,
                    outer_radius * scale) for index in range(6)]
                facet_draw.line(ring + [ring[0]], fill=alpha, width=6,
                    joint="curve")
            for arm in range(6):
                angle = -math.pi / 2 + arm * math.pi / 3
                vein_draw.line((origin, point(angle, outer_radius * 0.98)),
                    fill=58, width=5)
                # Small crystalline corner caps echo the chosen reference but
                # remain angular rather than becoming rounded beads.
                cap_center = point(angle, outer_radius * 0.98)
                cap = [point(-math.pi / 2 + index * math.pi / 3, 43.0,
                    cap_center) for index in range(6)]
                polygon_on(face_draw, cap, 24)
                rim_draw.line(cap + [cap[0]], fill=84, width=6,
                    joint="curve")

        # Fine deterministic internal fractures add photographic complexity
        # without changing a family's silhouette or filling it with white rime.
        face_bounds = face.getbbox()
        if face_bounds:
            face_pixels = face.load()
            for _ in range((105, 76, 54, 72)[style]):
                for _attempt in range(40):
                    x = rng.randrange(max(0, face_bounds[0]), min(high_size, face_bounds[2]))
                    y = rng.randrange(max(0, face_bounds[1]), min(high_size, face_bounds[3]))
                    if face_pixels[x, y] > 0:
                        length = rng.uniform(10.0, 36.0)
                        angle = rng.randrange(6) * math.pi / 3 + rng.uniform(-0.20, 0.20)
                        facet_draw.line(((x, y),
                            (x + math.cos(angle) * length,
                             y + math.sin(angle) * length)),
                            fill=rng.randint(31, 47), width=rng.choice((2, 3)))
                        break

        return tuple(layer.resize((256, 256), Image.Resampling.LANCZOS)
            for layer in (face, facet, vein, rim))

    atlas_alpha = Image.new("L", (8 * 256, 4 * 256), 0)
    # The hidden pixels deliberately use the same cool-blue family as visible
    # ice, preventing bilinear sampling from adding black or purple fringes.
    atlas_rgb = Image.new("RGB", atlas_alpha.size, (112, 160, 190))
    for row in range(4):
        face, facet, vein, rim = crystal_layers(row)
        frame = ImageChops.lighter(face, facet)
        frame = ImageChops.lighter(frame, vein)
        frame = ImageChops.lighter(frame, rim)
        frame_rgb = Image.new("RGB", (256, 256), (112, 160, 190))
        frame_rgb.paste((146, 188, 214), mask=facet)
        frame_rgb.paste((184, 216, 234), mask=vein)
        frame_rgb.paste((213, 234, 245), mask=rim)
        for column in range(8):
            atlas_alpha.paste(frame, (column * 256, row * 256))
            atlas_rgb.paste(frame_rgb, (column * 256, row * 256))

    # Keep hidden RGB identical to the ice colour so bilinear filtering cannot
    # introduce black, purple, or cyan squares at translucent edges.
    atlas = Image.merge("RGBA", (*atlas_rgb.split(), atlas_alpha))
    # Keep the source PNG easy to inspect in viewers that ignore alpha-channel
    # RGB. The shipping DDS below retains ice-blue hidden RGB for filtering.
    visible = atlas_alpha.point(lambda value: 255 if value > 0 else 0)
    preview = Image.composite(atlas, Image.new("RGBA", atlas.size), visible)
    preview.save(SNOW_CRYSTAL_PREVIEW, optimize=True)
    dark = Image.new("RGBA", atlas.size, (8, 12, 20, 255))
    dark.alpha_composite(atlas)
    dark.save(SNOW_CRYSTAL_DARK_PREVIEW, optimize=True)
    light = Image.new("RGBA", atlas.size, (218, 226, 232, 255))
    light.alpha_composite(atlas)
    light.save(SNOW_CRYSTAL_LIGHT_PREVIEW, optimize=True)
    # Preserve the subtle partial-alpha steps exactly; BC3 would quantize the
    # narrow branches and make the intended transparency flicker at a distance.
    atlas.save(SNOW_CRYSTAL_DDS, format="DDS")


def prepare_snow_crystal_atlas() -> None:
    """Build the shipping atlas from the user-selected clear-ice reference.

    The generated source contains four genuinely different crystals and fine
    photographic veins, but its nearly opaque alpha and cyan/purple fringe are
    unsuitable for a game particle.  Keep its luminance detail, rebuild every
    visible RGB value in one neutral ice-blue range, and remap alpha to a low
    translucent range.  The largest connected crystal in each quadrant is kept
    so generation specks and the almost-transparent black backdrop cannot ship.
    """
    if not SNOW_CRYSTAL_REFERENCE.exists():
        raise FileNotFoundError(f"Missing Snow reference: {SNOW_CRYSTAL_REFERENCE}")

    source = Image.open(SNOW_CRYSTAL_REFERENCE).convert("RGBA")
    width, height = source.size
    source_regions = (
        # The dense upper-left hero also crosses the exact vertical midpoint.
        # Its connected component continues to y=592 in the 1086px source, so
        # the old y=543 quadrant crop made the lower tip end on a straight line.
        (0, 0, width // 2, round(height * 0.60)),
        # The compact upper-right crystal crosses the exact vertical midpoint.
        # Include its complete lower arm instead of flattening the tip at y=543.
        (width // 2, 0, width, round(height * 0.60)),
        (0, height // 2, width // 2, height),
        (width // 2, height // 2, width, height),
    )
    # Preserve the visual hierarchy of the selected sheet: a large dense hero,
    # a compact dendrite, an open long-branched crystal and a small hex plate.
    target_sizes = (222, 196, 216, 150)
    atlas_alpha = Image.new("L", (8 * 256, 4 * 256), 0)
    hidden_ice = (104, 151, 181)
    atlas_rgb = Image.new("RGB", atlas_alpha.size, hidden_ice)

    row_hashes: list[bytes] = []
    for row, (box, target_size) in enumerate(zip(source_regions, target_sizes)):
        crystal = largest_alpha_component(source.crop(box), threshold=24)
        strong_alpha = crystal.getchannel("A").point(
            lambda value: 255 if value >= 24 else 0)
        bounds = strong_alpha.getbbox()
        if bounds is None:
            raise RuntimeError(f"Snow reference quadrant {row} is empty")
        if row < 2 and bounds[3] >= (box[3] - box[1]) - 8:
            raise RuntimeError(
                f"Snow reference quadrant {row} still touches its lower crop boundary")
        crystal = crystal.crop(bounds)

        converted: list[tuple[int, int, int, int]] = []
        for red, green, blue, alpha in crystal.getdata():
            luminance = (red * 0.2126 + green * 0.7152 + blue * 0.0722) / 255.0
            effective = (alpha / 255.0) * luminance
            if alpha < 24 or effective < 0.055:
                converted.append((*hidden_ice, 0))
                continue
            # A broad midtone in the reference becomes a transparent ice face;
            # only its finest white ridges approach the restrained alpha peak.
            # Keep the transparent ice body, but give the low and middle bands
            # enough coverage to remain legible against blue-grey exterior
            # scenery at the UI's 1.00 brightness baseline.
            out_alpha = min(174, round(10 + 164 * effective ** 1.10))
            shade = min(1.0, max(0.0, luminance))
            out_red = round(91 + 124 * shade)
            out_green = round(138 + 96 * shade)
            out_blue = round(171 + 76 * shade)
            converted.append((out_red, out_green, out_blue, out_alpha))

        crystal.putdata(converted)
        crystal.thumbnail((target_size, target_size), Image.Resampling.LANCZOS)
        frame = Image.new("RGBA", (256, 256), (*hidden_ice, 0))
        frame.alpha_composite(crystal,
            ((256 - crystal.width) // 2, (256 - crystal.height) // 2))
        # Lanczos works on straight RGBA and can pull arbitrary source RGB into
        # a fractional edge. Reconstruct RGB once more from the final alpha so
        # every visible and hidden texel is neutral ice blue with no hue noise.
        cleaned: list[tuple[int, int, int, int]] = []
        for _red, _green, _blue, alpha in frame.getdata():
            if alpha < 3:
                cleaned.append((*hidden_ice, 0))
                continue
            strength = min(1.0, max(0.0, (alpha - 10) / 170.0))
            cleaned.append((round(145 + 95 * strength),
                round(180 + 65 * strength), round(205 + 45 * strength), alpha))
        frame.putdata(cleaned)
        frame_alpha = frame.getchannel("A")
        row_hashes.append(frame.tobytes())

        for column in range(8):
            position = (column * 256, row * 256)
            atlas_alpha.paste(frame_alpha, position)
            atlas_rgb.paste(frame.convert("RGB"), position)

    if len(set(row_hashes)) != 4:
        raise RuntimeError("Snow-crystal reference families are not distinct")

    atlas = Image.merge("RGBA", (*atlas_rgb.split(), atlas_alpha))
    visible = atlas_alpha.point(lambda value: 255 if value > 0 else 0)
    preview = Image.composite(atlas, Image.new("RGBA", atlas.size), visible)
    preview.save(SNOW_CRYSTAL_PREVIEW, optimize=True)
    dark = Image.new("RGBA", atlas.size, (8, 12, 20, 255))
    dark.alpha_composite(atlas)
    dark.save(SNOW_CRYSTAL_DARK_PREVIEW, optimize=True)
    light = Image.new("RGBA", atlas.size, (218, 226, 232, 255))
    light.alpha_composite(atlas)
    light.save(SNOW_CRYSTAL_LIGHT_PREVIEW, optimize=True)
    # RGBA8 keeps the selected reference's fine internal veins. BC3 would turn
    # the low-alpha crystalline face into unstable 4x4 blocks at large scale.
    atlas.save(SNOW_CRYSTAL_DDS, format="DDS")


def verify(path: Path, expected_size: tuple[int, int]) -> tuple[int, int]:
    check = Image.open(path).convert("RGBA")
    if check.size != expected_size or check.getchannel("A").getextrema()[1] == 0:
        raise RuntimeError(f"DDS verification failed: {path}")
    return check.getchannel("A").getextrema()


def main() -> None:
    PREVIEW.parent.mkdir(parents=True, exist_ok=True)
    DDS.parent.mkdir(parents=True, exist_ok=True)
    prepare_mote_twinkle_atlas()
    prepare_single()
    prepare_sakura_material()
    prepare_star()
    prepare_atlas(LEAF_SOURCE, LEAF_PREVIEW, LEAF_DDS, "leaf", columns=8, rows=4)
    prepare_atlas(FEATHER_SOURCE, FEATHER_PREVIEW, FEATHER_DDS, "feather", columns=8, rows=4)
    prepare_black_feather_atlas()
    prepare_white_feather_material()
    prepare_pop_star_atlas()
    prepare_snow_crystal_atlas()
    print(f"motes={MOTE_PREVIEW}")
    print(f"motes_dds={MOTE_DDS} size=2048x256 alpha={verify(MOTE_DDS, (2048, 256))}")
    print(f"prepared={PREVIEW}")
    print(f"dds={DDS} size=256x256 alpha={verify(DDS, (256, 256))}")
    print(f"atlas={ATLAS_PREVIEW}")
    print(f"atlas_dds={ATLAS_DDS} size=1024x512 alpha={verify(ATLAS_DDS, (1024, 512))}")
    print(f"star={STAR_PREVIEW}")
    print(f"star_dds={STAR_DDS} size=256x256 alpha={verify(STAR_DDS, (256, 256))}")
    print(f"leaves={LEAF_PREVIEW}")
    print(f"leaves_dds={LEAF_DDS} size=2048x1024 alpha={verify(LEAF_DDS, (2048, 1024))}")
    print(f"feathers={FEATHER_PREVIEW}")
    print(f"feathers_dds={FEATHER_DDS} size=2048x1024 alpha={verify(FEATHER_DDS, (2048, 1024))}")
    print(f"black_feathers={BLACK_FEATHER_PREVIEW}")
    print(f"black_feathers_dds={BLACK_FEATHER_DDS} size=2048x1024 alpha={verify(BLACK_FEATHER_DDS, (2048, 1024))}")
    print(f"pop_stars={POP_STAR_PREVIEW}")
    print(f"pop_stars_dds={POP_STAR_DDS} size=2048x1024 alpha={verify(POP_STAR_DDS, (2048, 1024))}")
    print(f"snow_crystals={SNOW_CRYSTAL_PREVIEW}")
    print(f"snow_crystals_dark_preview={SNOW_CRYSTAL_DARK_PREVIEW}")
    print(f"snow_crystals_dds={SNOW_CRYSTAL_DDS} size=2048x1024 alpha={verify(SNOW_CRYSTAL_DDS, (2048, 1024))}")


if __name__ == "__main__":
    main()
