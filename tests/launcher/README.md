# Launcher regression tests

Run from the repository root:

```sh
python3 -B -m unittest discover -s tests/launcher -v
```

The tests use disposable directories and stub Wine, window inspection, process
termination and sleeps. No Adobe application starts, no real process is killed,
and the user's prefix, desktop entries and MIME preferences are untouched.

Existing application windows must receive files without the ten-second exit
grace period. Windowless processes retain that grace period and must still have
no window before they can be terminated. Failed window inspection must never
authorize termination.

With `desktop-file-utils` and Python's PyGObject installed, the suite also checks
MIME discovery and dispatches a synthetic `misterhorsepm:` callback through
GIO's desktop-entry parser to stub Wine. It covers custom prefixes, quoting,
idempotent registration, the configured Flatpak command and launcher removal.
It does not start a browser, contact Mister Horse or run a Flatpak sandbox.
