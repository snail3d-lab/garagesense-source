#pragma once

// ============================================================
// GARAGECAM — XIAO ESP32S3 Sense pin map + tuning knobs
// Pin map for the Seeed XIAO ESP32S3 Sense.
// ============================================================

// Shown in the config UI and attached to "Report an issue" submissions.
#define FW_VERSION "1.6"

// --- microphone (PDM, via I2S) — do NOT reuse these pins ---
#define MIC_PDM_CLK   42
#define MIC_PDM_DATA  41

// --- camera (DVP parallel) — do NOT reuse these pins ---
#define CAM_PIN_XCLK   10
#define CAM_PIN_PCLK   13
#define CAM_PIN_VSYNC  38
#define CAM_PIN_HREF   47
#define CAM_PIN_SDA    40   // SCCB (SIOD)
#define CAM_PIN_SCL    39   // SCCB (SIOC)
#define CAM_PIN_D0     15
#define CAM_PIN_D1     17
#define CAM_PIN_D2     18
#define CAM_PIN_D3     16
#define CAM_PIN_D4     14
#define CAM_PIN_D5     12
#define CAM_PIN_D6     11
#define CAM_PIN_D7     48

// --- misc ---
#define LED_PIN        21   // USER LED, active-LOW

// If photos come out upside-down / mirrored, flip these and reflash.
#define CAM_VFLIP      1
#define CAM_HMIRROR    0
#define CAM_JPEG_QUALITY 12 // lower = sharper but bigger file (5–30)
// AEC settle: grab-and-discard this many frames right before a capture so the
// sensor's auto-exposure converges on the current scene (fixed window-facing
// camera, light varies all day).
#define AEC_SETTLE_FRAMES  4

// --- audio / frame analysis tuning ---
#define SAMPLE_RATE          16000
#define FRAME_SAMPLES        256     // 16 ms per analysis frame
#define NOISE_FLOOR_DB       -60.0f  // never track noise below this
#define NOISE_RISE_COEF      0.10f   // per quiet frame, how fast the floor rises
#define NOISE_FALL_COEF      0.01f   // per quiet frame, how fast it falls
#define THRESHOLD_MARGIN_DB  10.0f   // door threshold = noise floor + margin

// --- behavior ---
#define COOLDOWN_MS          90000   // one photo per door cycle; 90 s also debounces a
                                     // double-trigger (open then close within a minute)
// --- door episode bookkeeping ---
// A door cycle is a single sustained event, so an episode never repeats. The
// shared main.cpp still tracks episodes; these keep that path harmless.
#define EPISODE_REPEAT_MS    8000    // unused for doors (kept so the shared code compiles)
#define EPISODE_MAX          1       // ONE photo per door cycle — the door only moves once
#define EPISODE_GAP_MS       12000   // quiet this long => the episode has ended

// --- garage door detection ---
// A sectional door takes 10-15 s to travel, so the motor envelope is 2-20 s of
// sustained energy. These are the values the bark detector could never see.
#define DOOR_HP_CUTOFF_HZ       120.0f   // low-pass-ish: keep motor hum, drop HVAC rumble
#define DOOR_THRESHOLD_MARGIN_DB 10.0f   // door motor is steady and clear; less margin than a bark
#define DOOR_MIN_MS             1500     // shorter than this is a knock, not a door
#define DOOR_MAX_MS             25000    // longer means a truck idling, not a cycle
#define DOOR_END_QUIET_MS       1200     // energy gone this long => the cycle has ended
#define DOOR_MAX_VARIANCE       30.0f    // steady hum; voices/slams swing far more
#define DOOR_MATCH_MIN          0.55f    // template correlation floor (0..1)

// --- the photo delay ---
// The motor stops when the door is *nearly* seated; the panel is still settling.
// Wait this long after the cycle ends so the shot shows a fully-open door and
// the car is actually in/out. Eric: "the picture is taken because you have to
// figure the garage door has to be up for the photo to be a good photo."
#define DOOR_PHOTO_DELAY_MS     10000    // 10 s after the motor stops
// --- quiet-hours schedule (day mask + per-hour on/off mask) ---
// daysMask bits: 0=Mon 1=Tue 2=Wed 3=Thu 4=Fri 5=Sat 6=Sun.
// hoursMask bits 0..23: bit N set = the device is ON during hour N (N=0 is
// midnight). Default is all 24 bits set = always on; the UI shows every hour
// lit and a tap turns that hour off (quiet).
#define SCHED_DAYS_DEFAULT   0x7F       // all seven days (always on)
#define SCHED_HOURS_DEFAULT  0xFFFFFFu  // all 24 hours on (always on by default)
#define WIFI_TIMEOUT_MS      20000
// Must outlast the worst un-ticked path in loop(): camera capture (~3 s) +
// a Telegram send where every HTTP stage hits its 15 s timeout (~45 s).
// A truly stuck loop still reboots within 90 s.
#define WATCHDOG_TIMEOUT_MS  90000
#define PERIODIC_REBOOT_MS   21600000ULL // reboot every 6 h — keeps heap fresh for unattended duty

// Your UTC offset in hours (e.g. -7 for Pacific). Used for photo captions.
#define TZ_OFFSET_HOURS      0

// Bump for each release — shown in the serial banner ("fw %d").
#define FIRMWARE_VERSION 13

// --- config access point (open AP, first N minutes after power-on) ---
#define AP_SSID          "garagesense-config"
#define AP_WINDOW_MS     600000  // 10 min window, then drop the AP (board stays on home WiFi)

// --- camera orientation setting (0=none 3=180; 90° not supported by this camera) ---
#define CAM_ROTATE_DEFAULT 0     // upright by default for a shell mounted the right way up; set 3 if yours is upside-down
#define CAM_MIRROR_DEFAULT 1     // 1 = horizontal mirror. Eric's install reads reversed without it (2026-11-10).
// --- camera exposure setting (0=dim 1=medium 2=bright) ---
#define CAM_EXPOSURE_DEFAULT 0     // DIM. Eric's install is bright/window-lit and 1 blew out highlights (2026-11-10).
