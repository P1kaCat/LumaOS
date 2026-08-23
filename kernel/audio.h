/* audio.h — Intel High Definition Audio (HDA) Controller Driver
 *
 * Phase 7e: Intel HDA controller discovery, CORB/RIRB setup and codec discovery.
 */
#ifndef LUMAOS_AUDIO_H
#define LUMAOS_AUDIO_H

#include <stdint.h>
#include "pci.h"

/* HDA Register Offsets */
#define HDA_REG_GCAP       0x00
#define HDA_REG_VMIN       0x02
#define HDA_REG_VMAJ       0x03
#define HDA_REG_OUTPAY     0x04
#define HDA_REG_INPAY      0x06
#define HDA_REG_GCTL       0x08
#define HDA_REG_WAKEEN     0x0C
#define HDA_REG_STATESTS   0x0E
#define HDA_REG_GSTS       0x10
#define HDA_REG_CORBLBASE  0x40
#define HDA_REG_CORBUBASE  0x44
#define HDA_REG_CORBWP     0x48
#define HDA_REG_CORBRP     0x4A
#define HDA_REG_CORBCTL    0x4C
#define HDA_REG_CORBSTS    0x4D
#define HDA_REG_CORBSIZE   0x4E
#define HDA_REG_RIRBLBASE  0x50
#define HDA_REG_RIRBUBASE  0x54
#define HDA_REG_RIRBWP     0x58
#define HDA_REG_RINTCNT    0x5A
#define HDA_REG_RIRBCTL    0x5C
#define HDA_REG_RIRBSTS    0x5D
#define HDA_REG_RIRBSIZE   0x5E

#define HDA_GCTL_CRST      (1u << 0)
#define HDA_CORBCTL_RUN    (1u << 1)
#define HDA_RIRBCTL_RUN    (1u << 1)

/* Discover and initialize Intel HDA controller */
void audio_init(void);

#endif /* LUMAOS_AUDIO_H */
