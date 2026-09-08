#ifndef USER_API_H
#define USER_API_H
#include <stdint.h>
#include "../include/graphics_abi.h"
#include "../include/input_abi.h"
static inline long call3(long number, uint64_t a, uint64_t b, uint64_t c) {
    __asm__ volatile("int $0x80" : "+a"(number) : "D"(a), "S"(b), "d"(c) : "memory", "cc");
    return number;
}
static inline void say(const char *s) { call3(0, (uintptr_t)s, 0, 0); }
__attribute__((noreturn)) static inline void done(int status) {
    call3(1, status, 0, 0);
    for (;;) __asm__ volatile("pause");
}
static inline void sleep_ticks(unsigned ticks) {
    call3(5, ticks, 0, 0);
    /* sleep marks the task sleeping; the timer performs the actual switch. */
    call3(6, 0, 0, 0);
}
#endif
