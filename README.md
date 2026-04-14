# aegis-cpp — Minimal C++ Update Service

A clean, OOP C++17 update service inspired by RAUC. It keeps the core functionality we care about here: bundle creation, dm-verity, dm-crypt, signature verification, slot management, and a D-Bus service interface.

## Architecture

```
┌─────────────────────────────────────────────────────────┐
│                        main.cpp                         │
│                     CLI dispatcher                      │
├───────┬────────┬────────┬────────┬───────┬──────────────┤
│bundle │install │ info   │ status │ mark  │   service    │
│create │        │extract │        │good   │   (D-Bus)    │
│resign │        │        │        │bad    │              │
│       │        │        │        │active │              │
└───┬───┴───┬────┴───┬────┴───┬────┴───┬───┴──────┬───────┘
    │       │        │        │        │          │
┌───▼───┐ ┌─▼──────┐ │  ┌─────▼──┐ ┌───▼────┐ ┌──▼──────┐
│Bundle │ │Install  │ │  │Context │ │  Mark  │ │Service  │
│create │ │orchestr.│ │  │(global)│ │good/bad│ │D-Bus    │
│open   │ │plans    │ │  │config  │ │active  │ │daemon   │
│mount  │ │execute  │ │  │state   │ │        │ │         │
└┬──┬───┘ └──┬──────┘ │  └────────┘ └───┬────┘ └─────────┘
 │  │        │        │                  │
 │  │   ┌────▼─────┐  │           ┌──────▼──────┐
 │  │   │Update    │  │           │Bootchooser  │
 │  │   │Handlers  │  │           │(interface)  │
 │  │   ├──────────┤  │           ├─────────────┤
 │  │   │Raw       │  │           │UBoot        │ fw_setenv/fw_printenv
 │  │   │FileCopy  │  │           │Custom       │ user script backend
 │  │   │Tar       │  │           │Noop         │ testing
 │  │   └──────────┘  │           └─────────────┘
 │  │                 │
┌▼──▼──┐  ┌───────┐  ┌▼────────┐  ┌──────────┐  ┌──────────┐
│Signa-│  │Verity │  │  Crypt  │  │Checksum  │  │ Mount    │
│ture  │  │Hash   │  │AES-256  │  │SHA-256   │  │squashfs  │
│CMS   │  │Tree   │  │CBC      │  │          │  │loop/dm   │
└──┬───┘  └───┬───┘  └────┬────┘  └──────────┘  └──────────┘
   │          │            │
   └──────────┴────────────┘
         OpenSSL + Linux dm
```

## Design Principles

1. **Strategy Pattern** — `IBootchooser` and `IUpdateHandler` are abstract interfaces with concrete implementations. Adding a new bootloader = one new class.

2. **Singleton Context** — `Context::instance()` holds parsed config, runtime state, and booted slot info. Replaces the original GLib-based context pattern.

3. **Value-oriented Result<T>** — Error handling uses `Result<T>` instead of GLib GError. No exceptions for expected failures.

4. **RAII everywhere** — OpenSSL objects use unique_ptr with custom deleters. Bundle mount/unmount is guard-scoped during install.

5. **Minimal dependencies** — Only OpenSSL, libcurl, and standard Linux headers. No GLib dependency at runtime (though build uses pkg-config for compatibility).

## Removed From Original RAUC

| Component | Status |
|-----------|--------|
| GRUB bootchooser | **Removed** |
| Barebox bootchooser | **Removed** |
| EFI bootchooser | **Removed** |
| Casync support | **Removed** |
| NBD streaming | **Removed** |
| GPT/MBR partition editing | **Removed** |
| eMMC boot partition handling | Stub only |
| GLib/GIO runtime dependency | **Removed** |
| D-Bus service | Minimal `de.pengutronix.aegis.Installer` implementation |
| Adaptive updates | **Removed** |

## Kept / Reimplemented

| Feature | File(s) |
|---------|---------|
| Bundle create (plain/verity/crypt) | `bundle.cpp` |
| CMS signing & verification | `signature.cpp` |
| dm-verity hash tree | `verity_hash.cpp` |
| dm-crypt AES-256 encryption | `crypt.cpp` |
| Device-mapper setup | `dm.cpp` |
| U-Boot bootchooser | `bootchooser.cpp` |
| Custom bootchooser script | `bootchooser.cpp` |
| Slot management & detection | `slot.cpp` |
| Install orchestrator | `install.cpp` |
| INI config parser (system.conf) | `config_file.cpp` |
| Manifest parser/writer | `manifest.cpp` |
| Slot status persistence | `status_file.cpp` |
| Mark good/bad/active | `mark.cpp` |
| HTTP bundle download | `network.cpp` |
| Full CLI interface | `main.cpp` |

## Building

```bash
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)
```

### Dependencies

- C++17 compiler (GCC 8+ / Clang 7+)
- CMake 3.16+
- OpenSSL 1.1+ (libssl-dev)
- libcurl (libcurl4-openssl-dev)
- squashfs-tools (mksquashfs at runtime)
- u-boot-tools (fw_setenv/fw_printenv at runtime, for U-Boot backend)

