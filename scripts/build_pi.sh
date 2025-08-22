#!/usr/bin/env bash
set -euo pipefail
sudo apt update
sudo apt install -y cmake g++ libsdl2-dev libcurl4-openssl-dev libjson-c-dev libssl-dev
mkdir -p build && cd build
cmake ..
make -j$(nproc)
echo "Run with: ./telepaint ../config.json"
