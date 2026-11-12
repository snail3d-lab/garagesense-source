#!/usr/bin/env bash
# Pre-publish secret sweep — run this before you publish or re-share the tree.
#
# Text files are easy to sweep; binaries are not. The merged firmware image
# carries every compile-time default in its .rodata section, so building with
# a credentials.h full of real values leaves a real SSID, password, bot token
# and chat ID inside flasher/firmware/*.bin — invisible to a text-only grep.
# This script scans text AND binaries. Matches print redacted (class +
# location, never the secret itself), so its output is safe to paste anywhere.
#
# Two severities: [HIT] fails the scan (a real secret or a link that shouldn't
# be there); [note] is informational and never fails, because it is something
# you cannot avoid or need to fix (see home-build-path below).
#
#   ./tools/secret_scan.sh        exit 0 = publishable, exit 1 = fix first
set -uo pipefail
cd "$(dirname "$0")/.." || exit 1

HITS=$(mktemp); NOTES=$(mktemp); trap 'rm -f "$HITS" "$NOTES"' EXIT
EXCL=(--exclude-dir=.git --exclude-dir=.pio --exclude=secret_scan.sh)

# scan <label> <extended-regex> [extra grep flags/excludes...]
scan() {
  local label=$1 pat=$2; shift 2
  grep -ranoE "${EXCL[@]}" "$@" -e "$pat" . 2>/dev/null |
  while IFS=: read -r f l m; do
    printf '  [HIT] %-22s %s:%s  %s…\n' "$label" "${f#./}" "$l" "$(printf %s "$m" | cut -c1-6)" >> "$HITS"
  done
}

# note <label> <extended-regex> — same detection, informational only, and
# collapsed to one line per file so an unavoidable pattern can't bury a real hit.
note() {
  local label=$1 pat=$2; shift 2
  grep -raoE "${EXCL[@]}" "$@" -e "$pat" . 2>/dev/null | cut -d: -f1 | sort | uniq -c |
  while read -r n f; do
    printf '  [note] %-21s %s  %s match(es)\n' "$label" "${f#./}" "$n" >> "$NOTES"
  done
}

# Value-shaped secrets, any file type (grep -a treats binaries as text):
scan "telegram-bot-token"    '[0-9]{9,10}:[A-Za-z0-9_-]{30,}'
# Informational, never a finding: the Arduino core bakes the compiler's source
# path (__FILE__) into assert strings, so any PlatformIO-built image contains
# the folder it was built in — including Espressif's own CI paths in the same
# binary. It names a build machine, not a secret. Silence it by building under
# a neutral path (e.g. PLATFORMIO_CORE_DIR=/tmp/core) if you care who reads it.
note "home-build-path"       '/Users/[A-Za-z0-9._-]{2,}'
scan "donation-link"         '(buymeacoffee|ko[-.]fi|patreon|paypal\.me)\.[a-z]+/'
# Loose key="value" assignments; tools/mock_ui.py is exempt because its whole
# job is shipping fake config values for screenshots (its content is still
# scanned by the three strict patterns above).
scan "generic-secret-assign" '(password|passwd|secret|api[_-]?key|token)[[:space:]]*[=:][[:space:]]*["'"'"'][^"'"'"' ]{8,}' --exclude=mock_ui.py

# Your own credentials.h values, hunted even inside the shipped .bin.
# Values matching the example placeholders in credentials.h.example are fine.
if [ -f include/credentials.h ]; then
  for key in WIFI_SSID WIFI_PASSWORD TELEGRAM_BOT_TOKEN TELEGRAM_CHAT_ID; do
    val=$(sed -n "s/.*define[[:space:]]*$key[[:space:]]*\"\(.*\)\".*/\1/p" include/credentials.h)
    [ -n "$val" ] || continue
    case "$val" in your-*|123456*|ABC*) continue ;; esac
    while IFS= read -r f; do
      printf '  [HIT] %-22s %s:in-file  %s…\n' "credentials.h-value" "${f#./}" "$val" >> "$HITS"
    done < <(grep -raF "${EXCL[@]}" --exclude=credentials.h -l "$val" . 2>/dev/null)
  done
fi

if [ -s "$NOTES" ]; then
  sort -u "$NOTES"
  echo "  ($(wc -l < "$NOTES" | tr -d ' ') note(s) above — informational, publish anyway)"
fi
if [ -s "$HITS" ]; then
  sort -u "$HITS"
  echo "secret_scan: FINDINGS ABOVE — fix before publishing"
  exit 1
fi
echo "secret_scan: clean — no findings"
