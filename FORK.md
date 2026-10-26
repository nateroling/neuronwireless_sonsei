# Sonsei firmware: hold-on-other-key-press

A personal build of Dygma's Sonsei Neuron firmware, based on the official
`Sonsei_v1.1.0` tag, with one behavior change for tap-hold keys.

## What changes

Fast Superkeys (a Superkey with only TAP and HOLD set, where HOLD is a
modifier or a layer) normally become a hold when you hold them past the
Hold Timeout, or when you press **and release** another key inside them.

With this build, they become a hold as soon as you **press** another key
while holding them, the way QMK's `HOLD_ON_OTHER_KEY_PRESS` and ZMK's
"hold-preferred" work. Example, with TAP = Esc and HOLD = Ctrl:

| You do                               | Stock 1.1.0  | This build |
|--------------------------------------|--------------|------------|
| Hold Esc/Ctrl, tap `c`, release      | Ctrl+C       | Ctrl+C (sent sooner) |
| Hold Esc/Ctrl, press `c`, release Esc/Ctrl, then `c` | Esc, c | Ctrl+C |
| Tap Esc/Ctrl alone                   | Esc          | Esc        |
| Hold Esc/Ctrl alone past Hold Timeout | Ctrl        | Ctrl       |

Unchanged guards from Bazecor's Superkey settings:

- **Minimum Hold Time**: the key must be held at least this long before it can
  become a hold. If you release it sooner, the stock rules decide.
- **Minimum Prior Interval**: if you press the key right after typing a
  letter, it stays a tap.
- **Hold Timeout** still turns a key held on its own into a hold (useful
  for Cmd/Ctrl + mouse click).
- **Overlap Threshold** has no effect in practice: every press resolves the key at the moment of the press.

Superkeys with more than TAP and HOLD, and keys whose TAP is itself a
modifier, are not affected.

## Flashing

1. Get the latest build. Either open the `builds/hold-on-other-key-press`
   branch and choose **Code → Download ZIP**, or download the
   `Sonsei-v1.1.0-holdonpress-…` artifact from the latest successful run in
   the Actions tab. Unzip it. You need the folder that contains
   `Wireless_neuron.bin` and `keyscanner.bin`.
2. In Bazecor: **Firmware Update → Install a custom firmware → select folder**,
   choose that folder, **Add custom firmware**, then **Update now**.
3. Bazecor should report the firmware version as `v1.1.0+holdonpress`.

`keyscanner.bin` is the official 1.1.0 file, unmodified.

The settings layout is unchanged, so your layers and settings carry over in
both directions. Bazecor also backs them up before flashing
(Settings → Backups).

## Going back to stock

Flash the official firmware with Bazecor's normal Firmware Update.

## Turning it off without reflashing

The Focus command `qukeys.holdOnOtherKeyPress` reads the setting (`1` or
`0`), and `qukeys.holdOnOtherKeyPress 0` turns it off until the keyboard
restarts. The setting is not saved to memory yet.

## Layout of this repo

- `patches/kaleidoscope_adapter/` holds the change, as a patch to
  `libraries/kaleidoscope_adapter` (`plugins/QukeysDygma.{h,cpp}`). CI
  applies it at build time.
- `tools/qukeys-sim/` is a host-side test. It compiles the real decision
  code against stubs and checks timed key sequences with the setting on and off.
- `tools/check_seal.py` runs the same image checks Bazecor runs before it
  flashes, plus a comparison with the official image.
- `.github/workflows/build.yml` builds, tests, checks and packages the firmware.
  The flashable folder is also published to the `builds/<branch>` branch.
