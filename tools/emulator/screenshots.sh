#!/usr/bin/env bash
# Store-Screenshots einer Plattform aufnehmen (Englisch, Demo-Daten).
#
#   wsl -d Pebble -u root bash tools/emulator/screenshots.sh emery [zielordner]
#
# Baut mit STAMINA_DEMO=1 (Beispieldaten), stellt die App auf Englisch und
# fotografiert fünf Ansichten. Vor jedem Bild wird die App neu gestartet,
# damit sich die Menüpositionen nicht verschieben.
set -e
cd "$(dirname "$0")/../.."
P=${1:-emery}
OUT=${2:-store/screenshots/$P}
DATA=$(mktemp -d)
export STAMINA_DEMO=1 PATH="$HOME/.local/bin:$PATH" PYTHONWARNINGS=ignore
mkdir -p "$OUT"
python3 tools/emulator/make_testdata.py "$DATA" >/dev/null

q() { "$@" 2>&1 | grep -v -E 'SyntaxWarning|^\s*"""|KiB/s|^N/A' || true; }
key() { python3 -c "import json;print(json.load(open('build/js/message_keys.json'))['$1'])"; }
E="--emulator $P"
shot() { q pebble screenshot $E --no-open "$OUT/$1.png" >/dev/null; echo "$P $1"; }
btn() { q pebble emu-button $E click "$@" >/dev/null; sleep 1; }
restart() { q pebble install $E >/dev/null; sleep 5; }

pebble build 2>&1 | grep -E 'error|finished' | grep -v RWX
q pebble kill; q pebble wipe
restart
q pebble send-app-message $E --app-uuid ca26453c-432f-4f97-94a6-b2afac7fb0c9 \
  --int "$(key LANGUAGE)=1" "$(key SEX)=0"

# Menüzeilen: 0 Solo, 1 Partner, 2 Verlauf, 3 Statistik, 4 Diagramme, 5 Erfolge, 6 Anlernen, 7 Einstellungen
restart; btn down -n 2; btn select; btn down -n 2; btn select; shot 02_session_detail
restart; btn down -n 4; btn select; shot 03_charts
restart; btn down -n 5; btn select; shot 04_achievements
restart; btn down -n 3; btn select; shot 05_stats_sleep
restart
q pebble emu-heart-rate $E 121
btn down; btn select; btn select                      # Partner -> Partner wählen
q pebble emu-accel $E custom "$DATA/posA.txt"
for _ in 1 2 3 4 5 6; do q pebble emu-heart-rate $E 121; sleep 1.5; done
sleep 2; shot 01_session
rm -rf "$DATA"
echo "fertig: $OUT"
