# Aegis

Aegis is a C++20 OTA utility that can:

- create signed `.swu` bundles with `aegis pack`
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

## Documentation

- [Bundle Guide](docs/bundle.md)
- [Daemon Guide](docs/daemon.md)
- [CLI Guide](docs/cli.md)

## Native Build

Pack-only build
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

daemon systemd service
```bash
aegis daemon --config /etc/skytrack/system.
```

cli
```bash
aegis status
aegis install /data/update.swu
aegis mark-active A
aegis get-primary
aegis get-booted
```

See [docs/daemon.md](docs/daemon.md) for the DBus API and `busctl` examples, and [docs/cli.md](docs/cli.md) for CLI usage.
