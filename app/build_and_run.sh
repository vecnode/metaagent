#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"

cmake -B build -DMETAAGENT_BUILD_APP=ON
cmake --build build --target metaagent-app -j
./build/metaagent-app
