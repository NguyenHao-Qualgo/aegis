# Aegis Target Runtime Guide

This document describes target-side Aegis operation: the daemon service, DBus API, CLI commands, configuration, and install behavior.

Aegis has two main operating modes:

1. **Native mode**: runs in the Yocto/native build environment and creates signed, optionally encrypted `.swu` bundles.
2. **Target mode**: runs on the target device and installs `.swu` bundles through a daemon, GCS integration, or CLI command.

This guide focuses only on **target mode**. For Yocto bundle creation, see `native.md`.

## 1. Target Mode Overview

On the target device, Aegis is normally used as a background OTA service.

The same `aegis` binary can provide:

- **Daemon mode**: long-running system service that performs update operations.
- **CLI mode**: command-line client that talks to the daemon over DBus.

The intended runtime model is:

1. The daemon starts through systemd.
2. The daemon registers a DBus service on the system bus.
3. A client requests an update from one of these paths:
   - CLI command: `aegis install <bundle>`
   - GCS integration
   - direct DBus method call
4. The daemon verifies the bundle, selects the inactive slot, streams payloads to the selected handler, updates boot state, and reports progress.

The CLI is intentionally thin. It does not install the bundle by itself. It sends requests to the daemon and renders daemon status in a terminal-friendly format.

## 2. Daemon Mode

When built with `AEGIS_ENABLE_DBUS=ON`, the `aegis` binary can run as a DBus service:

```bash
aegis daemon --config /etc/skytrack/system.conf
```

The daemon is normally started by systemd.

Example systemd service:

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

## 3. Target Configuration File

The daemon reads configuration from the file passed with `--config`.

Default path:

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

Key meanings:

| Key | Meaning |
| --- | --- |
| `hw-compatibility` | Hardware compatibility string used to validate whether a bundle is intended for this target. |
| `public-key` | PEM public key or PEM certificate used to verify `sw-description.sig`. |
| `aes-key` | Optional AES material file used to decrypt encrypted payloads. Required only when the manifest marks a payload as encrypted. |
| `bootloader-type` | Selects the boot-control backend. Use `nvidia` for NVIDIA boot control. Other values use the U-Boot-style backend. |
| `data-directory` | Daemon working directory used for persisted OTA state and downloaded bundles. |
| `log-level` | Runtime log verbosity level. |

On startup, the daemon creates `data-directory` if needed and stores OTA state in:

```text
<data-directory>/ota-state.env
```

## 4. AES Key File Format

The `aes-key` file must contain two whitespace-separated hex values:

```text
00112233445566778899aabbccddeeff00112233445566778899aabbccddeeff
aabbccddeeff00112233445566778899
```

Meaning:

- First token: AES key in hex.
- Second token: IV in hex.

Recommended format is one token per line.

For AES-256-CBC, use:

- 32-byte key: 64 hex characters.
- 16-byte IV: 32 hex characters.

If a manifest entry contains `ivt`, that per-entry IV overrides the default IV from `aes-key` for that payload.

Do not use raw `openssl enc -P` output directly because it includes labels such as `key=` and `iv =`. Aegis expects only the bare hex values.

## 5. Runtime Startup Behavior

On startup, the daemon:

1. Loads the config file.
2. Creates the data directory if missing.
3. Creates the selected boot-control backend.
4. Restores persisted OTA state.
5. Calls `resumeAfterBoot()` to complete any required post-boot state transition.
6. Registers the DBus object.
7. Enters the event loop.

Install requests are asynchronous. The `Install` DBus method returns quickly, and the actual install work runs on the daemon background install thread.

If the daemon is already in `Download` or `Install`, a second install request fails with:

```text
de.skytrack.Aegis1.Error.Busy
```

## 6. DBus Identity

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

## 7. DBus Methods

### `Install`

Request a new install:

```text
Install(s bundle)
```

The `bundle` string may be:

- a local filesystem path
- an `https://` URL (TODO / integration-dependent)

If URL download support is enabled, the daemon downloads the file into `data-directory` before installation.

### `GetStatus`

Return current daemon status:

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

