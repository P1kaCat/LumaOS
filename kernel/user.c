#include <stdint.h>
#include "cpu.h"
#include "sched.h"

extern const unsigned char user_code_start[];
extern const unsigned char user_code_end[];

#define USER_CODE_ADDR  0x800000ULL
#define USER_STACK_TOP  0xA00000ULL

void user_init(void) {
    serial_puts("[*] Creating user process...\n");

    uint8_t *dst = (uint8_t *)(unsigned long)USER_CODE_ADDR;
    const uint8_t *src = user_code_start;
    uint64_t len = user_code_end - user_code_start;
    for (uint64_t i = 0; i < len; i++)
        dst[i] = src[i];

    serial_puts("[+] User code copied to 0x800000\n");

    int pid = proc_create_user(USER_CODE_ADDR, USER_STACK_TOP);
    serial_puts("[+] User process PID=1 created\n");
    (void)pid;
}
