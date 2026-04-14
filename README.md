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

## License

This project is licensed under the MIT License. See the LICENSE file for details.