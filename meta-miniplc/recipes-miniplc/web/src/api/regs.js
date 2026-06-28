/*
 * api/regs.js --- pool register access.
 *
 * Target contract (plc-firmware Phase 3, not yet implemented):
 *   GET /api/regs?start=N&count=M  ->  { start, count, values: [u16, ...] }
 *
 * Until that endpoint exists, getRegs() transparently falls back to a
 * generated demo slice so the UI is fully developable now. The returned
 * object carries `source: "live" | "demo"` so the UI can badge it honestly.
 * When the endpoint lands, nothing in the UI changes --- only `source`
 * flips to "live".
 */

import { api } from "./client.js";
import { SYS, SYS_READ_COUNT } from "../domain/sysmap.js";

let _demoStart = Date.now();

function demoRegs(start, count) {
  // Plausible, moving SYS values so the dashboard looks alive in demo mode.
  const regs = new Array(count).fill(0);
  const upSec = Math.floor((Date.now() - _demoStart) / 1000) + 2860;
  const epoch = Math.floor(Date.now() / 1000);
  const tempMilliC = 44000 + Math.round(Math.sin(Date.now() / 4000) * 1500); // ~42.5..45.5 C
  const hb = (Math.floor(Date.now() / 1000) % 65536) & 0xffff;

  const put32 = (idx, val) => {
    const v = val >>> 0;
    regs[idx] = v & 0xffff;
    regs[idx + 1] = (v >>> 16) & 0xffff;
  };

  if (start === 0) {
    put32(SYS.CPU_TEMP_MILLIC, tempMilliC >>> 0);
    put32(SYS.UPTIME_SEC, upSec);
    put32(SYS.EPOCH, epoch);
    regs[SYS.HEARTBEAT] = hb;
  }
  return regs;
}

/**
 * Read `count` 16-bit registers starting at `start`.
 * @returns {Promise<{start:number,count:number,values:number[],source:"live"|"demo"}>}
 */
export async function getRegs(start, count) {
  try {
    const data = await api.get(`/regs?start=${start}&count=${count}`);
    if (data && Array.isArray(data.values)) {
      return { start, count, values: data.values, source: "live" };
    }
    throw new Error("unexpected /api/regs payload");
  } catch {
    return { start, count, values: demoRegs(start, count), source: "demo" };
  }
}

/** Convenience: read exactly the SYS block needed for the Overview page. */
export function getSysRegs() {
  return getRegs(0, SYS_READ_COUNT);
}
