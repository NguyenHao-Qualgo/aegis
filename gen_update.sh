#!/usr/bin/env bash
set -euo pipefail

OUT=""
AEGIS=""
DESC=""
KEY=""
PAYLOAD=""
AES_KEY=""
AES_IV=""

usage() {
    cat <<'EOF'
Usage:
  gen_update.sh \
    --output <update.swu> \
    --aegis <path-to-aegis> \
    --sw-description <template-file> \
    --sign-key <private.pem> \
    --payload <file> \
    [--aes-key-hex <64-hex> --aes-iv-hex <32-hex>]

Notes:
  - If AES key + IV are provided, payload is encrypted to <payload>.enc
  - Otherwise payload is packed as-is
  - sw-description must contain placeholders:
      __FILENAME__
      __ENCRYPTED__
      __SHA256__
      __IVT__
EOF
}

while [[ $# -gt 0 ]]; do
    case "$1" in
        --output) OUT="$2"; shift 2 ;;
        --aegis) AEGIS="$2"; shift 2 ;;
        --sw-description) DESC="$2"; shift 2 ;;
        --sign-key) KEY="$2"; shift 2 ;;
        --payload) PAYLOAD="$2"; shift 2 ;;
        --aes-key-hex) AES_KEY="$2"; shift 2 ;;
        --aes-iv-hex) AES_IV="$2"; shift 2 ;;
        -h|--help) usage; exit 0 ;;
        *) echo "unknown arg: $1" >&2; exit 1 ;;
    esac
done

[[ -n "$OUT" ]] || { echo "--output is required" >&2; exit 1; }
[[ -n "$AEGIS" ]] || { echo "--aegis is required" >&2; exit 1; }
[[ -n "$DESC" ]] || { echo "--sw-description is required" >&2; exit 1; }
[[ -n "$KEY" ]] || { echo "--sign-key is required" >&2; exit 1; }
[[ -n "$PAYLOAD" ]] || { echo "--payload is required" >&2; exit 1; }

[[ -x "$AEGIS" ]] || { echo "aegis not executable: $AEGIS" >&2; exit 1; }
[[ -f "$DESC" ]] || { echo "sw-description not found: $DESC" >&2; exit 1; }
[[ -f "$KEY" ]] || { echo "private key not found: $KEY" >&2; exit 1; }
[[ -f "$PAYLOAD" ]] || { echo "payload not found: $PAYLOAD" >&2; exit 1; }

USE_AES=0
if [[ -n "$AES_KEY" || -n "$AES_IV" ]]; then
    [[ -n "$AES_KEY" && -n "$AES_IV" ]] || { echo "provide both --aes-key-hex and --aes-iv-hex" >&2; exit 1; }
    [[ "${#AES_KEY}" -eq 64 ]] || { echo "--aes-key-hex must be 64 hex chars" >&2; exit 1; }
    [[ "${#AES_IV}" -eq 32 ]] || { echo "--aes-iv-hex must be 32 hex chars" >&2; exit 1; }
    USE_AES=1
fi

WORKDIR="$(mktemp -d)"
trap 'rm -rf "$WORKDIR"' EXIT

FINAL_DESC="$WORKDIR/sw-description"
SIG="$WORKDIR/sw-description.sig"

if [[ "$USE_AES" -eq 1 ]]; then
    PACK_PAYLOAD="$WORKDIR/$(basename "$PAYLOAD").enc"
    openssl enc -aes-256-cbc \
        -in "$PAYLOAD" \
        -out "$PACK_PAYLOAD" \
        -K "$AES_KEY" \
        -iv "$AES_IV"
    ENCRYPTED_VALUE="true"
    IVT_VALUE="$AES_IV"
else
    PACK_PAYLOAD="$WORKDIR/$(basename "$PAYLOAD")"
    cp "$PAYLOAD" "$PACK_PAYLOAD"
    ENCRYPTED_VALUE="false"
    IVT_VALUE=""
fi

SHA256="$(sha256sum "$PACK_PAYLOAD" | awk '{print $1}')"
FILENAME="$(basename "$PACK_PAYLOAD")"

cp "$DESC" "$FINAL_DESC"

sed -i \
    -e "s|__FILENAME__|$FILENAME|g" \
    -e "s|__ENCRYPTED__|$ENCRYPTED_VALUE|g" \
    -e "s|__SHA256__|$SHA256|g" \
    -e "s|__IVT__|$IVT_VALUE|g" \
    "$FINAL_DESC"

openssl dgst -sha256 -sign "$KEY" "$FINAL_DESC" > "$SIG"

"$AEGIS" pack \
    --output "$OUT" \
    --sw-description "$FINAL_DESC" \
    --sw-description-sig "$SIG" \
    "$PACK_PAYLOAD"

echo "Created $OUT"
echo "Payload: $FILENAME"
echo "Encrypted: $ENCRYPTED_VALUE"
echo "SHA256: $SHA256"