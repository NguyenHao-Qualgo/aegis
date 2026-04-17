#!/usr/bin/env bash
set -euo pipefail

usage() {
    cat <<'EOF'
Usage: ./extract.sh BUNDLE [CAFILE] [MANIFEST_OUT] [EXTRACT_DIR]

Inspect and optionally verify an Aegis bundle.

Arguments:
  BUNDLE        Bundle path to inspect
  CAFILE        CA/keyring PEM for CMS signature verification (optional)
  MANIFEST_OUT  Path to write the extracted manifest (default: manifest.ini)
  EXTRACT_DIR   Directory to extract full bundle payload into (optional)
EOF
}

if [[ "${1:-}" == "-h" || "${1:-}" == "--help" ]]; then
    usage
    exit 0
fi

if [[ $# -lt 1 || $# -gt 4 ]]; then
    usage >&2
    exit 1
fi

BUNDLE="$(realpath "$1")"
CAFILE="${2:-}"
MANIFEST_OUT="${3:-manifest.ini}"
EXTRACT_DIR="${4:-}"

if [[ ! -f "$BUNDLE" ]]; then
    echo "Error: bundle not found: $BUNDLE" >&2
    exit 1
fi

mkdir -p "$(dirname "$MANIFEST_OUT")"

# ---------------------------------------------------------------------------
# Parse the bundle trailer to determine whether the bundle is CMS-signed.
# Returns: PAYLOAD_SIZE  CMS_SIZE  (both in bytes)
# ---------------------------------------------------------------------------
parse_trailer() {
python3 - "$BUNDLE" <<'PY'
import os, struct, sys
path = sys.argv[1]
total = os.path.getsize(path)
if total < 8:
    raise SystemExit("bundle too small")
with open(path, "rb") as f:
    f.seek(-8, 2)
    cms_size = struct.unpack("<Q", f.read(8))[0]
if cms_size == 0 or cms_size > total - 8:
    # unsigned bundle — payload is the whole file
    print(total, 0)
else:
    payload_size = total - 8 - cms_size
    print(payload_size, cms_size)
PY
}

read -r PAYLOAD_SIZE CMS_SIZE < <(parse_trailer)

if (( CMS_SIZE > 0 )); then
    CMS_OFFSET="$PAYLOAD_SIZE"
    echo "Signed bundle"
    echo "  Payload size : $PAYLOAD_SIZE bytes"
    echo "  CMS size     : $CMS_SIZE bytes"
    echo "  CMS offset   : $CMS_OFFSET"

    if [[ -n "$CAFILE" ]]; then
        TMP_CMS="$(mktemp --suffix=.der)"
        trap 'rm -f "$TMP_CMS"' EXIT

        # Extract the CMS bytes using Python (efficient random-access read)
        python3 - "$BUNDLE" "$CMS_OFFSET" "$CMS_SIZE" "$TMP_CMS" <<'PY'
import sys
src, offset, size, dst = sys.argv[1], int(sys.argv[2]), int(sys.argv[3]), sys.argv[4]
with open(src, "rb") as f:
    f.seek(offset)
    data = f.read(size)
if len(data) != size:
    raise SystemExit("short read extracting CMS")
with open(dst, "wb") as f:
    f.write(data)
PY

        openssl cms -verify \
            -binary \
            -inform DER \
            -in  "$TMP_CMS" \
            -CAfile "$CAFILE" \
            -out "$MANIFEST_OUT"
        echo "Signature OK"
    else
        echo "No CAFILE supplied — skipping signature verification"
        # Extract manifest from the payload tar.gz without verifying
        head -c "$PAYLOAD_SIZE" "$BUNDLE" | tar -xOzf - manifest.ini > "$MANIFEST_OUT"
    fi
else
    echo "Unsigned bundle"
    if ! tar -tzf "$BUNDLE" manifest.ini >/dev/null 2>&1; then
        echo "Error: manifest.ini not found in bundle" >&2
        exit 1
    fi
    tar -xOzf "$BUNDLE" manifest.ini > "$MANIFEST_OUT"
fi

echo
echo "Manifest written to: $MANIFEST_OUT"
echo
cat "$MANIFEST_OUT"

# ---------------------------------------------------------------------------
# Optional full payload extraction
# ---------------------------------------------------------------------------
if [[ -n "$EXTRACT_DIR" ]]; then
    mkdir -p "$EXTRACT_DIR"
    echo
    if (( CMS_SIZE > 0 )); then
        head -c "$PAYLOAD_SIZE" "$BUNDLE" | tar -C "$EXTRACT_DIR" -xzf -
    else
        tar -C "$EXTRACT_DIR" -xzf "$BUNDLE"
    fi
    echo "Bundle payload extracted to: $EXTRACT_DIR"
fi
