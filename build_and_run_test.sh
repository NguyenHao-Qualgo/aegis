#!/bin/bash
set -euo pipefail

cmake -S . -B build -DAEGIS_ENABLE_COVERAGE=ON -DAEGIS_ENABLE_DBUS=OFF
cmake --build build -j$(nproc)
cmake --build build --target coverage