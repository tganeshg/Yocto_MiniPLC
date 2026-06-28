import { usePoll } from "./usePoll.js";
import { getSysRegs } from "../api/regs.js";
import { decodeSystemStatus } from "../domain/sysmap.js";

/*
 * useSystemStatus --- the Overview page's data source.
 * Polls the SYS register block, decodes it, and reports whether the data
 * is live (from the device) or demo (stubbed). The page binds to this and
 * stays unaware of transport details.
 */
export function useSystemStatus(intervalMs = 2000) {
  const { data, error, loading } = usePoll(getSysRegs, intervalMs);
  const status = data ? decodeSystemStatus(data.values) : null;
  return {
    status,
    source: data?.source ?? null, // "live" | "demo"
    loading,
    error,
  };
}
