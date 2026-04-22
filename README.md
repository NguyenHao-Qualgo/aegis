# Aegis

Aegis is a C++17 OTA utility that can:

- create signed SWUpdate-style `.swu` bundles with `aegis pack`
- stream-install signed bundles without unpacking the whole archive first
- handle `raw`, `archive`, and `tar` payload entries
- verify `sw-description` signatures and optionally decrypt AES-CBC payloads
- manage A/B slot activation through U-Boot or NVIDIA boot-control backends

When built with DBus support, the same binary also exposes an OTA daemon and a small CLI client for `status`, `install`, `mark-active`, `get-primary`, and `get-booted`.

## Repository Highlights

- `src/installer/packer.cpp`: writes `.swu` archives in `cpio crc/newc` format
- `src/installer/installer.cpp`: streaming installer for signed `.swu` bundles
- `src/installer/raw_handler.cpp`: raw image/file writer
- `src/installer/archive_handler.cpp`: streamed archive extraction through `libarchive`
- `src/service/dbus_service.cpp`: DBus daemon surface
- `sw-description.default`: example A/B rootfs manifest template
- `gen_update.sh`: helper that fills the manifest template, signs it, and packs the bundle
- `gen_test_keys.sh`: helper that creates local test RSA/AES materials

## Build Requirements

Required for all builds:

- CMake 3.16+
- a C++17 compiler
- `pkg-config`
- OpenSSL
- `libarchive`
- `fmt`
- `spdlog`

Additional dependencies:

- `GTest` when `AEGIS_BUILD_TESTS=ON` (default)
- `sdbus-c++` when `AEGIS_ENABLE_DBUS=ON`
- `lcov` and `genhtml` if you want the coverage target

## Native Build

Pack-only build:

```bash
cmake -S . -B build -DAEGIS_ENABLE_DBUS=OFF
cmake --build build --parallel
```

Run the unit tests:

```bash
ctest --test-dir build --output-on-failure
```

The generated binary is:

```bash
./build/aegis
```

There is also a helper script that configures coverage, builds, runs the tests, and generates an HTML report:

```bash
./build_and_run_test.sh
```

## DBus-Enabled Build

Enable DBus support if you want the daemon and client commands:

```bash
cmake -S . -B build -DAEGIS_ENABLE_DBUS=ON
cmake --build build --parallel
```

With that build, `aegis` supports:

```bash
aegis daemon --config /etc/skytrack/system.conf
aegis status
aegis install /data/update.swu
aegis mark-active A
aegis get-primary
aegis get-booted
```

The default daemon config path is `/etc/skytrack/system.conf`. The loader accepts either top-level keys or an `[update]` section with these entries:

```ini
[update]
public-key=/etc/skytrack/test.public.pem
aes-key=/etc/skytrack/aes.key
data-directory=/var/lib/aegis
bootloader-type=nvidia
```

Notes:

- `bootloader-type` accepts `nvidia` or defaults to the U-Boot backend
- U-Boot runtime support expects `fw_printenv` and `fw_setenv`
- NVIDIA runtime support expects `nvbootctrl` and the UEFI helper script used by Jetson systems
- install paths that write devices, mount filesystems, or modify boot state generally require root privileges

## Creating Test Keys

Generate a local RSA keypair plus an AES material file:

```bash
./gen_test_keys.sh
```

This creates:

```bash
./test-keys/test.private.pem
./test-keys/test.public.pem
./test-keys/aes.key
```

The script stops if `./test-keys` already exists.

## Creating A Bundle

`aegis pack` only assembles the archive. If you also want placeholder replacement and manifest signing, use `gen_update.sh`.

Quick smoke example:

```bash
./gen_update.sh \
  --output /tmp/update.swu \
  --aegis ./build/aegis \
  --sw-description ./sw-description.default \
  --sign-key ./test-keys/test.private.pem \
  --payload ./test
```

That command:

- copies `sw-description.default`
- replaces `__FILENAME__`, `__ENCRYPTED__`, `__SHA256__`, and `__IVT__`
- signs the final `sw-description`
- calls `aegis pack` to create the `.swu`

To emit an encrypted payload, add:

```bash
  --aes-key-hex <64-hex-key> \
  --aes-iv-hex <32-hex-iv>
```

If you already have a ready-to-pack `sw-description` and its detached signature, the low-level packing command is:

```bash
./build/aegis pack \
  --output update.swu \
  --sw-description sw-description \
  --sw-description-sig sw-description.sig \
  payload.bin
```
