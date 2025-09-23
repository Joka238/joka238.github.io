#!/bin/bash
set -e
if grep -qE 'src=["\x27]data/' web/index.html || grep -qE 'href=["\x27]data/' web/index.html; then
  echo "Fehler: index.html referenziert Daten unter data/*."
  exit 1
fi
if grep -q '<script' web/index.html; then
  echo "Fehler: index.html enthält ein <script>-Tag."
  exit 1
fi
echo "Web Bridge HTML überprüft: Keine unerwünschten Datenreferenzen oder Skripte gefunden."