### `MarkActive`

Mark a slot as the next primary slot:

```text
MarkActive(s slot)
```

The accepted slot values are validated by the boot-control backend. The CLI accepts only `A` or `B`.

### `GetPrimary`

Read the primary slot:

```text
GetPrimary() -> s
```

### `GetBooted`

Read the currently booted slot:

```text
GetBooted() -> s
```

## 8. DBus Signal

The daemon emits status updates through:

```text
StatusChanged(a{sv} status)
```

The signal payload has the same dictionary shape as `GetStatus`.

## 9. CLI Mode

When built with `AEGIS_ENABLE_DBUS=ON`, the same `aegis` binary provides a CLI client for daemon control.

Available commands:

```text
aegis status
aegis install <bundle>
aegis mark-active <A|B>
aegis get-primary
aegis get-booted
aegis help
```

The CLI connects to:

| Item | Value |
| --- | --- |
| Service | `de.skytrack.Aegis` |
| Object path | `/de/skytrack/Aegis` |
| Interface | `de.skytrack.Aegis1` |

## 10. CLI Command Reference

### `aegis status`

Query the daemon and print the current status dictionary.

```bash
aegis status
```

Typical fields:

- `State`
- `Operation`
- `Progress`
- `Message`
- `LastError`
- `BootedSlot`
- `PrimarySlot`
- `BundleVersion`

### `aegis install <bundle>`

Request installation of a bundle and print progress updates until a terminal state is reached.

```bash
aegis install /data/update.swu
```

The bundle argument may be:

- a local file path
- an `https://` URL (TODO / integration-dependent)

During install, the client subscribes to `StatusChanged` and prints progress lines like:

```text
Install [verify] 15% - Verifying package signature
Install [install] 30% - Installing payload
```

The client exits when the daemon reports one of these terminal states:

- `Idle`
- `Reboot`
- `Failure`

Important behavior:

- Pressing `Ctrl+C` stops only the CLI client.
- Pressing `Ctrl+C` does not cancel an install already running in the daemon.

### `aegis mark-active <A|B>`

Set the next primary slot.

```bash
aegis mark-active A
```

The client accepts only `A` or `B`.

### `aegis get-primary`

Print the primary slot reported by the daemon:

```bash
aegis get-primary
```

### `aegis get-booted`

Print the currently booted slot reported by the daemon:

```bash
aegis get-booted
```

### `aegis help`

Show command usage:

```bash
aegis help
aegis help install
```

## 11. Common CLI Examples

Check daemon status:

```bash
aegis status
```

Install a local bundle:

```bash
aegis install /data/update.swu
```

Install a remote bundle:

