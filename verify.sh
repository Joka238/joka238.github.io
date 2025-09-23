#!/usr/bin/env bash
set -euo pipefail
if grep -q '<script' index.html; then
  echo 'Fehler: index.html enthält <script>-Tags'; exit 1
fi
if grep -Eq 'data/' index.html; then
  echo 'Fehler: index.html referenziert data/*'; exit 1
fi
echo 'OK: index.html ist statisch und referenziert keine Daten.'
