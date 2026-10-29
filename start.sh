#!/usr/bin/env bash
set -e
cd "$(dirname "$0")"

echo "================================================================"
echo "  FALCON AI v2.0 — OpenCode-Inspired C++20 3D Agent & Desktop"
echo "  Zero Python · Zero Docker · Standalone Executable"
echo "================================================================"

if [ ! -f "bin/falcon_server" ]; then
  if [ ! -f "frontend/web_bundle/index.html" ]; then
    (cd frontend && npm install && npm run build)
  fi
  make -j"$(nproc 2>/dev/null || echo 2)"
fi

exec ./bin/falcon_server --port 8000
