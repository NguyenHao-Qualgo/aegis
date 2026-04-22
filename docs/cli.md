# CLI Guide

This document describes the user-facing `aegis` client commands that talk to the daemon over DBus.

## Overview

When built with `AEGIS_ENABLE_DBUS=ON`, the same `aegis` binary provides a small CLI client for:

- `status`
- `install`
- `mark-active`
- `get-primary`
- `get-booted`
- `help`

The CLI connects to the system bus service:

- service: `de.skytrack.Aegis`
- object path: `/de/skytrack/Aegis`
- interface: `de.skytrack.Aegis1`

## Build

```bash
cmake -S . -B build -DAEGIS_ENABLE_DBUS=ON
cmake --build build --parallel
```

The binary is:

```bash
./build/aegis
```

## Commands

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
- an `https://`  (TODO)

During install, the client subscribes to `StatusChanged` and prints lines like:

```text
Install [verify] 15% - Verifying package signature
Install [install] 30% - Installing payload
```

The client exits when the daemon reports one of these terminal states:

- `Idle`
- `Reboot`
- `Failure`

Important behavior:

- pressing `Ctrl+C` only stops the client
- it does not cancel the install already running in the daemon

### `aegis mark-active <A|B>`

Set the next primary slot.

```bash
aegis mark-active A
```

The client only accepts `A` or `B`.

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

## Common Examples

Check status:

```bash
./build/aegis status
```

Install a local bundle:

```bash
./build/aegis install /data/update.swu
```

Install a remote bundle:

```bash
./build/aegis install https://example.com/update.swu
```

Mark slot `B` active:

```bash
./build/aegis mark-active B
```

Read slot state:

```bash
./build/aegis get-primary
./build/aegis get-booted
```

## Error Cases

Typical failures you may see:

- daemon not running on the system bus
- install requested without a bundle path
- `mark-active` called with something other than `A` or `B`
- daemon returns an install failure through `LastError`
- daemon rejects a second install while one is already in progress

## Relationship To The Daemon

The CLI is thin by design:

- it does not install bundles by itself
- it forwards requests to the daemon
- it renders DBus status into terminal-friendly output

See [daemon.md](daemon.md) for the DBus methods and service details, and [bundle.md](bundle.md) for how to generate the `.swu` bundle passed to `aegis install`.
