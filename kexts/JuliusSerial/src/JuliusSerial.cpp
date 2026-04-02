/*
 * kexts/JuliusSerial/src/JuliusSerial.cpp
 * JULIUS OS — I/O Kit UART Serial Driver Implementation
 *
 * Targets the ARM PL011 UART as found in QEMU's "virt" machine
 * at physical address 0x09000000.
 *
 * For x86_64 (QEMU q35), replace the physical base with 0x3F8 (COM1)
 * and use 8-bit I/O port access instead of MMIO — see the #ifdef below.
 *
 * Build: see build/build-kexts.sh
 */

#include "JuliusSerial.h"

OSDefineMetaClassAndStructors(JuliusSerialDriver, IOService)

/* Default UART base for QEMU virt (ARM64 PL011) */
static const IOPhysicalAddress kDefaultUARTBase = 0x09000000;
static const IOByteCount       kUARTRegionSize  = 0x1000;       /* 4 KiB */

/* ── Register accessors ─────────────────────────────────────────────────── */

inline uint32_t JuliusSerialDriver::reg_read(uint32_t offset) {
    return *(volatile uint32_t *)(_regs + offset);
}

inline void JuliusSerialDriver::reg_write(uint32_t offset, uint32_t value) {
    *(volatile uint32_t *)(_regs + offset) = value;
    /* Memory barrier: ensure the write reaches the device before we continue */
    __asm__ volatile ("" ::: "memory");
}

/* ── Hardware initialisation ────────────────────────────────────────────── */

void JuliusSerialDriver::hw_init(uint32_t baud) {
    /* 1. Disable UART before changing settings */
    reg_write(PL011_CR, 0);

    /* 2. Wait for any in-progress transmission to complete */
    int timeout = 100000;
    while ((reg_read(PL011_FR) & PL011_FR_BUSY) && timeout-- > 0)
        IODelay(1);

    /* 3. Clear the receive FIFO */
    while (!(reg_read(PL011_FR) & PL011_FR_RXFE))
        (void)reg_read(PL011_DR);

    /* 4. Set baud rate divisors (IBRD must be written before FBRD) */
    uint32_t ibrd = JULIUS_UART_CLOCK_HZ / (16 * baud);
    uint32_t fbrd = ((JULIUS_UART_CLOCK_HZ % (16 * baud)) * 64
                      + baud / 2) / baud;
    reg_write(PL011_IBRD, ibrd);
    reg_write(PL011_FBRD, fbrd);

    /* 5. 8 data bits, 1 stop bit, no parity, FIFO enabled */
    reg_write(PL011_LCRH, PL011_LCRH_WLEN_8 | PL011_LCRH_FEN);

    /* 6. Disable all interrupts (we poll in this minimal driver) */
    reg_write(PL011_IMSC, 0);
    reg_write(PL011_ICR,  0x7FF);   /* clear any pending interrupts */

    /* 7. Enable UART: TX + RX + UART enable */
    reg_write(PL011_CR, PL011_CR_TXE | PL011_CR_RXE | PL011_CR_UARTEN);

    IOLog("JuliusSerial: PL011 configured: %u baud, IBRD=%u FBRD=%u\n",
          baud, ibrd, fbrd);
}

/* ── IOService::start ───────────────────────────────────────────────────── */

bool JuliusSerialDriver::start(IOService *provider) {
    if (!super::start(provider)) {
        IOLog("JuliusSerial: super::start() failed\n");
        return false;
    }

    IOLog("JuliusSerial: starting\n");

    /* Read base address from device tree if available, else use default */
    _physBase = kDefaultUARTBase;
    OSData *regData = OSDynamicCast(OSData, provider->getProperty("reg"));
    if (regData && regData->getLength() >= sizeof(uint64_t)) {
        _physBase = (IOPhysicalAddress)(*(uint64_t *)regData->getBytesNoCopy());
        IOLog("JuliusSerial: using device-tree base 0x%llx\n",
              (unsigned long long)_physBase);
    } else {
        IOLog("JuliusSerial: no 'reg' property, using default 0x%llx\n",
              (unsigned long long)_physBase);
    }

    /* Map UART registers into kernel virtual address space */
    IOMemoryDescriptor *desc = IOMemoryDescriptor::withPhysicalAddress(
                                    _physBase, kUARTRegionSize, kIODirectionNone);
    if (!desc) {
        IOLog("JuliusSerial: cannot create memory descriptor\n");
        return false;
    }

    _mmioMap = desc->map();
    desc->release();

    if (!_mmioMap) {
        IOLog("JuliusSerial: cannot map UART MMIO at 0x%llx\n",
              (unsigned long long)_physBase);
        return false;
    }

    _regs = (volatile uint8_t *)_mmioMap->getVirtualAddress();
    IOLog("JuliusSerial: MMIO mapped at virt 0x%llx\n",
          (unsigned long long)(uintptr_t)_regs);

    /* Initialise the UART at 115200 baud */
    hw_init(JULIUS_UART_BAUD);

    /* Announce ourselves to the world */
    putString("\r\n[JuliusSerial] UART driver started. JULIUS OS serial console ready.\r\n");

    /* Publish our service so other kexts can find us */
    registerService();
    IOLog("JuliusSerial: registered as com.julius-os.driver.serial\n");

    return true;
}

/* ── IOService::stop ────────────────────────────────────────────────────── */

void JuliusSerialDriver::stop(IOService *provider) {
    IOLog("JuliusSerial: stopping\n");
    putString("\r\n[JuliusSerial] UART driver stopping.\r\n");

    /* Disable the UART */
    if (_regs) reg_write(PL011_CR, 0);

    if (_mmioMap) {
        _mmioMap->release();
        _mmioMap = nullptr;
        _regs    = nullptr;
    }

    super::stop(provider);
}

/* ── IOService::message ─────────────────────────────────────────────────── */

IOReturn JuliusSerialDriver::message(UInt32 type, IOService *provider,
                                      void *argument) {
    IOLog("JuliusSerial: message type=0x%x\n", type);
    return super::message(type, provider, argument);
}

/* ── Serial I/O ─────────────────────────────────────────────────────────── */

bool JuliusSerialDriver::tx_ready(void) {
    return !(reg_read(PL011_FR) & PL011_FR_TXFF);
}

bool JuliusSerialDriver::rx_ready(void) {
    return !(reg_read(PL011_FR) & PL011_FR_RXFE);
}

void JuliusSerialDriver::putChar(char c) {
    if (!_regs) return;
    /* Busy-wait for TX FIFO to have space (with a timeout to avoid lock-up) */
    int timeout = 100000;
    while (!tx_ready() && timeout-- > 0)
        IODelay(1);
    if (timeout <= 0) {
        IOLog("JuliusSerial: TX timeout!\n");
        return;
    }
    reg_write(PL011_DR, (uint32_t)(uint8_t)c);
}

bool JuliusSerialDriver::getChar(char *out) {
    if (!_regs || !rx_ready()) return false;
    *out = (char)(reg_read(PL011_DR) & 0xFF);
    return true;
}

void JuliusSerialDriver::putString(const char *s) {
    while (*s) {
        if (*s == '\n') putChar('\r');
        putChar(*s++);
    }
}

void JuliusSerialDriver::setBaud(uint32_t baud) {
    IOLog("JuliusSerial: changing baud to %u\n", baud);
    hw_init(baud);
}
