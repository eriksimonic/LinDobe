#!/usr/bin/env python3
"""Extract the default executable icon using 7-Zip and Pillow.

Select the first RT_GROUP_ICON (the default Windows icon), rather than the
largest unrelated RT_ICON. Rebuild its ICO directory with the group's color
depths: 7-Zip's individual ICO wrappers omit them, which can lose alpha.
"""

import argparse
from io import BytesIO
from pathlib import Path
import struct
import subprocess
import sys
import tempfile

from PIL import Image


def resource_key(path):
    # PE resource directories put named entries before numeric IDs. Prefer the
    # neutral language, then the lowest language ID, for the same icon group.
    name = path.name
    name_key = (1, int(name)) if name.isdecimal() else (0, name)
    language = path.parent.parent.name
    language_key = int(language) if language.isdecimal() else -1
    return name_key, language_key


def group_ico(group):
    """Reconstruct an ICO using a raw GROUP_ICON and 7-Zip's ICON wrappers."""
    data = group.read_bytes()
    reserved, kind, count = struct.unpack_from('<HHH', data)
    if reserved or kind != 1 or not count or len(data) != 6 + 14 * count:
        raise ValueError('invalid icon group')
    directory, images = [], []
    offset = 6 + 16 * count
    for index in range(count):
        width, height, colors, reserved, planes, depth, size, resource = (
            struct.unpack_from('<BBBBHHIH', data, 6 + index * 14)
        )
        icon_path = group.parent.parent / 'ICON' / str(resource)
        if icon_path.is_file():
            # PNG-compressed RT_ICON resources are extracted without a wrapper
            # or an extension by 7-Zip.
            payload = icon_path.read_bytes()
            if len(payload) != size:
                raise ValueError('truncated icon resource')
        else:
            wrapper = icon_path.with_suffix('.ico').read_bytes()
            # 7-Zip wraps bitmap RT_ICONs in single-image ICOs. Discard the
            # synthetic directory and retain the original group metadata.
            if struct.unpack_from('<HHH', wrapper) != (0, 1, 1):
                raise ValueError('invalid individual icon')
            image_size, image_offset = struct.unpack_from('<II', wrapper, 14)
            payload = wrapper[image_offset:image_offset + image_size]
            if len(payload) != size or image_size != size:
                raise ValueError('truncated or mismatched icon resource')
        directory.append(struct.pack('<BBBBHHII', width, height, colors,
                                     reserved, planes, depth, size, offset))
        images.append(payload)
        offset += size
    return struct.pack('<HHH', 0, 1, count) + b''.join(directory + images)


def decode_icon(data):
    with Image.open(BytesIO(data)) as image:
        # Pillow resolves equal-size entries by color depth. Do not assign
        # Image.size manually: request the actual frame from the ICO decoder.
        size = max(image.ico.sizes(), key=lambda size: (size[0] * size[1], size))
        return image.ico.getimage(size).convert('RGBA')


def select_icon(resources, executable):
    groups = sorted(resources.glob('.rsrc/**/GROUP_ICON/*'), key=resource_key)
    if groups:
        try:
            return decode_icon(group_ico(groups[0]))
        except (OSError, ValueError, struct.error) as error:
            print(f'Cannot decode default icon of {executable.name}: {error}',
                  file=sys.stderr)
    # An explicitly named application icon is a useful fallback; arbitrary
    # large ICON resources might be document/tool icons and are not suitable.
    for path in sorted(executable.parent.glob('*Application_Icon*.ico')):
        try:
            return decode_icon(path.read_bytes())
        except (OSError, ValueError, struct.error):
            continue
    raise ValueError('no usable default or Application_Icon icon')


def extract_icon(executable, destination):
    with tempfile.TemporaryDirectory(prefix='lindobe-icon-') as temporary:
        resources = Path(temporary)
        try:
            subprocess.run(
                ['7z', 'x', '-y', f'-o{resources}', str(executable.resolve()),
                 '.rsrc/ICON/*', '.rsrc/*/ICON/*',
                 '.rsrc/GROUP_ICON/*', '.rsrc/*/GROUP_ICON/*'],
                stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, check=True,
            )
        except (OSError, subprocess.CalledProcessError) as error:
            print(f'Cannot extract resources from {executable.name}: {error}',
                  file=sys.stderr)
        image = select_icon(resources, executable)
        image.thumbnail((256, 256), Image.Resampling.LANCZOS)
        # Keep any existing PNG intact if extraction or conversion fails.
        with tempfile.NamedTemporaryFile(dir=destination.parent, suffix='.png',
                                         delete=False) as output:
            temporary_png = Path(output.name)
        try:
            image.save(temporary_png, format='PNG')
            temporary_png.replace(destination)
        finally:
            temporary_png.unlink(missing_ok=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('executable', type=Path)
    parser.add_argument('destination', type=Path)
    args = parser.parse_args()
    try:
        extract_icon(args.executable, args.destination)
    except (OSError, ValueError, struct.error) as error:
        print(f'Icon unchanged for {args.executable.name}: {error}', file=sys.stderr)
        return 1
    return 0


if __name__ == '__main__':
    sys.exit(main())
