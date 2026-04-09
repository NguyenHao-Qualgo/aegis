# rauc-cpp — Minimal C++ RAUC Reimplementation

A clean, OOP reimplementation of [RAUC](https://rauc.io/) (Robust Auto-Update Controller) in C++17. Keeps critical functionality (bundle creation, dm-verity, dm-crypt, signature verification, slot management) while stripping bootloader support down to **U-Boot** and **Custom** backends only.

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

2. **Singleton Context** — `Context::instance()` holds parsed config, runtime state, and booted slot info. Replaces RAUC's GLib-based `r_context()`.

3. **Value-oriented Result<T>** — Error handling uses `Result<T>` instead of GLib GError. No exceptions for expected failures.

4. **RAII everywhere** — OpenSSL objects use unique_ptr with custom deleters. Bundle mount/unmount is guard-scoped during install.

5. **Minimal dependencies** — Only OpenSSL, libcurl, and standard Linux headers. No GLib dependency at runtime (though build uses pkg-config for compatibility).

## Removed from original RAUC

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
| D-Bus service (full) | Skeleton only |
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
rauc --cert=sign.cert.pem --key=sign.key.pem \
     --bundle-format=verity \
     bundle content-dir/ update.raucb

# Create an encrypted (crypt) bundle
rauc --cert=sign.cert.pem --key=sign.key.pem \
     --bundle-format=crypt \
     --recipient=device1.cert.pem \
     --recipient=device2.cert.pem \
     bundle content-dir/ update-encrypted.raucb

# Install a bundle
rauc --conf=/etc/rauc/system.conf install update.raucb

# Show bundle info
rauc --keyring=ca.cert.pem info update.raucb

# Show system status
rauc status --detailed

# Mark current slot as good
rauc mark-good

# Run as service
rauc service
```

## system.conf Example

```ini
[system]
compatible=MyBoard v2
bootloader=uboot
mountprefix=/mnt/rauc/
statusfile=/data/rauc.status

[keyring]
path=/etc/rauc/ca.cert.pem

[handlers]
pre-install=/usr/lib/rauc/pre-install.sh
post-install=/usr/lib/rauc/post-install.sh

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
