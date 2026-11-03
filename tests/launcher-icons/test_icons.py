"""Synthetic regression fixtures; no Adobe executables or icons are included."""
import importlib.util
from io import BytesIO
from pathlib import Path
import struct
import tempfile
import unittest
from unittest.mock import patch

from PIL import Image

spec = importlib.util.spec_from_file_location(
    'extract_app_icon', Path(__file__).resolve().parents[2] / 'scripts/extract-app-icon.py')
icons = importlib.util.module_from_spec(spec)
spec.loader.exec_module(icons)


def add_group(root, name, color, size=32, language=None, png=False):
    resources = root / '.rsrc'
    if language is not None:
        resources /= str(language)
    (resources / 'GROUP_ICON').mkdir(parents=True, exist_ok=True)
    (resources / 'ICON').mkdir(exist_ok=True)
    image = Image.new('RGBA', (size, size), color)
    data = BytesIO()
    if png:
        image.save(data, format='PNG')
        payload = data.getvalue()
        (resources / 'ICON' / f'{name}').write_bytes(payload)
    else:
        image.save(data, format='ICO', sizes=[(size, size)], bitmap_format='bmp')
        original = data.getvalue()
        length, offset = struct.unpack_from('<II', original, 14)
        payload = original[offset:offset + length]
        # Reproduce 7-Zip's wrapper: planes and bit depth are both zero.
        wrapper = struct.pack('<HHHBBBBHHII', 0, 1, 1, size % 256,
                              size % 256, 0, 0, 0, 0, len(payload), 22)
        (resources / 'ICON' / f'{name}.ico').write_bytes(wrapper + payload)
    group = resources / 'GROUP_ICON' / str(name)
    group.write_bytes(struct.pack('<HHHBBBBHHIH', 0, 1, 1, size % 256,
                                  size % 256, 0, 0, 1, 32, len(payload), int(name)))
    return group


class IconTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)
        self.executable = self.root / 'App.exe'

    def test_default_group_wins_over_larger_tool_icon(self):
        add_group(self.root, 1001, (0, 255, 0, 255), size=256)
        add_group(self.root, 1000, (255, 0, 0, 255))
        image = icons.select_icon(self.root, self.executable)
        self.assertEqual(image.size, (32, 32))
        self.assertEqual(image.getpixel((0, 0)), (255, 0, 0, 255))

    def test_group_bit_depth_restores_alpha(self):
        group = add_group(self.root, 100, (20, 40, 80, 64))
        image = icons.decode_icon(icons.group_ico(group))
        self.assertEqual(image.getpixel((0, 0)), (20, 40, 80, 64))

    def test_largest_frame_within_selected_icon(self):
        image = Image.new('RGBA', (256, 256), (20, 40, 80, 255))
        data = BytesIO()
        image.save(data, format='ICO', sizes=[(16, 16), (32, 32), (256, 256)])
        self.assertEqual(icons.decode_icon(data.getvalue()).size, (256, 256))

    def test_png_resource_without_extension(self):
        add_group(self.root, 101, (20, 40, 80, 64), size=256, png=True)
        image = icons.select_icon(self.root, self.executable)
        self.assertEqual(image.size, (256, 256))
        self.assertEqual(image.getpixel((0, 0)), (20, 40, 80, 64))

    def test_named_group_and_neutral_language_order(self):
        first = add_group(self.root, 0, (255, 0, 0, 255), language=0)
        first.rename(first.with_name('MAIN'))
        second = add_group(self.root, 0, (0, 255, 0, 255), language=1033)
        second.rename(second.with_name('MAIN'))
        add_group(self.root, 1, (0, 0, 255, 255))
        self.assertEqual(icons.select_icon(self.root, self.executable).getpixel((0, 0)),
                         (255, 0, 0, 255))

    def test_application_icon_fallback(self):
        image = Image.new('RGBA', (32, 32), (20, 40, 80, 255))
        image.save(self.root / 'App_Application_Icon.ico', sizes=[(32, 32)])
        with patch.object(icons.subprocess, 'run', side_effect=FileNotFoundError):
            icons.extract_icon(self.executable, self.root / 'output.png')
        with Image.open(self.root / 'output.png') as result:
            self.assertEqual(result.getpixel((0, 0)), (20, 40, 80, 255))

    def test_failed_extraction_preserves_existing_png(self):
        target = self.root / 'output.png'
        target.write_bytes(b'existing custom icon')
        with patch.object(icons.subprocess, 'run', side_effect=FileNotFoundError):
            with self.assertRaises(ValueError):
                icons.extract_icon(self.executable, target)
        self.assertEqual(target.read_bytes(), b'existing custom icon')

    def test_truncated_resource_rejected(self):
        group = add_group(self.root, 100, (20, 40, 80, 64))
        (group.parent.parent / 'ICON' / '100.ico').write_bytes(b'broken')
        with self.assertRaises((ValueError, struct.error)):
            icons.group_ico(group)


if __name__ == '__main__':
    unittest.main()
