# kernel/

This directory contains the XNU kernel source tree (as a git submodule)
and JULIUS OS patches on top of it.

## Structure

```
kernel/
├── xnu/                    ← Apple XNU source (git submodule)
├── dtrace/                 ← dtrace CTF tools (git submodule)
├── AvailabilityVersions/   ← SDK headers (git submodule)
└── patches/                ← JULIUS OS kernel patches
    ├── 0001-julius-pexpert.patch
    └── 0002-julius-boot-args.patch
```

## Setting up the submodules

```bash
git submodule update --init --recursive
```

This pulls the three Apple open-source repositories. The XNU repo is large
(~500 MiB) — the download takes a few minutes.

## Choosing an XNU tag

XNU version numbers correspond to macOS releases:

| XNU tag | macOS |
|---|---|
| xnu-10002.x | macOS 15 |
| xnu-8796.x  | macOS 14 |
| xnu-8020.x  | macOS 13 |

Pin to a specific tag for reproducibility:

```bash
cd kernel/xnu
git checkout xnu-10002.41.9.1
cd ../..
git add kernel/xnu
git commit -m "kernel: pin XNU to 10002.41.9.1"
```

## Applying JULIUS patches

```bash
cd kernel/xnu
git am ../../kernel/patches/*.patch
```

## Kernel configuration

XNU build configs live in `xnu/config/`. The key variables:

| Variable | Values | Effect |
|---|---|---|
| `KERNEL_CONFIGS` | `DEVELOPMENT` / `RELEASE` | Assertions, kprintf, debug symbols |
| `ARCH_CONFIGS` | `X86_64` / `ARM64` | Target ISA |
| `MACHINE_CONFIG` | `DEFAULT` / `VMAPPLE` | Platform variant |

For JULIUS OS development, always use `DEVELOPMENT`.

## pexpert changes

Our platform expert additions live in:
```
xnu/pexpert/arm64/julius_pe.c    ← ARM64 UART + boot args
xnu/pexpert/i386/julius_pe.c     ← x86 additions
```

These are applied via the patches above. See `docs/pexpert.md` for details.
