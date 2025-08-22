#!/usr/bin/env bash
set -euo pipefail
brew install cmake sdl2 json-c openssl || true
mkdir -p build && cd build
# Pass OpenSSL root so find_package works everywhere
cmake -DOPENSSL_ROOT_DIR=$(brew --prefix openssl) ..
make -j4
echo "Run with: ./telepaint ../config.json"
