#ifndef PROJECT_STORE_H
#define PROJECT_STORE_H

/**
 * Unzip upload to staging, install ladder + HMI, reload VM, start PLC logic.
 * @return 0 ok, negative errno-style
 */
int project_store_apply_zip(void);

/** Load persisted program.bin into ladder VM (boot). */
int project_store_load_persisted_program(void);

#endif
