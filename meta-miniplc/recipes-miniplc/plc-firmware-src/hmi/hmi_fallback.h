/**
 * Local LCD (LVGL) default when no hmi_manifest / no screens are deployed.
 * See plan.md HMI-07, §12.2
 */
#ifndef HMI_FALLBACK_H
#define HMI_FALLBACK_H

/** OEM-visible panel text — LVGL loader uses this when manifest is absent or empty. */
#define HMI_FALLBACK_LABEL_TEXT "No UI configured"

/** Default deployed manifest path after apply (align with REST / filesystem policy). */
#define HMI_MANIFEST_PRIMARY_PATH "/var/lib/plc/hmi/hmi_manifest.json" /* keep in sync with core/paths.h */

#endif
