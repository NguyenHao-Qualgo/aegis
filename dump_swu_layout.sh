#!/usr/bin/env bash
set -euo pipefail

if [[ $# -ne 1 ]]; then
    echo "Usage: $0 <bundle.swu>" >&2
    exit 1
fi

bundle="$1"

if [[ ! -f "$bundle" ]]; then
    echo "error: file not found: $bundle" >&2
    exit 1
fi

read_header() {
    local file="$1"
    local offset="$2"
    dd if="$file" bs=1 skip="$offset" count=110 status=none
}

read_name() {
    local file="$1"
    local offset="$2"
    local count="$3"
    dd if="$file" bs=1 skip="$offset" count="$count" status=none | tr -d '\000'
}

pad_to() {
    local size="$1"
    local align="$2"
    echo $(( (align - (size % align)) % align ))
}

hex8_to_dec() {
    local value="$1"
    echo $((16#$value))
}

print_offset() {
    printf '0x%06X' "$1"
}

total_size=$(stat -c '%s' "$bundle")
offset=0
index=1

echo "Bundle: $bundle"
echo "Size:   ${total_size} bytes"
echo

while (( offset + 110 <= total_size )); do
    header="$(read_header "$bundle" "$offset")"
    magic="${header:0:6}"

    if [[ "$magic" != "070702" ]]; then
        echo "error: expected 070702 at $(print_offset "$offset"), got '${magic}'" >&2
        exit 1
    fi

    mode_hex="${header:14:8}"
    filesize_hex="${header:54:8}"
    namesize_hex="${header:94:8}"
    checksum_hex="${header:102:8}"

    mode_dec=$(hex8_to_dec "$mode_hex")
    filesize=$(hex8_to_dec "$filesize_hex")
    namesize=$(hex8_to_dec "$namesize_hex")
    checksum=$(hex8_to_dec "$checksum_hex")

    name_offset=$((offset + 110))
    name="$(read_name "$bundle" "$name_offset" "$namesize")"
    name_pad=$(pad_to $((110 + namesize)) 4)
    data_offset=$((name_offset + namesize + name_pad))
    data_pad=$(pad_to "$filesize" 4)
    next_offset=$((data_offset + filesize + data_pad))

    echo "[$index] $name"
    echo "  entry_offset : $(print_offset "$offset")"
    echo "  magic        : $magic"
    printf '  mode         : 0%o\n' "$mode_dec"
    echo "  namesize     : ${namesize} bytes"
    echo "  filesize     : ${filesize} bytes"
    echo "  checksum     : ${checksum} (0x$(printf '%08X' "$checksum"))"
    echo "  name_offset  : $(print_offset "$name_offset")"
    echo "  name_pad     : ${name_pad} bytes"
    echo "  data_offset  : $(print_offset "$data_offset")"
    echo "  data_pad     : ${data_pad} bytes"
    echo "  next_entry   : $(print_offset "$next_offset")"
    echo

    offset="$next_offset"
    ((index++))

    if [[ "$name" == "TRAILER!!!" ]]; then
        archive_pad=$(pad_to "$offset" 512)
        echo "Archive trailer reached"
        echo "  trailer_end  : $(print_offset "$offset")"
        echo "  final_pad    : ${archive_pad} bytes"
        echo "  final_size   : $(print_offset $((offset + archive_pad)))"
        echo
        if (( offset + archive_pad != total_size )); then
            echo "warning: parsed size plus 512-byte padding does not match file size" >&2
        fi
        exit 0
    fi
done

echo "error: TRAILER!!! entry not found" >&2
exit 1
