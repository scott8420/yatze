#!/usr/bin/env bash
# Yatze build. Deps (Ubuntu/Debian): libgtkmm-4.0-dev libspdlog-dev cmake g++.
set -e
cd "$(dirname "${BASH_SOURCE[0]}")"
cmake -B build
cmake --build build -- -j"$(nproc)"
echo ""
echo "Built: ./build/yatze  and  ./build/yatze_selftest"
echo "Smoke: ./build/yatze_selftest"
