/* user.h — User mode (Ring 3) + syscalls */
#ifndef LUMAOS_USER_H
#define LUMAOS_USER_H

void user_init(void);
int spawn_shell(void);
int spawn_file(const char *path);  /* Phase 6: ELF64 loader from FAT32 */

#endif