### Cross-compilation

```bash
cmake .. -DCMAKE_TOOLCHAIN_FILE=your-toolchain.cmake \
         -DCMAKE_BUILD_TYPE=Release
```

## Usage

```bash
# Create a verity bundle
aegis --cert=sign.cert.pem --key=sign.key.pem \
     --bundle-format=verity \
     bundle content-dir/ update.aegisb

# Create an encrypted (crypt) bundle
aegis --cert=sign.cert.pem --key=sign.key.pem \
     --bundle-format=crypt \
     --recipient=device1.cert.pem \
     --recipient=device2.cert.pem \
     bundle content-dir/ update-encrypted.aegisb

# Install a bundle
aegis --conf=/etc/aegis/system.conf install update.aegisb

# Show bundle info
aegis --keyring=ca.cert.pem info update.aegisb

# Show system status
aegis status --detailed

# Mark current slot as good
aegis mark-good

# Run as service
aegis service

# Example D-Bus calls
busctl introspect de.pengutronix.aegis / de.pengutronix.aegis.Installer
busctl get-property de.pengutronix.aegis / de.pengutronix.aegis.Installer Operation
busctl call de.pengutronix.aegis / de.pengutronix.aegis.Installer GetSlotStatus
```

The service now exposes a minimal Aegis D-Bus object at `/` on bus name
`de.pengutronix.aegis` with:

- methods: `InstallBundle`, `Install`, `Info`, `InspectBundle`, `Mark`, `GetSlotStatus`, `GetPrimary`
- properties: `Operation`, `LastError`, `Progress`, `Compatible`, `Variant`, `BootSlot`
- signal: `Completed`

`Install` and `InstallBundle` are non-blocking and emit `Completed` when the
background installation thread finishes.

For the real system bus, D-Bus policy must allow the process to own
`de.pengutronix.aegis`. This repository now installs
`packaging/dbus-1/system.d/de.pengutronix.aegis.conf` via CMake to
`/usr/share/dbus-1/system.d` by default, which matches the packaged layout
used by current systems.

For D-Bus activation, this repository also installs
`de.pengutronix.aegis.service` to `/usr/share/dbus-1/system-services` by
default so `busctl introspect de.pengutronix.aegis / de.pengutronix.aegis.Installer`
can auto-start the daemon.

For systemd-based targets, the build also installs `aegis.service` to
`/usr/lib/systemd/system` by default. The unit uses `Type=dbus` with
`BusName=de.pengutronix.aegis`, matching how systemd-integrated D-Bus services are
typically integrated with the system bus.

This implementation exposes the installer object at `/`. Some RAUC-derived targets
may expose slash-separated object paths such as `/de/pengutronix/aegis/Installer`,
so use `busctl introspect` on the target to confirm the path before scripting
against it.

For local development without touching the system bus, run the service with
`AEGIS_DBUS_BUS=session`.

## system.conf Example

```ini
[system]
compatible=MyBoard v2
bootloader=uboot
mountprefix=/mnt/aegis/
statusfile=/data/aegis.status

[keyring]
path=/etc/aegis/ca.cert.pem

[handlers]
pre-install=/usr/lib/aegis/pre-install.sh
post-install=/usr/lib/aegis/post-install.sh

[slot.rootfs.0]
device=/dev/mmcblk0p2
type=ext4
bootname=system0

[slot.rootfs.1]
device=/dev/mmcblk0p3
type=ext4
bootname=system1

[slot.appfs.0]
device=/dev/mmcblk0p4
type=ext4
parent=rootfs.0

[slot.appfs.1]
device=/dev/mmcblk0p5
type=ext4
parent=rootfs.1
```

## Extending

### Adding a new bootloader backend

```cpp
// 1. Add enum value in config_file.h
enum class Bootloader { UBoot, Custom, Noop, MyNewBoot };

// 2. Implement the interface in bootchooser.cpp
class MyNewBootchooser : public IBootchooser {
    Slot* get_primary(...) override { /* ... */ }
    Result<void> set_primary(Slot&) override { /* ... */ }
    Result<bool> get_state(const Slot&) override { /* ... */ }
    Result<void> set_state(Slot&, bool) override { /* ... */ }
};

// 3. Register in the factory
std::unique_ptr<IBootchooser> create_bootchooser(const SystemConfig& cfg) {
    switch (cfg.bootloader) {
        case Bootloader::MyNewBoot: return std::make_unique<MyNewBootchooser>();
        // ...
    }
}
```

### Adding a new update handler

```cpp
class MyCustomHandler : public IUpdateHandler {
    Result<void> install(const std::string& image_path,
                         const ManifestImage& image,
                         Slot& target,
                         ProgressCallback progress) override {
        // Your custom installation logic
    }
};
```

## License

LGPL-2.1-or-later (same as original RAUC)
