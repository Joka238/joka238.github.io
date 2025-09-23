#!/usr/bin/env bash
set -euo pipefail
grep -q '<script' index.html && { echo 'Fehler: index.html enthält <script>'; exit 1; } || true
grep -Eq 'data/' index.html && { echo 'Fehler: index.html referenziert data/*'; exit 1; } || true
[ -f .nojekyll ] || { echo 'Fehler: .nojekyll fehlt'; exit 1; }
echo 'OK: Statische Bridge-Dateien sind sauber.'
