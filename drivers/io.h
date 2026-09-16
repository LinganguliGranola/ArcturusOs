#ifndef IO_H
#define IO_H

#include <stddef.h>
#include <stdint.h>

static inline uint8_t inb(uint16_t port) {
    uint8_t ret;
    __asm__ volatile ( "inb %1, %0" : "=a"(ret) : "Nd"(port) );
    return ret;
}

static inline void outb(uint16_t port, uint8_t val) {
    __asm__ volatile ( "outb %0, %1" : : "a"(val), "Nd"(port) );
}

/* Transfer a complete ATA sector without relying on a hosted C library. */
static inline void insw(uint16_t port, void *address, size_t count) {
    __asm__ volatile ("cld; rep insw"
                      : "+D"(address), "+c"(count)
                      : "d"(port)
                      : "memory");
}

static inline void outsw(uint16_t port, const void *address, size_t count) {
    __asm__ volatile ("cld; rep outsw"
                      : "+S"(address), "+c"(count)
                      : "d"(port));
}

#endif
