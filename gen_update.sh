#!/usr/bin/env bash
set -euo pipefail

usage() {
    cat <<'EOF'
Usage:
  gen_update.sh
    --output <update.swu>
    --swupdate <path-to-swupdate_cpp>
    --slotclass <name>                    # e.g. ROOTFS
    --payload <file>
    [--version <x.y.z>]
    --sign-key <private.pem>
    [--encrypt]
    [--aes-key-hex <64-hex>]
    [--aes-iv-hex <32-hex>]
    [--aes-key-file-out <file>]
    [--workdir <dir>]

Behavior:
  - Generates grouped SWUpdate metadata:
      software.A.images -> installs to /dev/disk/by-partlabel/<slotclass>_B
      software.B.images -> installs to /dev/disk/by-partlabel/<slotclass>_A
  - Payload type is currently fixed to raw to match the new design example.

Example:
   AES_KEY=$(openssl rand -hex 32)
   AES_IV=$(openssl rand -hex 16)
  ./gen_update.sh \
    --output update.swu \
    --swupdate ./build/swupdate_cpp \
    --slotclass ROOTFS \
    --payload rootfs.ext4.enc \
    --sign-key priv.pem \
    --encrypt \
    --aes-key-hex ${AES_KEY} \
    --aes-iv-hex ${AES_IV}
EOF
}

fail() {
    echo "error: $*" >&2
    exit 1
}

require_cmd() {
    command -v "$1" >/dev/null 2>&1 || fail "missing required command: $1"
}

sha256_file() {
    sha256sum "$1" | awk '{print $1}'
}

is_hex_len() {
    local value="$1"
    local expected_len="$2"
    [[ "${#value}" -eq "${expected_len}" && "${value}" =~ ^[0-9a-fA-F]+$ ]]
}

OUTPUT=""
SWUPDATE_BIN=""
SLOTCLASS=""
PAYLOAD=""
SIGN_KEY=""
WORKDIR=""
VERSION="1.0.0"
ENCRYPT=0
AES_KEY_HEX=""
AES_IV_HEX=""
AES_KEY_FILE_OUT=""

while [[ $# -gt 0 ]]; do
    case "$1" in
        --output)
            OUTPUT="${2:-}"
            shift 2
            ;;
        --swupdate)
            SWUPDATE_BIN="${2:-}"
            shift 2
            ;;
        --slotclass)
            SLOTCLASS="${2:-}"
            shift 2
            ;;
        --payload)
            PAYLOAD="${2:-}"
            shift 2
            ;;
        --version)
            VERSION="${2:-}"
            shift 2
            ;;
        --sign-key)
            SIGN_KEY="${2:-}"
            shift 2
            ;;
        --workdir)
            WORKDIR="${2:-}"
            shift 2
            ;;
        --encrypt)
            ENCRYPT=1
            shift
            ;;
        --aes-key-hex)
            AES_KEY_HEX="${2:-}"
            shift 2
            ;;
        --aes-iv-hex)
            AES_IV_HEX="${2:-}"
            shift 2
            ;;
        --aes-key-file-out)
            AES_KEY_FILE_OUT="${2:-}"
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

[[ -n "${OUTPUT}" ]] || fail "--output is required"
[[ -n "${SWUPDATE_BIN}" ]] || fail "--swupdate is required"
[[ -n "${SLOTCLASS}" ]] || fail "--slotclass is required"
[[ -n "${PAYLOAD}" ]] || fail "--payload is required"
[[ -n "${SIGN_KEY}" ]] || fail "--sign-key is required"

[[ -x "${SWUPDATE_BIN}" ]] || fail "swupdate binary is not executable: ${SWUPDATE_BIN}"
[[ -f "${PAYLOAD}" ]] || fail "payload not found: ${PAYLOAD}"
[[ -f "${SIGN_KEY}" ]] || fail "sign key not found: ${SIGN_KEY}"

require_cmd openssl
require_cmd sha256sum

if [[ -z "${WORKDIR}" ]]; then
    WORKDIR="$(mktemp -d)"
    CLEAN_WORKDIR=1
else
    mkdir -p "${WORKDIR}"
    CLEAN_WORKDIR=0
fi

cleanup() {
    if [[ "${CLEAN_WORKDIR}" -eq 1 ]]; then
        rm -rf "${WORKDIR}"
    fi
}
trap cleanup EXIT

