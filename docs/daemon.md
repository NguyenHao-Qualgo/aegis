# Daemon Guide

This document describes the background service mode of Aegis: how it is started, which config it reads, and which DBus methods and signals it exposes.

## Overview

When built with `AEGIS_ENABLE_DBUS=ON`, the `aegis` binary can run as a DBus service:

```bash
aegis daemon --config /etc/skytrack/system.conf
```

The systemd unit in `systemd/aegis.service` starts it as a system-bus service:

```ini
[Service]
Type=dbus
BusName=de.skytrack.Aegis
ExecStart=/usr/bin/aegis daemon --config /etc/skytrack/system.conf
```

## Build

```bash
cmake -S . -B build -DAEGIS_ENABLE_DBUS=ON
cmake --build build --parallel
```

## Config File

The daemon loads configuration from the file passed with `--config`. If no flag is given, it uses:

```text
/etc/skytrack/system.conf
```

The loader accepts either top-level keys or an `[update]` section:

```ini
[update]
public-key=/etc/skytrack/public.pem
aes-key=/etc/skytrack/aes.key
data-directory=/data/aegis
bootloader-type=nvidia
```

Meaning of each key:

- `public-key`: PEM public key or PEM certificate used to verify `sw-description.sig`
- `aes-key`: optional AES material file for encrypted payloads
- `data-directory`: daemon working directory; used for persisted OTA state and downloaded bundles
- `bootloader-type`: `nvidia` or any other value for the U-Boot backend

On startup, the daemon creates `data-directory` if needed and stores OTA state in:

```text
<data-directory>/ota-state.env
```

## Runtime Behavior

On startup the daemon:

1. loads config
2. creates the data directory
3. creates the boot-control backend
4. restores persisted OTA state
5. calls `resumeAfterBoot()`
6. registers the DBus object and enters the event loop

Install requests are asynchronous. The `Install` DBus method returns quickly, and the actual work runs on a detached worker thread.

If the daemon is already in `Download` or `Install`, a new `Install` request fails with:

```text
de.skytrack.Aegis1.Error.Busy
```

## DBus Identity

The daemon is registered on the system bus with:

- service: `de.skytrack.Aegis`
- object path: `/de/skytrack/Aegis`
- interface: `de.skytrack.Aegis1`

The introspection XML lives in `dbus/de.skytrack.Aegis1.xml`.

## DBus Methods

### Install

Request a new install:

```text
Install(s bundle)
```

The `bundle` string may be:

- a local filesystem path
- an `https://` URL (TODO)

If a URL is passed, the daemon downloads it into `data-directory` before installation.

### GetStatus

Return the current daemon status:

```text
GetStatus() -> a{sv}
```

Returned keys:

- `State`
- `Operation`
- `Progress`
- `Message`
- `LastError`
- `BootedSlot`
- `PrimarySlot`
- `BundleVersion`

### MarkActive

Mark a slot as the next primary slot:

```text
MarkActive(s slot)
```

Accepted slot values are validated by the boot-control backend.

### GetPrimary

Read the primary slot:

```text
GetPrimary() -> s
```

### GetBooted

Read the currently booted slot:

```text
GetBooted() -> s
```

## DBus Signal

The daemon emits status updates through:

```text
StatusChanged(a{sv} status)
```

This payload has the same dictionary shape as `GetStatus`.

## `busctl` Examples

Show the interface:

```bash
busctl --system introspect de.skytrack.Aegis /de/skytrack/Aegis de.skytrack.Aegis1
```

Dump the introspection XML:

```bash
busctl --system introspect de.skytrack.Aegis /de/skytrack/Aegis de.skytrack.Aegis1 --xml-interface
```

Read status:

```bash
busctl --system --verbose call \
  de.skytrack.Aegis \
  /de/skytrack/Aegis \
  de.skytrack.Aegis1 \
  GetStatus
```

Start an install from a local file:

```bash
busctl --system call \
  de.skytrack.Aegis \
  /de/skytrack/Aegis \
  de.skytrack.Aegis1 \
  Install s /data/update.swu
```

Start an install from a URL:

```bash
busctl --system call \
  de.skytrack.Aegis \
  /de/skytrack/Aegis \
  de.skytrack.Aegis1 \
  Install s https://example.com/update.swu
```

Mark slot `A` active:

```bash
busctl --system call \
  de.skytrack.Aegis \
  /de/skytrack/Aegis \
  de.skytrack.Aegis1 \
  MarkActive s A
```

Read primary and booted slots:

```bash
busctl --system call de.skytrack.Aegis /de/skytrack/Aegis de.skytrack.Aegis1 GetPrimary
busctl --system call de.skytrack.Aegis /de/skytrack/Aegis de.skytrack.Aegis1 GetBooted
```

Monitor status updates:

```bash
busctl --system \
  --match="type='signal',path='/de/skytrack/Aegis',interface='de.skytrack.Aegis1',member='StatusChanged'" \
  monitor de.skytrack.Aegis
```

## Notes

- Device writes, mounting, and boot-state changes generally require root privileges.
- U-Boot integration expects `fw_printenv` and `fw_setenv`.
- NVIDIA integration expects `nvbootctrl`.
- The daemon publishes status updates whenever the OTA state machine saves progress.
