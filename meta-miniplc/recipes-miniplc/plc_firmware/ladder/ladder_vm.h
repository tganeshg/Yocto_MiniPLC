#ifndef LADDER_VM_H
#define LADDER_VM_H

#include <stddef.h>
#include <stdint.h>

/** Max instruction count for one program */
#define LADDER_MAX_INSTRUCTIONS 4096

/** Stack depth for boolean evaluation */
#define LADDER_STACK_DEPTH 128

/**
 * Load .plcbin from memory. Validates magic, CRC32, copies instructions.
 * @return 0 on success, -1 format / CRC error.
 */
int ladder_vm_load_blob(const uint8_t *data, size_t len);

void ladder_vm_clear_program(void);

/** True if a valid program is loaded (may still be STOPped at engine level). */
int ladder_vm_program_loaded(void);

/** Execute one full scan of the loaded program. No-op if not loaded. */
void ladder_vm_execute_scan(void);

#endif
