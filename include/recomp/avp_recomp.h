#ifndef AVP_RECOMP_H
#define AVP_RECOMP_H
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

enum {
    AVP_SR_RAM_SIZE  = 0x00200000u,
    AVP_SR_ROM_BASE  = 0x00800000u,
    AVP_SR_ROM_SIZE  = 0x00400000u,
};

typedef struct AvpSrCpu {
    uint32_t d[8];
    uint32_t a[8];
    uint32_t pc;
    uint16_t sr;
    uint8_t stopped;
    uint8_t faulted;
} AvpSrCpu;

typedef struct AvpSrHooks {
    void *user;
    uint32_t (*mmio_read32)(void *user, uint32_t addr);
    void (*mmio_write32)(void *user, uint32_t addr, uint32_t value);
    int (*call_external)(void *user, AvpSrCpu *cpu, uint32_t target);
    void (*trace_pc)(void *user, const AvpSrCpu *cpu, uint32_t pc);
} AvpSrHooks;

typedef struct AvpSrMachine {
    AvpSrCpu cpu;
    uint8_t *ram;
    size_t ram_size;
    const uint8_t *rom;
    size_t rom_size;
    AvpSrHooks hooks;
    uint32_t fault_addr;
    char fault_reason[128];
} AvpSrMachine;

void avp_sr_init(AvpSrMachine *m, uint8_t *ram, size_t ram_size,
                 const uint8_t *rom, size_t rom_size, const AvpSrHooks *hooks);
uint8_t  avp_sr_read8(AvpSrMachine *m, uint32_t addr);
uint16_t avp_sr_read16(AvpSrMachine *m, uint32_t addr);
uint32_t avp_sr_read32(AvpSrMachine *m, uint32_t addr);
void avp_sr_write8(AvpSrMachine *m, uint32_t addr, uint8_t value);
void avp_sr_write16(AvpSrMachine *m, uint32_t addr, uint16_t value);
void avp_sr_write32(AvpSrMachine *m, uint32_t addr, uint32_t value);
int avp_sr_call(AvpSrMachine *m, uint32_t target);
void avp_sr_fault(AvpSrMachine *m, uint32_t addr, const char *reason);

/* First statically-recompiled retail module: MAIN/ROM.S at $802000. */
int avp_sr_rom_s_802000(AvpSrMachine *m);

#ifdef __cplusplus
}
#endif
#endif
