# Buyer notes — GarageSense

Email your GitHub username to snailmail3d@gmail.com and you'll be added to this
repository, usually within a day.

## Start here

1. Get the board: **Seeed XIAO ESP32S3 Sense** — the *Sense* version, with the
   camera + mic expansion and an OV2640. A plain XIAO ESP32S3 has no camera or
   mic and can't do this. About $14–20 shipped.
2. Flash it: `pio run -t upload` over USB (no button pressing), or serve the
   [`flasher/`](flasher/) folder and flash from Chrome — see
   [`flasher/README.md`](flasher/README.md).
3. Make a Telegram bot with @BotFather, grab your numeric user ID from
   @userinfobot, press **Start** in the bot chat.
4. Mount the board next to the opener, join its `garagesense-config` WiFi for ten
   minutes, open `garagesense.local` on your phone, fill in WiFi + bot token +
   user ID, hit Save.
5. Teach it the door: open the serial console and press `l`, then run the door
   once. It records this door's motor and stops matching other noises.

## What's in the box

```
src/main.cpp                the firmware: mic pipeline, camera, Telegram,
                            config AP + web UI, watchdog, schedule
include/door_detector.h     the door detector — high-pass, noise floor,
                            sustained envelope, learned template
include/config.h            pin map + every tuning knob in one place
include/ui_page.h           the phone config UI, one self-contained page
flasher/                    your own browser flasher (page + esptool + firmware)
tools/mock_ui.py            run the config UI on your Mac, no hardware
tools/secret_scan.sh        pre-publish sweep: greps text AND the .bin for
                            leaked credentials (scan output is redacted)
platformio.ini              the build, pinned to the one core version that
                            doesn't break the PDM mic driver
```

## Your Telegram bill, honestly

Telegram's Bot API is free and GarageSense sends a few kilobytes per door cycle.
You need no account with anyone, including me — the bot is yours, the photo path
is board → Telegram → your phone. Nothing routes through a server of mine, and
there's nothing to subscribe to.

## Placement

WiFi at the opener is usually fine. If the board reports `ip=0.0.0.0` on serial
(`i`), it is too far from the access point — move it a few feet. Modem sleep is
already off and `ensureWifi()` re-tries every 30 s.

## Updates

Every update lands here — `git pull`. If you bought before an update and it's
material, you'll get an email too.

## Questions

snailmail3d@gmail.com — real human, answers within a day or two.