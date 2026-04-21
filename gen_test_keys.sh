#!/usr/bin/env bash
set -euo pipefail

usage() {
    cat <<'EOF'
Usage:
  gen_test_keys.sh [--outdir <dir>] [--name <prefix>] [--bits <rsa-bits>]

Examples:
  ./gen_test_keys.sh
  ./gen_test_keys.sh --outdir ./keys --name qemuarm64
EOF
}

fail() {
    echo "error: $*" >&2
    exit 1
}

require_cmd() {
    command -v "$1" >/dev/null 2>&1 || fail "missing required command: $1"
}

OUTDIR=""
NAME="swupdate-test"
BITS="2048"

while [[ $# -gt 0 ]]; do
    case "$1" in
        --outdir)
            OUTDIR="${2:-}"
            shift 2
            ;;
        --name)
            NAME="${2:-}"
            shift 2
            ;;
        --bits)
            BITS="${2:-}"
            shift 2
            ;;
        -h|--help)
            usage
            exit 0
            ;;
        *)
            fail "unknown argument: $1"
            ;;
    esac
done

require_cmd openssl

if [[ -z "${OUTDIR}" ]]; then
    OUTDIR="./keys"
fi

mkdir -p "${OUTDIR}"

PRIVATE_KEY="${OUTDIR}/${NAME}.key.pem"
PUBLIC_KEY="${OUTDIR}/${NAME}.public.pem"

openssl genrsa -out "${PRIVATE_KEY}" "${BITS}"
openssl rsa -in "${PRIVATE_KEY}" -pubout -out "${PUBLIC_KEY}"

echo "Created private key: ${PRIVATE_KEY}"
echo "Created public key:  ${PUBLIC_KEY}"
echo
echo "Use for SWU generation:"
echo "  --sign-key ${PRIVATE_KEY}"
echo
echo "Use for target install:"
echo "  --public-key ${PUBLIC_KEY}"
