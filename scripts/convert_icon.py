import os
import io
import sys
import argparse

USAGE = 'Usage: python scripts/convert_icon.py input.png|input.svg output_name width height'

def svg_to_png_bytes(svg_path, width, height):
    import cairosvg

    with open(svg_path, 'rb') as f:
        svg_data = f.read()
    png_bytes = cairosvg.svg2png(bytestring=svg_data, output_width=width, output_height=height)
    return png_bytes

def load_image(path, width, height, fit_contain=False, rotate=True):
    from PIL import Image

    ext = os.path.splitext(path)[1].lower()
    if ext == '.svg':
        png_bytes = svg_to_png_bytes(path, width, height)
        img = Image.open(io.BytesIO(png_bytes))
    else:
        img = Image.open(path)
        img = img.convert('RGBA')
        if fit_contain:
            scale = min(width / img.width, height / img.height)
            resized_size = (max(1, round(img.width * scale)), max(1, round(img.height * scale)))
            resized = img.resize(resized_size, Image.Resampling.LANCZOS)
            img = Image.new('RGBA', (width, height), (255, 255, 255, 255))
            img.alpha_composite(resized, ((width - resized.width) // 2, (height - resized.height) // 2))
        else:
            img = img.resize((width, height), Image.Resampling.LANCZOS)
        # Flatten alpha: paste on white background
        background = Image.new('RGBA', img.size, (255, 255, 255, 255))
        background.paste(img, mask=img.split()[3])
        img = background
    if rotate:
        img = img.rotate(90, expand=True)
    return img

def image_to_c_array(img, array_name, threshold, emit_dimensions=False):
    # Convert to grayscale, then threshold to get white=1, black=0
    # Convert to grayscale
    img = img.convert('L')
    width, height = img.size
    pixels = list(img.getdata())
    packed = []
    for y in range(height):
        for x in range(0, width, 8):
            byte = 0
            for b in range(8):
                if x + b < width:
                    v = pixels[y * width + x + b]
                    # 1 for white, 0 for black
                    bit = 1 if v >= threshold else 0
                    byte |= (bit << (7 - b))
            packed.append(byte)
    # Format as C array
    c = '#pragma once\n#include <cstdint>\n\n'
    c += f'// size: {width}x{height}\n'
    if emit_dimensions:
        c += f'static constexpr uint16_t {array_name}Width = {width};\n'
        c += f'static constexpr uint16_t {array_name}Height = {height};\n\n'
    c += f'static const uint8_t {array_name}[] = {{\n    '
    for i, v in enumerate(packed):
        c += f'0x{v:02X}, '
        if (i + 1) % 16 == 0:
            c += '\n    '
    c = c.rstrip(', \n') + '\n};\n'
    return c

def write_preview(img, threshold, screen_size, output_path, title, status, version,
                  title_font_path, small_font_path, title_size, small_size):
    from PIL import Image, ImageDraw, ImageFont

    screen_width, screen_height = screen_size
    preview = Image.new('L', (screen_width, screen_height), 255)
    monochrome = img.convert('L').point(lambda value: 255 if value >= threshold else 0)
    x = (screen_width - img.width) // 2
    y = (screen_height - img.height) // 2
    preview.paste(monochrome, (x, y))

    title_font = ImageFont.truetype(title_font_path, title_size)
    small_font = ImageFont.truetype(small_font_path, small_size)
    draw = ImageDraw.Draw(preview)
    draw.text((screen_width // 2, y + img.height + 70), title, font=title_font, fill=0, anchor='mt')
    draw.text((screen_width // 2, y + img.height + 95), status, font=small_font, fill=0, anchor='mt')
    draw.text((screen_width // 2, screen_height - 30), version, font=small_font, fill=0, anchor='mt')

    os.makedirs(os.path.dirname(output_path), exist_ok=True)
    preview.save(output_path, format='PNG')


def main():
    parser = argparse.ArgumentParser(description='Convert PNG/SVG/JPEG icons to 1-bpp C arrays.')
    parser.add_argument('input_path')
    parser.add_argument('output_name')
    parser.add_argument('width', type=int)
    parser.add_argument('height', type=int)
    parser.add_argument('--fit-contain', action='store_true', help='Preserve aspect ratio and center on white canvas.')
    parser.add_argument('--no-rotate', action='store_true', help='Do not apply the legacy 90-degree rotation.')
    parser.add_argument('--threshold', type=int, default=128, help='Grayscale cutoff from 0 to 255.')
    parser.add_argument('--header-output', help='Override the default generated-header output path.')
    parser.add_argument('--array-name', help='Override the generated C array name.')
    parser.add_argument('--emit-dimensions', action='store_true', help='Emit constexpr width and height beside the array.')
    parser.add_argument('--preview', help='Optional full-screen boot preview PNG output path.')
    parser.add_argument('--screen-size', help='Preview screen dimensions as WIDTHxHEIGHT.')
    parser.add_argument('--preview-title', default='CrossPoint')
    parser.add_argument('--preview-status', default='BOOTING')
    parser.add_argument('--preview-version', default='')
    parser.add_argument('--preview-title-font')
    parser.add_argument('--preview-small-font')
    parser.add_argument('--preview-title-size', type=int, default=21)
    parser.add_argument('--preview-small-size', type=int, default=17)
    args = parser.parse_args()

    if not 0 <= args.threshold <= 255:
        parser.error('--threshold must be between 0 and 255')
    if args.preview and (not args.screen_size or not args.preview_title_font or not args.preview_small_font):
        parser.error('--preview requires --screen-size, --preview-title-font, and --preview-small-font')

    array_name = args.array_name or args.output_name.capitalize() + 'Icon'
    img = load_image(args.input_path, args.width, args.height,
                     fit_contain=args.fit_contain, rotate=not args.no_rotate)
    c_array = image_to_c_array(img, array_name, args.threshold, args.emit_dimensions)

    project_root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    output_path = args.header_output or os.path.join(project_root, 'src', 'components', 'icons', f'{args.output_name}.h')
    os.makedirs(os.path.dirname(output_path), exist_ok=True)
    with open(output_path, 'w') as f:
        f.write(c_array)
    print(f'Wrote {output_path}')

    if args.preview:
        screen_width, screen_height = map(int, args.screen_size.lower().split('x', 1))
        write_preview(img, args.threshold, (screen_width, screen_height), args.preview,
                      args.preview_title, args.preview_status, args.preview_version,
                      args.preview_title_font, args.preview_small_font,
                      args.preview_title_size, args.preview_small_size)
        print(f'Wrote {args.preview}')

if __name__ == '__main__':
    main()
