#!/bin/bash
set -e
# fail if any script tag sneaks in
if grep -Riq '<script' index.html 404.html; then
  echo 'Fehler: Script-Tags gefunden.'; exit 1; fi
# fail if index tries to reference /data or similar
if grep -RiqE 'src=["\x27]?data/|href=["\x27]?data/' index.html; then
  echo 'Fehler: Verbotene Daten-Referenzen.'; exit 1; fi
echo 'GitHub Pages Bridge geprüft: keine Skripte, keine Datenreferenzen.'
