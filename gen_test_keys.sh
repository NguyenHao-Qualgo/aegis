#!/usr/bin/env bash

set -euo pipefail

BASE="${PWD}/test-keys"
PRIVATE_KEY="${BASE}/test.private.pem"
PUBLIC_KEY="${BASE}/test.public.pem"
AES_KEY_FILE="${BASE}/aes.key"
BITS="${BITS:-3072}"

print_usage_vars() {
    echo
    echo "Done! Add to your Yocto recipe/local.conf:"
    echo "AEGIS_PRIVATE_KEY=\"${PRIVATE_KEY}\""
    echo "AEGIS_PUBLIC_KEY=\"${PUBLIC_KEY}\""
    echo "AEGIS_AES_FILE=\"${AES_KEY_FILE}\""
    echo
}

if [[ -d "${BASE}" ]]; then
    echo "Error: ${BASE} already exists, aborting." >&2
    print_usage_vars
    exit 1
fi

mkdir -p "${BASE}"

echo "Generating RSA private key..."
openssl genrsa -out "${PRIVATE_KEY}" "${BITS}"

echo "Generating RSA public key..."
openssl rsa -in "${PRIVATE_KEY}" -pubout -out "${PUBLIC_KEY}"

echo "Generating AES key file..."
AES_KEY_HEX="$(openssl rand -hex 32)"
AES_IV_HEX="$(openssl rand -hex 16)"

cat > "${AES_KEY_FILE}" <<EOF
${AES_KEY_HEX}
${AES_IV_HEX}
EOF

print_usage_vars