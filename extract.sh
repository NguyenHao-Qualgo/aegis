#!/usr/bin/env bash
set -euo pipefail

BUNDLE="${1:-update.aegisb}"
CAFILE="${2:-/home/hao-nna/uav-yocto-build/rauc-ca/ca.cert.pem}"
OUT="${3:-manifest.aegism}"

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
with open(path, "rb") as f:
    f.seek(-8, 2)
    sig_size = struct.unpack("<Q", f.read(8))[0]
cms_offset = size - 8 - sig_size
print(size)
print(sig_size)
print(cms_offset)
PY
}

mapfile -t INFO < <(read_sig_info)
BUNDLE_SIZE="${INFO[0]}"
SIG_SIZE="${INFO[1]}"
CMS_OFFSET="${INFO[2]}"

echo "Bundle size : $BUNDLE_SIZE"
echo "CMS size    : $SIG_SIZE"
echo "CMS offset  : $CMS_OFFSET"

dd if="$BUNDLE" of="$TMP_CMS" bs=1 skip="$CMS_OFFSET" count="$SIG_SIZE" status=none

openssl cms -verify \
  -inform DER \
  -in "$TMP_CMS" \
  -CAfile "$CAFILE" \
  -out "$OUT"

echo "Manifest written to: $OUT"
echo
cat "$OUT"