#!/usr/bin/env bash
set -euo pipefail

usage() {
    cat <<'EOF'
Usage: ./create.sh IMAGE CERT KEY [OUTPUT_BUNDLE]

Create a signed Aegis bundle from a single image file.

Arguments:
  IMAGE          Path to the image file to include
  CERT           Signing certificate (PEM)
  KEY            Signing private key (PEM)
  OUTPUT_BUNDLE  Output bundle path (default: update.aegisb)

Environment overrides:
  AEGIS_BIN      Aegis binary (default: ./build/aegis)
  COMPATIBLE     compatible string  (default: MyBoard)
  VERSION        version string     (default: 1.0.0)
  SLOT_CLASS     slot-class         (default: rootfs)
  IMAGE_TYPE     payload type       (default: archive)
EOF
}

if [[ "${1:-}" == "-h" || "${1:-}" == "--help" ]]; then
    usage
    exit 0
fi

if [[ $# -lt 3 || $# -gt 4 ]]; then
    usage >&2
    exit 1
fi

IMAGE="$(realpath "$1")"
CERT="$(realpath "$2")"
KEY="$(realpath "$3")"
OUTPUT_BUNDLE="${4:-update.aegisb}"

AEGIS_BIN="${AEGIS_BIN:-./build/aegis}"
COMPATIBLE="${COMPATIBLE:-MyBoard}"
VERSION="${VERSION:-1.0.0}"
SLOT_CLASS="${SLOT_CLASS:-rootfs}"
IMAGE_TYPE="${IMAGE_TYPE:-archive}"

if [[ ! -x "$AEGIS_BIN" ]]; then
    echo "Error: aegis binary not found or not executable: $AEGIS_BIN" >&2
    exit 1
fi
if [[ ! -f "$IMAGE" ]]; then
    echo "Error: image not found: $IMAGE" >&2
    exit 1
fi
if [[ ! -f "$CERT" ]]; then
    echo "Error: cert not found: $CERT" >&2
    exit 1
fi
if [[ ! -f "$KEY" ]]; then
    echo "Error: key not found: $KEY" >&2
    exit 1
fi

echo "Creating bundle: $OUTPUT_BUNDLE"

"$AEGIS_BIN" bundle create \
    --compatible "$COMPATIBLE" \
    --version    "$VERSION" \
    --artifact   "$SLOT_CLASS:$IMAGE_TYPE:$IMAGE" \
    --cert       "$CERT" \
    --key        "$KEY" \
    --output     "$OUTPUT_BUNDLE"

echo "Bundle ready: $OUTPUT_BUNDLE"
