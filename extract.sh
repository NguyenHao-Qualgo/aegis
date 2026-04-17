#!/usr/bin/env bash
set -euo pipefail

usage() {
    cat <<'EOF'
Usage: ./extract.sh BUNDLE [CAFILE] [MANIFEST_OUT] [EXTRACT_DIR]

Extract and optionally verify the manifest from an Aegis bundle.

Arguments:
  BUNDLE        Bundle path to inspect
  CAFILE        Optional CA/keyring PEM for CMS verification
  MANIFEST_OUT  Optional manifest output path (default: manifest.ini)
  EXTRACT_DIR   Optional directory to extract the full bundle contents
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

if [[ -n "$CAFILE" ]]; then
    TMP_CMS="$(mktemp)"
    cleanup() {
        rm -f "$TMP_CMS"
    }
    trap cleanup EXIT

    read_sig_info() {
python3 - "$BUNDLE" <<'PY'
import os, struct, sys
path = sys.argv[1]
size = os.path.getsize(path)
if size < 8:
    raise SystemExit(1)
with open(path, "rb") as f:
    f.seek(-8, 2)
    sig_size = struct.unpack("<Q", f.read(8))[0]
if sig_size == 0 or sig_size > size - 8:
    raise SystemExit(1)
cms_offset = size - 8 - sig_size
print(size)
print(sig_size)
print(cms_offset)
PY
    }

    if mapfile -t INFO < <(read_sig_info); then
        BUNDLE_SIZE="${INFO[0]}"
        SIG_SIZE="${INFO[1]}"
        CMS_OFFSET="${INFO[2]}"

        echo "Bundle size : $BUNDLE_SIZE"
        echo "CMS size    : $SIG_SIZE"
        echo "CMS offset  : $CMS_OFFSET"

        dd if="$BUNDLE" of="$TMP_CMS" bs=1 skip="$CMS_OFFSET" count="$SIG_SIZE" status=none

        openssl cms -verify \
          -binary \
          -inform DER \
          -in "$TMP_CMS" \
          -CAfile "$CAFILE" \
          -out "$MANIFEST_OUT"
    else
        echo "Error: bundle does not contain a valid CMS trailer" >&2
        exit 1
    fi
else
    if ! tar -tzf "$BUNDLE" >/dev/null 2>&1; then
        echo "Error: bundle is not a readable unsigned aegis tar archive: $BUNDLE" >&2
        exit 1
    fi
    if ! tar -tzf "$BUNDLE" | grep -Fx "manifest.ini" >/dev/null; then
        echo "Error: manifest.ini not found in bundle: $BUNDLE" >&2
        exit 1
    fi
    tar -xOf "$BUNDLE" manifest.ini > "$MANIFEST_OUT"
fi

echo "Manifest written to: $MANIFEST_OUT"
echo
cat "$MANIFEST_OUT"

if [[ -n "$EXTRACT_DIR" ]]; then
    mkdir -p "$EXTRACT_DIR"
    if [[ -n "${CMS_OFFSET:-}" ]]; then
        dd if="$BUNDLE" bs=1 count="$CMS_OFFSET" status=none | tar -C "$EXTRACT_DIR" -xzf -
    else
        tar -C "$EXTRACT_DIR" -xzf "$BUNDLE"
    fi
    echo
    echo "Bundle extracted to: $EXTRACT_DIR"
fi
