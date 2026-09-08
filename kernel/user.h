/* user.h — User mode (Ring 3) + syscalls */
#ifndef LUMAOS_USER_H
#define LUMAOS_USER_H
#include <stdint.h>

void user_init(void);
/* Release dynamic pages and return the static address-space slot to the pool. */
void user_release_address_space(uint64_t cr3);
int spawn_shell(void);
int spawn_file(const char *path);  /* Phase 6: ELF64 loader from FAT32 */

#endif
