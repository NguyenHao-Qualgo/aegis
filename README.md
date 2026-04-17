# Aegis single-binary OTA starter

One executable provides:

- `aegis daemon`
- `aegis status`
- `aegis install <bundle>`
- `aegis mark-good`
- `aegis mark-bad`
- `aegis mark-active <A|B>`
- `aegis bundle create ...`

## Bundle creation

Aegis now creates bundles from artifact files, closer to RAUC slot/image sections.

Example:

```bash
aegis bundle create \
  --compatible qemuarm64 \
  --version 1.0.0 \
  --format plain \
  --output qemuarm64-1.0.0.aegisb \
  --artifact rootfs:archive:/tmp/rootfs.tar.gz \
  --artifact boot:file:/tmp/boot.tar.gz
```

Artifact syntax:

```text
--artifact <slot-class>:<type>:<path>
```

Supported bundle image `type` values in this starter are free-form metadata, but recommended values are:

- `archive` for tar/tar.gz payloads
- `raw` for raw images such as `rootfs.ext4`
- `file` for generic files

The generated `manifest.ini` uses RAUC-like image sections:

```ini
[update]
compatible=qemuarm64
version=1.0.0

[bundle]
format=plain

[image.rootfs]
slot-class=rootfs
source-type=file
type=archive
filename=rootfs.tar.gz
sha256=...
size=...
```

## Current installer limitation

The current OTA installer path in this starter still installs only the `rootfs` image when its manifest `type=archive`.
That keeps the daemon minimal while the bundle format is already moved closer to a RAUC-style multi-image manifest.
