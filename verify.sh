#!/usr/bin/env bash
set -euo pipefail
! grep -q '<script' web/index.html
! grep -Eq 'data/' web/index.html
echo "OK: sauber"
