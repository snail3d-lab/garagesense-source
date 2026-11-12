# GarageSense

A garage door monitor on a **Seeed XIAO ESP32S3 Sense**. It listens for the door
motor, waits ten seconds for the door to finish settling, then takes a photo and
sends it to Telegram. No cloud service, no AI model, no subscription — the
detection is signal processing on the board itself.

    motor detected ──► motor stops ──► +10 s ──► photo ──► Telegram

| | |
|---|---|
| Board | Seeed XIAO ESP32S3 **Sense** (OV2640 camera + PDM mic, 8 MB flash) |
| Firmware | Arduino C++ / PlatformIO, `seeed_xiao_esp32s3` |
| Config AP | `garagesense-config` → `http://garagesense.local` or `192.168.4.1` |
| OTA | `garagesense-<last 4 of MAC>.local`, password `garagesense` |
| Photo | Telegram `sendPhoto`, caption `🚪 Garage door heard <time>` |

## Why it is not a loudness trigger

A door and a transient sound are opposite shapes, so a burst detector cannot see
a door at all:

| | Transient (bark, clap) | Garage door |
|---|---|---|
| Duration | 50–380 ms | **2–20 s sustained motor** |
| Envelope | sharp attack, fast decay | slow ramp, plateau, abrupt stop |
| Repetition | repeats within seconds | **one event per cycle** |

`DoorDetector` (`include/door_detector.h`) looks for the opposite of a burst:
energy above the adaptive noise floor that **stays** there with low variance,
then stops. The pipeline is high-pass → per-frame RMS envelope → adaptive noise
floor → sustained check → end-of-motion check → duration sanity → optional
learned-template correlation.

## The ten-second delay

The motor stops when the door is *nearly* seated — the panel is still settling
and the car is still rolling in. So the event does not shoot immediately: it
**arms** a one-shot (`pendingPhotoAt`) that `loop()` fires `DOOR_PHOTO_DELAY_MS`
later. Capturing inline would block the audio loop.

## Learn mode — teach it *your* door

Chain drive and belt drive sound nothing alike. The recorder captures this
install's envelope so a lawnmower stops triggering it.

| Key | Action |
|---|---|
| `l` | start learning — run the door now; recording stops 1.2 s after the motor does |
| `L` | clear the learned template (back to "any door") |
| `s` | status — the `learned:` line shows sample count and whether matching is on |
| `t` | manual test photo (bypasses schedule and cooldown) |
| `c` | clear the cooldown |
| `i` | wifi info |
| `w` | wifi scan |
| `a` | reopen the config AP |
| `1` / `2` | nudge the threshold margin ±2 dB |

The template is stored in NVS (`tmpl` + `tmplLen`) and reloaded at boot, so it
survives a reflash that does not erase NVS. Matching is a normalized
cross-correlation of the envelopes resampled to 64 points, floor `DOOR_MATCH_MIN`
(0.55).

## Quiet hours

`daysMask` (bit0=Mon … bit6=Sun) and `hoursMask` (bit N = on during hour N), both
in NVS and both editable from the web UI. A real door event outside the window is
skipped; manual `t` bypasses it.

## Build

```bash
~/Library/Python/3.9/bin/pio run
```

`pio run -t merge` does not work on this project — hand-merge instead:

```bash
python3 -m esptool --chip esp32s3 merge_bin -o /tmp/garagesense.bin \
  0x0     .pio/build/seeed_xiao_esp32s3/bootloader.bin \
  0x8000  .pio/build/seeed_xiao_esp32s3/partitions.bin \
  0x10000 .pio/build/seeed_xiao_esp32s3/firmware.bin
```

## Flash

| Goal | Command |
|---|---|
| Keep WiFi + learned template | `write_flash 0x10000 firmware.bin` ← **app only** |
| Recover a bricked board | `write_flash 0x0 merged.bin` (then re-enter WiFi + re-learn) |

The merged image's `0x9000`–`0x10000` gap is `FF` filler, so flashing it at `0x0`
**erases NVS** — WiFi credentials and the learned door template. The browser
flasher therefore writes bootloader / partitions / app at their own offsets.

Build a released image with `include/credentials.h` **empty**: the image carries
every compile-time default in `.rodata`, so real values would ship inside the
`.bin`.

## Layout

```
src/main.cpp               mic pipeline, camera, Telegram, config AP + web UI,
                           watchdog, schedule, delayed capture, learn mode
include/door_detector.h    sustained-envelope detector + learned template
include/config.h           pin map + every tuning knob in one place
include/ui_page.h          the phone config UI, one self-contained page
flasher/                   browser flasher: page + bundled esptool + firmware
tools/mock_ui.py           run the config UI on a Mac, no hardware
tools/secret_scan.sh       pre-publish sweep: greps text AND the .bin
platformio.ini             the build, pinned to the one core version that does
                           not break the PDM mic driver
```

## License

See `LICENSE`.