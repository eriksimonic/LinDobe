# Launcher icon regression tests

Run from the repository root with Python 3 and Pillow installed:

```sh
python3 -B -m unittest discover -s tests/launcher-icons -v
```

All resources are generated in temporary directories. No Adobe binaries or
artwork are included. The fixtures reproduce 7-Zip's bitmap ICO wrappers with
zero planes/bit depth, PNG resources without filename extensions, multiple icon
groups, and localized resources.

The launcher selects the first icon group in resource order (named groups before
numeric IDs), then the largest frame inside that group. It does not select the
largest image across unrelated tool/document icons. Neutral language resources
are preferred when present. An adjacent `*Application_Icon*.ico` is a fallback;
failed extraction leaves an existing PNG intact.

Windows icon index zero means the first icon, as documented in
[ExtractIconExW](https://learn.microsoft.com/en-us/windows/win32/api/shellapi/nf-shellapi-extracticonexw).
The reconstruction follows Microsoft's explanation of
[icon resource groups](https://devblogs.microsoft.com/oldnewthing/20120720-00/?p=7083).
It preserves the group directory's color depths, which are absent from 7-Zip's
individual ICO wrappers and are needed to decode bitmap alpha correctly.

To validate against a locally installed application without changing launchers:

```sh
python3 scripts/extract-app-icon.py '/path/to/Application.exe' /tmp/application-icon.png
```

Compare the output to the application's default icon. To exercise the full
launcher generator, set `XDG_DATA_HOME` and `ADOBEWINE_HOME` to disposable
directories and `WINEPREFIX` to the existing prefix. Run twice and compare the
PNG hashes to check regeneration. Do not commit extracted Adobe icons.