PAYLOAD_BASENAME="$(basename "${PAYLOAD}")"
FINAL_PAYLOAD="${PAYLOAD}"
AES_KEY_FILE=""
ENCRYPTED_LINE=""
IVT_LINE=""

if [[ "${ENCRYPT}" -eq 1 ]]; then
    [[ -n "${AES_KEY_HEX}" ]] || fail "--aes-key-hex is required with --encrypt"
    [[ -n "${AES_IV_HEX}" ]] || fail "--aes-iv-hex is required with --encrypt"
    is_hex_len "${AES_KEY_HEX}" 64 || fail "--aes-key-hex must be 64 hex chars"
    is_hex_len "${AES_IV_HEX}" 32 || fail "--aes-iv-hex must be 32 hex chars"

    FINAL_PAYLOAD="${WORKDIR}/${PAYLOAD_BASENAME}.enc"
    openssl enc -aes-256-cbc -in "${PAYLOAD}" -out "${FINAL_PAYLOAD}" -K "${AES_KEY_HEX}" -iv "${AES_IV_HEX}"

    if [[ -z "${AES_KEY_FILE_OUT}" ]]; then
        if [[ "${OUTPUT}" == *.swu ]]; then
            AES_KEY_FILE="${OUTPUT%.swu}.aes"
        else
            AES_KEY_FILE="${OUTPUT}.aes"
        fi
    else
        AES_KEY_FILE="${AES_KEY_FILE_OUT}"
    fi

    mkdir -p "$(dirname "${AES_KEY_FILE}")"
    printf '%s %s\n' "${AES_KEY_HEX}" "${AES_IV_HEX}" > "${AES_KEY_FILE}"

    PAYLOAD_BASENAME="$(basename "${FINAL_PAYLOAD}")"
    ENCRYPTED_LINE='            encrypted = true;'
    IVT_LINE="            ivt = \"${AES_IV_HEX}\";"
fi

STAGED_PAYLOAD="${WORKDIR}/${PAYLOAD_BASENAME}"
if [[ "${FINAL_PAYLOAD}" != "${STAGED_PAYLOAD}" ]]; then
    cp "${FINAL_PAYLOAD}" "${STAGED_PAYLOAD}"
fi

PAYLOAD_SHA256="$(sha256_file "${STAGED_PAYLOAD}")"

TARGET_A="/dev/disk/by-partlabel/${SLOTCLASS}_A"
TARGET_B="/dev/disk/by-partlabel/${SLOTCLASS}_B"

SW_DESCRIPTION="${WORKDIR}/sw-description"
SW_DESCRIPTION_SIG="${WORKDIR}/sw-description.sig"

cat > "${SW_DESCRIPTION}" <<EOF
software =
{
    version = "${VERSION}";

    A:
    {
        images: (
        {
            filename  = "${PAYLOAD_BASENAME}";
            type      = "raw";
            device    = "${TARGET_A}";
$( [[ -n "${ENCRYPTED_LINE}" ]] && printf '%s\n' "${ENCRYPTED_LINE}" )
$( [[ -n "${IVT_LINE}" ]] && printf '%s\n' "${IVT_LINE}" )
            sha256    = "${PAYLOAD_SHA256}";
        }
        );
    };
    B:
    {
        images: (
        {
            filename  = "${PAYLOAD_BASENAME}";
            type      = "raw";
            device    = "${TARGET_B}";
$( [[ -n "${ENCRYPTED_LINE}" ]] && printf '%s\n' "${ENCRYPTED_LINE}" )
$( [[ -n "${IVT_LINE}" ]] && printf '%s\n' "${IVT_LINE}" )
            sha256    = "${PAYLOAD_SHA256}";
        }
        );
    };
}
EOF

openssl dgst -sha256 -sign "${SIGN_KEY}" "${SW_DESCRIPTION}" > "${SW_DESCRIPTION_SIG}"

"${SWUPDATE_BIN}" pack \
    --output "${OUTPUT}" \
    --sw-description "${SW_DESCRIPTION}" \
    --sw-description-sig "${SW_DESCRIPTION_SIG}" \
    "${STAGED_PAYLOAD}"

echo "Created SWU: ${OUTPUT}"
echo "Payload SHA256: ${PAYLOAD_SHA256}"
echo "Group A target: ${TARGET_A}"
echo "Group B target: ${TARGET_B}"
if [[ "${ENCRYPT}" -eq 1 ]]; then
    echo "AES key file: ${AES_KEY_FILE}"
fi