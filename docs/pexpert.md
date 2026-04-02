# JULIUS OS — Platform Expert (pexpert)

The Platform Expert (`pexpert/`) is XNU's hardware abstraction layer — the
boundary between the generic kernel and the specific hardware platform. It is
the first JULIUS-controlled code that runs during boot (before VM is set up),
and it is where we will add support for our custom ARM64 chip.

---

## What pexpert does

1. **Early console output** — `PE_early_puts()` / `PE_init_iokit()` provide
   `kprintf()` output before any drivers load.
2. **Boot argument parsing** — `PE_parse_boot_argn()` extracts values from
   the bootloader's argument string.
3. **Clock frequency** — `PE_i_can_has_debugger()` returns the CPU frequency
   used by delay loops.
4. **Device tree** — On ARM64, pexpert parses the Apple Device Tree (ADT)
   blob passed by the bootloader.
5. **Platform identification** — `PEGetPlatformName()` returns a string
   that I/O Kit uses to match platform-specific drivers.

---

## Relevant source files in XNU

```
xnu/pexpert/
├── gen/
│   ├── bootargs.c      ← PE_parse_boot_argn() implementation
│   └── pe_gen.c        ← generic helpers
├── i386/
│   ├── pe_init.c       ← x86 PE_init_platform()
│   └── pe_serial.c     ← x86 serial console (16550 UART)
├── arm/
│   └── pe_init.c       ← ARM PE (older ARM32, reference)
├── arm64/
│   ├── pe_init.c       ← ARM64 PE_init_platform(), our main target
│   ├── pe_serial.c     ← ARM64 UART implementations (PL011, S3C, etc.)
│   └── pl011.c         ← PL011 UART driver (used by QEMU virt)
└── pexpert.h           ← The interface all PE implementations must satisfy
```

---

## The PE interface (`pexpert.h`)

Every platform must implement these functions:

```c
/* Phase 1: called before VM is initialized.
   Set up the early serial console. Physical addresses only. */
void PE_init_platform(boolean_t vm_initialized, void *args);

/* Phase 2: called after VM. Map UART into virtual space. */
void PE_init_iokit(void);

/* Output len bytes to the early console (before IOLog works). */
void PE_early_puts(char *str, int size);

/* Get a named value from the boot argument string. */
int  PE_parse_boot_argn(const char *arg_string, void *arg_ptr, int max_arg);

/* Return the platform name string (used by IOKit matching). */
boolean_t PEGetPlatformName(char *name, int namelen);

/* Return the machine name. */
boolean_t PEGetMachineName(char *name, int namelen);
```

---

## Adding a custom platform

To add support for a new chip (e.g., "JuliusChip A1"), create:
```
xnu/pexpert/arm64/julius_chip.c
```

Minimal implementation:

```c
/* xnu/pexpert/arm64/julius_chip.c */
#include <pexpert/pexpert.h>
#include <pexpert/arm64/board_config.h>

/* ── Your chip's UART base address ────────────────────────────────────── */
/* Replace with the real MMIO address from your chip's datasheet.        */
#define JULIUS_CHIP_UART_BASE   0x10000000ULL
#define JULIUS_CHIP_UART_SIZE   0x1000

/* PL011 register offsets (adjust if your UART is different) */
#define UART_DR     0x000
#define UART_FR     0x018
#define UART_FR_TXFF (1u << 5)
#define UART_IBRD   0x024
#define UART_FBRD   0x028
#define UART_LCRH   0x02C
#define UART_CR     0x030

static volatile uint32_t *uart_regs = NULL;

static void uart_putc_early(char c) {
    /* Phase 1: use raw physical address */
    volatile uint32_t *regs = (volatile uint32_t *)JULIUS_CHIP_UART_BASE;
    while (regs[UART_FR / 4] & UART_FR_TXFF)
        ;
    regs[UART_DR / 4] = (uint32_t)(uint8_t)c;
}

void PE_init_platform(boolean_t vm_initialized, void *args) {
    if (!vm_initialized) {
        /* Phase 1: Physical address access only */
        uart_putc_early('J');   /* Proof of life */
        return;
    }

    /* Phase 2: VM is set up — remap UART into kernel virtual space */
    uart_regs = (volatile uint32_t *)ml_io_map(JULIUS_CHIP_UART_BASE,
                                                JULIUS_CHIP_UART_SIZE);

    /* Configure UART: 115200 baud, 8N1 (for 24 MHz ref clock) */
    uart_regs[UART_CR   / 4] = 0;        /* disable */
    uart_regs[UART_IBRD / 4] = 13;       /* 24000000 / (16 * 115200) */
    uart_regs[UART_FBRD / 4] = 1;
    uart_regs[UART_LCRH / 4] = 0x70;    /* 8N1, FIFO enable */
    uart_regs[UART_CR   / 4] = 0x301;   /* enable TX+RX+UART */

    kprintf("JuliusChip: platform expert initialised\n");
}

void PE_early_puts(char *str, int size) {
    for (int i = 0; i < size; i++) {
        if (str[i] == '\n') uart_putc_early('\r');
        uart_putc_early(str[i]);
    }
}

boolean_t PEGetPlatformName(char *name, int namelen) {
    strlcpy(name, "Julius Platform", namelen);
    return TRUE;
}

boolean_t PEGetMachineName(char *name, int namelen) {
    strlcpy(name, "JuliusChip A1", namelen);
    return TRUE;
}
```

Then add `julius_chip.c` to `xnu/pexpert/arm64/Makefile.pexpert` and
rebuild the kernel.

---

## Device Tree (ARM64)

On ARM64, XNU reads hardware configuration from a binary "Apple Device Tree"
(not the Linux FDT/DTB format — incompatible). The bootloader must build this
blob and pass it in `boot_args.deviceTreeP`.

A minimal device tree for a custom board includes:

```
chosen {
    boot-args = "serial=3 -v debug=0x14e";
    memory-size = <0x40000000>;   /* 1 GiB */
}

cpus {
    cpu@0 {
        compatible = "arm,cortex-a57";
        reg = <0x0>;
    }
}

uart@10000000 {
    compatible = "arm,pl011";
    reg = <0x10000000 0x1000>;
    interrupts = <0 1 4>;
}

memory@40000000 {
    device_type = "memory";
    reg = <0x40000000 0x40000000>;
}
```

Apple's DTB format is documented in `iokit/IOKit/IODeviceTreeSupport.h`
and parsed in `pexpert/gen/pe_gen.c`.

Tools for creating Apple device trees:
- `adt2dtb` (community tool, converts Apple DT to Linux DTB for reference)
- The PureDarwin project has scripts for building device trees for QEMU.
