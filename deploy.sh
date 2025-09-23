#!/usr/bin/env bash
set -euo pipefail

REPO_SSH="${1:-git@github.com:joka238/joka238.github.io.git}"
TMP="$(mktemp -d)"
echo "[*] Klone ${REPO_SSH} nach ${TMP}"
git clone -b main "${REPO_SSH}" "${TMP}/repo"
cd "${TMP}/repo"

echo "[*] Kopiere Bridge-Dateien an Repo-Root"
# Pfad zu diesem Skript-Verzeichnis ermitteln
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cp -av "${SCRIPT_DIR}/index.html" "${SCRIPT_DIR}/style.css" "${SCRIPT_DIR}/verify.sh" .
mkdir -p .github/workflows
cp -av "${SCRIPT_DIR}/.github/workflows/pages.yml" .github/workflows/
cp -av "${SCRIPT_DIR}/.nojekyll" .

echo "[*] Commit & Push"
git add index.html style.css verify.sh .nojekyll .github/workflows/pages.yml
git commit -m "telepaint: seed bridge + pages workflow" || echo "[i] Nichts zu committen."
git push

echo "[*] Fertig. Öffne Settings → Pages und prüfe den neuen Deployment-Run unter Actions."
