/*
 * kexts/JuliusSerial/include/JuliusSerial.h
 * JULIUS OS — I/O Kit UART Serial Driver
 *
 * Supports PL011 (ARM64/QEMU virt) and 16550 (x86_64/QEMU q35).
 * The driver registers itself as "com.julius-os.driver.serial" so that
 * other kexts can look it up via IOServiceGetMatchingService().
 */

#pragma once

#include <IOKit/IOService.h>
#include <IOKit/IOMemoryDescriptor.h>
#include <IOKit/IOLib.h>

/* ── Register layout (PL011) ────────────────────────────────────────────── */
/* Base address is read from the device tree node "reg" property.          */

#define PL011_DR        0x000   /* Data register (read = rx, write = tx)   */
#define PL011_RSR       0x004   /* Receive status / error clear             */
#define PL011_FR        0x018   /* Flag register                            */
#define PL011_IBRD      0x024   /* Integer baud rate divisor                */
#define PL011_FBRD      0x028   /* Fractional baud rate divisor             */
#define PL011_LCRH      0x02C   /* Line control register                    */
#define PL011_CR        0x030   /* Control register                         */
#define PL011_IMSC      0x038   /* Interrupt mask set/clear                 */
#define PL011_ICR       0x044   /* Interrupt clear register                 */

/* FR (Flag register) bits */
#define PL011_FR_TXFF   (1u << 5)  /* TX FIFO full  */
#define PL011_FR_RXFE   (1u << 4)  /* RX FIFO empty */
#define PL011_FR_BUSY   (1u << 3)  /* UART busy     */

/* LCRH bits */
#define PL011_LCRH_WLEN_8   (3u << 5)  /* 8-bit word length */
#define PL011_LCRH_FEN      (1u << 4)  /* FIFO enable       */

/* CR bits */
#define PL011_CR_RXE    (1u << 9)   /* Receive enable  */
#define PL011_CR_TXE    (1u << 8)   /* Transmit enable */
#define PL011_CR_UARTEN (1u << 0)   /* UART enable     */

/* ── Baud rate divisors (24 MHz ref clock, 115200 baud) ─────────────────── */
#define JULIUS_UART_CLOCK_HZ    24000000UL
#define JULIUS_UART_BAUD        115200UL
#define JULIUS_IBRD     (JULIUS_UART_CLOCK_HZ / (16 * JULIUS_UART_BAUD))
#define JULIUS_FBRD     (((JULIUS_UART_CLOCK_HZ % (16 * JULIUS_UART_BAUD)) \
                           * 64 + JULIUS_UART_BAUD / 2) / JULIUS_UART_BAUD)

/* ── Driver class ───────────────────────────────────────────────────────── */

class JuliusSerialDriver : public IOService
{
    OSDeclareDefaultStructors(JuliusSerialDriver)

public:
    /* IOService lifecycle */
    bool        start(IOService *provider)  override;
    void        stop(IOService *provider)   override;
    IOReturn    message(UInt32 type, IOService *provider,
                        void *argument = NULL) override;

    /* Public serial API — used by other kexts or IOUserClient */
    void        putChar(char c);
    bool        getChar(char *out);          /* non-blocking; false if empty */
    void        putString(const char *s);
    void        setBaud(uint32_t baud);

private:
    IOMemoryMap         *_mmioMap   = nullptr;
    volatile uint8_t    *_regs      = nullptr;
    IOPhysicalAddress    _physBase  = 0;

    /* Helpers */
    ALWAYS_INLINE uint32_t  reg_read(uint32_t offset);
    ALWAYS_INLINE void      reg_write(uint32_t offset, uint32_t value);
    void                    hw_init(uint32_t baud);
    bool                    tx_ready(void);
    bool                    rx_ready(void);
};
