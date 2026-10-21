#!/usr/bin/env bash
# Yatze build. Deps (Fedora): gtkmm4.0-devel spdlog-devel libepoxy-devel cmake gcc-c++.
set -e
cd "$(dirname "${BASH_SOURCE[0]}")"
cmake -B build
cmake --build build -- -j"$(nproc)"
echo ""
echo "Built: ./build/yatze  and  ./build/yatze_selftest"
echo "Smoke: ./build/yatze_selftest"
