# Aegis Target Runtime Guide

This document describes how Aegis runs on the target device.

Use this guide when you want to understand:

- daemon mode
- systemd integration
- `/etc/skytrack/system.conf`
- DBus methods and signals
- CLI commands
- target-side key and AES material requirements

For Yocto-side bundle generation, see [build.md](build.md).
For the install pipeline inside the daemon, see [overview.md](overview.md).
For the OTA state machine, see [ota-flow.md](ota-flow.md).

## 1. Runtime Model

On the target, Aegis is normally used as:

- a long-running daemon started by systemd
- a thin CLI client that talks to that daemon over DBus

The intended flow is:

1. systemd starts the daemon
2. the daemon registers a DBus service on the system bus
3. a user or automation requests installation through CLI, DBus, or GCS integration
4. the daemon verifies the bundle, installs to the inactive slot, updates state, and reports progress

The CLI does not perform installation by itself. It only forwards commands to the daemon and renders daemon status for the user.

## 2. Daemon Mode

When built with DBus support, the daemon runs as:

```bash
aegis daemon --config /etc/skytrack/system.conf
```

Typical systemd unit:

```ini
[Unit]
Description=Aegis OTA daemon
After=network-online.target
Wants=network-online.target

[Service]
Type=dbus
BusName=de.skytrack.Aegis
ExecStart=/usr/bin/aegis daemon --config /etc/skytrack/system.conf
Restart=on-failure
RestartSec=5min

[Install]
WantedBy=multi-user.target
```

Useful service commands:

```bash
systemctl status aegis
systemctl start aegis
systemctl stop aegis
systemctl restart aegis
journalctl -u aegis -f
```

## 3. Configuration File

The daemon reads:

```text
/etc/skytrack/system.conf
```

The loader accepts either top-level keys or an `[update]` section.

Recommended format:

```ini
[update]
hw-compatibility=jetson-orin-nano-devkit-nvme
public-key=/etc/skytrack/public.pem
aes-key=/etc/skytrack/aes.key
bootloader-type=nvidia
data-directory=/data/aegis
log-level=4
```

## 4. Configuration Fields

| Key | Meaning |
| --- | --- |
| `hw-compatibility` | Hardware compatibility string used to validate whether a bundle is intended for this target. |
| `public-key` | PEM public key or PEM certificate used to verify `sw-description.sig`. |
| `aes-key` | AES material file used to decrypt encrypted payloads. Required only when encrypted payloads are used. |
| `bootloader-type` | Selects the boot-control backend. Use `nvidia` for NVIDIA devices. |
| `data-directory` | Daemon working directory used for OTA state and downloaded bundles. |
| `log-level` | Runtime log verbosity level. |

## 5. Key Material On The Target

The target must have:

- the public key matching the build-side signing private key
- the AES material matching the build-side `AEGIS_AES_FILE` when encrypted payloads are used

In practice:

- `public-key=/etc/skytrack/public.pem` should point to the deployed copy of the matching public key
- `aes-key=/etc/skytrack/aes.key` should point to the deployed AES material file

The private signing key must never be installed on the target.

See [build.md](build.md) for the corresponding build-side variables:

- `AEGIS_PRIVATE_KEY`
- `AEGIS_PUBLIC_KEY`
- `AEGIS_AES_FILE`

## 6. Daemon Startup Behavior

On startup, the daemon:

1. loads the config file
2. creates the configured data directory if needed
3. creates the selected boot-control backend
4. restores persisted OTA state
5. resumes any required post-boot OTA transition
6. registers the DBus object
7. enters the event loop

The daemon stores OTA state in:

```text
<data-directory>/ota-state.env
```

## 7. DBus Identity

The daemon registers on the system bus with:

| Item | Value |
| --- | --- |
| Service | `de.skytrack.Aegis` |
| Object path | `/de/skytrack/Aegis` |
| Interface | `de.skytrack.Aegis1` |

The introspection XML lives in:

```text
dbus/de.skytrack.Aegis1.xml
```

## 8. DBus Methods

### `Install`

```text
Install(s bundle)
```

Requests installation of a bundle.

The `bundle` argument may be:

- a local filesystem path
- an `https://` URL when remote-download integration is used

### `GetStatus`

```text
GetStatus() -> a{sv}
```

Returns current daemon status.

Important fields:

- `State`
- `Operation`
- `Progress`
- `Message`
- `LastError`
- `BootedSlot`
- `PrimarySlot`
- `BundleVersion`

### `MarkActive`

```text
MarkActive(s slot)
```

Sets the next primary slot.

### `GetPrimary`

```text
GetPrimary() -> s
```

Returns the currently configured primary slot.

### `GetBooted`

```text
GetBooted() -> s
```

Returns the slot the device is currently booted from.

## 9. DBus Signal

The daemon emits:

```text
StatusChanged(a{sv} status)
```

This signal is used by the CLI and other clients to follow OTA progress.

## 10. CLI Mode

When built with `AEGIS_ENABLE_DBUS=ON`, the same `aegis` binary provides the CLI client.

Available commands:

```text
aegis status
aegis install <bundle>
aegis mark-active <A|B>
aegis get-primary
aegis get-booted
aegis help
```

The CLI:

- connects to the system bus
- calls daemon DBus methods
- subscribes to `StatusChanged` where needed
- prints daemon status in a terminal-friendly format

The CLI does not:

- verify signatures by itself
- decrypt payloads by itself
- write payloads by itself
- own OTA state by itself

Those responsibilities belong to the daemon.

## 11. Common CLI Usage

Read status:

```bash
aegis status
```

Install a local bundle:

```bash
aegis install /data/update.swu
```

Mark slot `B` active:

```bash
aegis mark-active B
```

Read slot state:

```bash
aegis get-primary
aegis get-booted
```

Important behavior:

- pressing `Ctrl+C` stops only the CLI client
- it does not cancel an install already running in the daemon

## 12. `busctl` Examples

Show the interface:

```bash
busctl --system introspect de.skytrack.Aegis /de/skytrack/Aegis de.skytrack.Aegis1
```

Read status:

```bash
busctl --system --verbose call \
  de.skytrack.Aegis \
  /de/skytrack/Aegis \
  de.skytrack.Aegis1 \
  GetStatus
```

Start an install:

```bash
busctl --system call \
  de.skytrack.Aegis \
  /de/skytrack/Aegis \
  de.skytrack.Aegis1 \
  Install s /data/update.swu
```

Monitor status:

```bash
busctl --system \
  --match="type='signal',path='/de/skytrack/Aegis',interface='de.skytrack.Aegis1',member='StatusChanged'" \
  monitor de.skytrack.Aegis
```

## 13. Boot-Control Backends

Aegis supports:

- NVIDIA boot control
- U-Boot-style boot control

Use:

```ini
bootloader-type=nvidia
```

for NVIDIA targets.

Runtime requirements:

- `nvbootctrl` for NVIDIA systems
- `fw_printenv` and `fw_setenv` for U-Boot-style systems

## 14. Related Documents

- [build.md](build.md)
- [overview.md](overview.md)
- [ota-flow.md](ota-flow.md)