```bash
aegis install https://example.com/update.swu
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

## 12. `busctl` Examples

Show the DBus interface:

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

## 13. Target Install Flow

A typical target-side installation flow is:

1. User, GCS, or automation requests install.
2. The daemon receives `Install(bundle)`.
3. If the bundle is remote, the daemon downloads it into `data-directory`.
4. The daemon reads the `.swu` as a stream.
5. The daemon verifies `sw-description.sig` using the configured public key.
6. The daemon reads the currently booted slot.
7. The daemon chooses the inactive slot as the target slot.
8. The daemon parses only the matching `A` or `B` block from `sw-description`.
9. The daemon streams each matching payload to the selected handler.
10. If a payload is encrypted, the daemon decrypts it while streaming.
11. The daemon verifies payload SHA-256.
12. The daemon updates OTA state.
13. The daemon marks the target slot active when the install succeeds.
14. The target reboots when required.
15. After boot, the daemon resumes state handling and confirms the boot result.

## 14. Streaming Behavior

Aegis installs directly from the `.swu` stream:

- It does not unpack the whole bundle into a temporary directory first.
- It reads each CPIO entry in order.
- Matching payloads are streamed directly to the selected handler.
- Encrypted payloads are decrypted in-process while streaming.
- Archive payloads are extracted while reading the bundle.
- Compressed raw payloads can be inflated while writing to the target device.

This behavior reduces temporary storage requirements on the target.

## 15. Install-Time Checks

Before and during installation, Aegis checks:

1. `sw-description` is the first archive entry.
2. `sw-description.sig` is the second archive entry.
3. The signature matches the configured public key.
4. The selected manifest block matches the target slot.
5. Each manifest `filename` matches a payload inside the bundle.
6. Each payload SHA-256 matches the packed payload contents.
7. Encrypted entries can be decrypted with the configured AES material or per-entry IV.
8. The selected handler can write to the configured target device or path.

If a required check fails, installation stops and `LastError` is updated.

## 16. Boot-Control Backends

Aegis uses the configured boot-control backend to read and update slot state.

### NVIDIA backend

Use this for NVIDIA targets:

```ini
bootloader-type=nvidia
```

Runtime requirement:

```text
nvbootctrl
```

### U-Boot backend

Use this for U-Boot bootchooser-style integration. Any non-`nvidia` value selects the U-Boot-style backend in the current implementation.

Runtime requirements:

```text
fw_printenv
fw_setenv
```

Boot-control operations generally require root privileges.

## 17. Relationship Between CLI And Daemon

The CLI is a DBus client only.

It does not:

- verify bundle signatures itself
- decrypt payloads itself
- write payloads to devices itself
- change boot slots directly by itself
- maintain OTA state by itself

It does:

- call daemon DBus methods
- subscribe to daemon status signals
- print daemon status in a user-friendly format

The daemon owns the install state machine and all target-changing operations.

## 18. Common Error Cases

### Daemon is not running

Symptoms:

- `aegis status` fails.
- DBus service `de.skytrack.Aegis` is not found.

Check:

```bash
systemctl status aegis
journalctl -u aegis -n 100
```

### Missing or invalid bundle path

Symptoms:

- `aegis install` fails immediately.
- Daemon reports a file open or download error.

Check:

```bash
ls -lh /data/update.swu
```

### Invalid `mark-active` argument

Symptoms:

- CLI rejects values other than `A` or `B`.

Correct examples:

```bash
aegis mark-active A
aegis mark-active B
```

### Signature verification failure

Check:

- Target `public-key` matches the build-time private key.
- The bundle was not modified after generation.
- `sw-description` and `sw-description.sig` are both present and in the correct order.

### Payload decryption failure

Check:

- Target `aes-key` matches the build-time `AEGIS_AES_FILE`.
- AES file contains only bare hex values.
- Manifest `ivt` value is valid if present.

### Daemon reports busy

Reason:

- Another install is already running.

Expected DBus error:

```text
de.skytrack.Aegis1.Error.Busy
```

Check current state:

```bash
aegis status
```

### Install fails after payload writing

Check:

- Device paths from the selected manifest slot block are correct.
- Target partitions exist.
- The daemon has permission to write to the target devices.
- Boot-control backend is available and working.

## 19. Operational Checklist

Before installing:

- [ ] `aegis.service` is running.
- [ ] `/etc/skytrack/system.conf` exists.
- [ ] `public-key` points to the correct public key or certificate.
- [ ] `aes-key` points to the correct AES material when encrypted payloads are used.
- [ ] `data-directory` exists or can be created by the daemon.
- [ ] Boot-control tools are available (`nvbootctrl` or `fw_printenv` / `fw_setenv`).
- [ ] Target slot partitions exist.
- [ ] The `.swu` bundle is accessible locally or through the configured download path.

Install:

```bash
aegis install /data/update.swu
```

Watch logs:

```bash
journalctl -u aegis -f
```

Check state:

```bash
aegis status
aegis get-primary
aegis get-booted
```

## 20. Quick Start

1. Confirm daemon is running:

```bash
systemctl status aegis
```

2. Confirm target config:

```bash
cat /etc/skytrack/system.conf
```

3. Copy bundle to target:

```bash
cp rootfs-update.swu /data/update.swu
```

4. Install bundle:

```bash
aegis install /data/update.swu
```

5. Monitor daemon logs:

```bash
journalctl -u aegis -f
```

6. Check slot state:

```bash
aegis get-primary
aegis get-booted
```