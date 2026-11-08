# Mister Horse acceptance project

Run these ExtendScript files only in the isolated test prefix, using the
existing `adobewine run` command and After Effects' `-r` argument.

1. Open an empty project and run `setup.jsx`. It creates a three-second,
   1280x720 composition with one selected text layer and saves the project
   under the prefix's temporary directory, in `lindobe-misterhorse-test`.
2. Open Animation Composer and manually apply a free animation to the selected
   `Animation target` layer. Check playback and interaction in the panel.
3. Run `verify.jsx`. It requires the test project and an empty render queue,
   checks that animation keyframes or expressions exist, saves and reopens
   the project, and verifies that the animation structure persists.
4. The script renders the three-second composition using the `Lossless`
   output module template and writes `verification.txt`. It does not change
   another queued render. The output is `misterhorse-test.avi` in the same
   temporary test directory. Independently inspect its duration and frames
   with a media metadata tool such as ffprobe.

Preserve the previous test AVI before repeating verification, so After Effects
does not stop at an overwrite confirmation. Do not remove unrelated renders.

The scripts do not install plugins, enter credentials, or automate the plugin
panel. Applying the preset and checking playback remain manual acceptance
steps. Missing the named render template is an explicit error, not a reason
to select an unrelated output format automatically.

The final Wine 11.20 development-module run with Animation Composer 4.2.6 and
After Effects 26.5.0.89 reports save/reopen and render PASS, with three enabled
expressions. The output contains 72 frames, 21 distinct frame hashes, and the
same per-frame hashes as the earlier render. The user confirms stable viewer
display and normal panel mouse interaction with patch 0136.
