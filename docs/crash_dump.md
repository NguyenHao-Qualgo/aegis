# AEGIS also supports Google Breakpad for dump collection and analysis

[Google Breakpad](https://chromium.googlesource.com/breakpad/breakpad/) is a cross-platform crash reporting library. When an AEGIS process crashes, Breakpad intercepts the signal, writes a minidump (`.dmp`) file to disk, and exits - preserving the full register state, stack, and loaded modules at the moment of the crash.

---

## How it works

```
┌─────────────────────────────────┐
│  AEGIS process (aarch64 target) │
│                                 │
│  CrashHandler (RAII wrapper)    │
│  └─ ExceptionHandler installed  │
│                                 │
│  SIGSEGV / SIGABRT / etc.       │
│       │                         │
│       ▼                         │
│  Breakpad writes <uuid>.dmp     │
│  to /tmp (or configured path)   │
└─────────────────────────────────┘
         │  copy .dmp off device
         ▼
┌─────────────────────────────────┐
│  Build machine (x86-64)         │
│                                 │
│  analyze_crash.sh               │
│  ├─ dump_syms  → .sym file      │
│  └─ minidump_stackwalk          │
│       └─ symbolicated trace     │
└─────────────────────────────────┘
```

---

## Collecting a dump

On the target, crash dumps land in `/tmp` by default:

```bash
ls /tmp/*.dmp
# /tmp/6ae9981c-0c4c-45e9-fe82cf82-1b28ce7b.dmp

# Copy to build machine
scp root@<device-ip>:/tmp/*.dmp .
```

---

## Analyzing a dump

### Prerequisites

- Yocto build environment sourced (`oe-init-build-env`)
- The **unstripped** binary (Yocto puts it in the `-dbg` package split):

```
tmp/work/*/1.0/aeigs/git/packages-split/aegis-dbg/usr/bin/.debug/aegis
```

### Script

```bash
#!/bin/bash
set -e

DUMP_FILE="${1:?Usage: $0 <path/to/uuid.dmp> <path/to/unstripped-binary>}"
UNSTRIPPED="${2:?Usage: $0 <path/to/uuid.dmp> <path/to/unstripped-binary>}"
SYMBOLS_DIR="$(dirname "$0")/symbols"

if [ ! -f "$UNSTRIPPED" ]; then
    echo "ERROR: binary not found: $UNSTRIPPED" >&2
    exit 1
fi

if [ -z "$BUILDDIR" ] || ! command -v bitbake &>/dev/null; then
    echo "ERROR: Yocto environment not sourced. please setup:" >&2
    exit 1
fi

echo "[1/3] Populating native sysroot with breakpad-native tools..."
bitbake breakpad-native -c addto_recipe_sysroot

echo "[2/3] Extracting symbols from: $UNSTRIPPED"
SYM_TMP=$(mktemp /tmp/sym_XXXXXX.sym)

oe-run-native breakpad-native dump_syms "$UNSTRIPPED" \
    | grep -v '^Getting sysroot' > "$SYM_TMP"

HASH=$(grep '^MODULE' "$SYM_TMP" | awk '{print $4}')
BINARY_NAME=$(grep '^MODULE' "$SYM_TMP" | awk '{print $5}')

SYM_DEST="$SYMBOLS_DIR/$BINARY_NAME/$HASH"
mkdir -p "$SYM_DEST"
mv "$SYM_TMP" "$SYM_DEST/$BINARY_NAME.sym"
echo "    symbols: $SYM_DEST/$BINARY_NAME.sym  (hash=$HASH)"

echo "[3/3] Analyzing minidump: $DUMP_FILE"
echo "──────────────────────────────────────────────────────────────────────────"
oe-run-native breakpad-native minidump_stackwalk "$DUMP_FILE" "$SYMBOLS_DIR" 2>/dev/null
```

### Usage

```bash
# Source the Yocto environment first
source setup.sh <machine>

./analyze_crash.sh ./6ae9981c-....dmp \
    tmp/work/*/crash-dump/1.0/packages-split/crash-dump-dbg/usr/bin/.debug/crash_dump
```

### Expected output

```
Crash reason:  SIGSEGV /SEGV_MAPERR
Crash address: 0x0
Process uptime: not available

Thread 0 (crashed)
 0  aegis!std::_Function_handler<void(sdbus::MethodCall), sdbus::MethodVTableItem::implementedAs<aegis::DbusService::DbusService(aegis::OtaService&)::<lambda()> >(aegis::DbusService::DbusService(aegis::OtaService&)::<lambda()>&&)::<lambda(sdbus::MethodCall)> >::_M_invoke [dbus_service.cpp : 25 + 0x0]
     x0 = 0x0000007ffd6b64f0    x1 = 0x0000000000000000
     x2 = 0x000000000000002a    x3 = 0x000000558c66c120
     x4 = 0x000000558c683ab0    x5 = 0x6465746f6f427465
     x6 = 0x6465746f6f427465    x7 = 0x000000558c683a51
     x8 = 0x0000007ffd6b5cb8    x9 = 0x0000000000000000
    x10 = 0x0000002a0b563f18   x11 = 0x0000007f97cd0e80
    x12 = 0x0000007f968ff080   x13 = 0x0000000000001070
    x14 = 0x0000000000000001   x15 = 0x0000000000000000
    x16 = 0x0000007f9828fde8   x17 = 0x0000007f9824d510
    x18 = 0x0000007f980a39d8   x19 = 0x000000558c682d38
    x20 = 0x0000007ffd6b5c88   x21 = 0x0000007ffd6b5cb8
    x22 = 0x0000007ffd6b5e10   x23 = 0x0000007ffd6b5d30
    x24 = 0x0000000000000000   x25 = 0x0000000000000000
    x26 = 0x000000558c683160   x27 = 0x000000558c683610
    x28 = 0x0000007ffd6b5e28    fp = 0x0000007ffd6b5ce0
     lr = 0x000000558c5f7f14    sp = 0x0000007ffd6b5c80
     pc = 0x000000558c5f7f28
    Found by: given as instruction pointer in context
 1  aegis!std::_Function_handler<void(sdbus::MethodCall), sdbus::MethodVTableItem::implementedAs<aegis::DbusService::DbusService(aegis::OtaService&)::<lambda()> >(aegis::DbusService::DbusService(aegis::OtaService&)::<lambda()>&&)::<lambda(sdbus::MethodCall)> >::_M_invoke [Message.h : 260 + 0x0]
     fp = 0x0000007ffd6b5d50    lr = 0x0000007f982574b0
     sp = 0x0000007ffd6b5cf0    pc = 0x000000558c5f7f14
    Found by: previous frame's frame pointer
 2  libsdbus-c++.so.2 + 0x274ac
     fp = 0x0000007ffd6b5e80    lr = 0x0000007f97b669a4

```

---
