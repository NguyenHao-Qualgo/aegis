#!/usr/bin/env bash
set -euo pipefail

usage() {
    cat <<'EOF'
Usage: ./create.sh IMAGE CERT KEY [OUTPUT_BUNDLE]

Create a simple Aegis bundle from a single image file.

Arguments:
  IMAGE          Path to the image file to include
  CERT           Signing certificate path
  KEY            Signing private key path
  OUTPUT_BUNDLE  Optional output bundle path (default: update.aegisb)

Environment overrides:
  AEGIS_BIN      Aegis binary path (default: ./build/aegis)
  CONTENT_DIR    Working content directory (default: ./build/content-dir)
  COMPATIBLE     Manifest compatible string (default: MyBoard)
  VERSION        Manifest version string (default: 1.0.0)
  SLOT_CLASS     Manifest slot class (default: rootfs)
  IMAGE_TYPE     Manifest payload type (default: archive)
  SOURCE_TYPE    Manifest source type (default: file)
  BUNDLE_FORMAT  Bundle format written into the manifest (default: plain)
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
CONTENT_DIR="${CONTENT_DIR:-./build/content-dir}"
COMPATIBLE="${COMPATIBLE:-MyBoard}"
VERSION="${VERSION:-1.0.0}"
SLOT_CLASS="${SLOT_CLASS:-rootfs}"
IMAGE_TYPE="${IMAGE_TYPE:-archive}"
SOURCE_TYPE="${SOURCE_TYPE:-file}"
BUNDLE_FORMAT="${BUNDLE_FORMAT:-plain}"

if [[ ! -x "$AEGIS_BIN" ]]; then
    echo "Error: Aegis binary not found or not executable: $AEGIS_BIN" >&2
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

mkdir -p "$CONTENT_DIR"

IMAGE_NAME="$(basename "$IMAGE")"
cp "$IMAGE" "$CONTENT_DIR/$IMAGE_NAME"

cat > "$CONTENT_DIR/manifest.ini" <<EOF
[update]
compatible=$COMPATIBLE
version=$VERSION

[bundle]
format=$BUNDLE_FORMAT

[image.$SLOT_CLASS]
slot-class=$SLOT_CLASS
source-type=$SOURCE_TYPE
type=$IMAGE_TYPE
filename=$IMAGE_NAME
EOF

echo "Created manifest: $CONTENT_DIR/manifest.ini"
echo "Creating bundle: $OUTPUT_BUNDLE"

"$AEGIS_BIN" bundle create \
  --cert "$CERT" \
  --key "$KEY" \
  --manifest "$CONTENT_DIR/manifest.ini" \
  --output "$OUTPUT_BUNDLE"

echo "Bundle ready: $OUTPUT_BUNDLE"
