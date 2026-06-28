/*
 * domain/sysmap.js --- framework-agnostic decode of the SYS register block.
 *
 * Canonical layout from libmdcu-pool/files/mdcu_regmap.h (SYS range 0..99):
 *   CPU temp   i32 millicelsius  @ regs 0..1
 *   uptime     u32 seconds       @ regs 2..3
 *   epoch      u32 seconds       @ regs 4..5
 *   heartbeat  u16               @ reg  6
 *
 * The pool stores 16-bit registers. A 32-bit value occupies two of them.
 * We assume LOW word first (reg[n] = low 16 bits, reg[n+1] = high 16 bits),
 * matching little-endian ARM storage.
 * TODO: confirm word order against mdcu_pool.c typed accessors once the
 *       /api/regs endpoint defines its on-the-wire format.
 *
 * No React, no DOM, no CSS in this file --- pure data, fully reusable and
 * unit-testable. This is the contract the presentation layer binds to.
 */

export const SYS = {
  CPU_TEMP_MILLIC: 0, // i32, regs 0..1
  UPTIME_SEC: 2, // u32, regs 2..3
  EPOCH: 4, // u32, regs 4..5
  HEARTBEAT: 6, // u16, reg 6
};

export const SYS_READ_COUNT = 8; // regs 0..7 cover every SYS field above

const u16 = (v) => v & 0xffff;

export function u32From(regs, base) {
  const lo = u16(regs[base] ?? 0);
  const hi = u16(regs[base + 1] ?? 0);
  return (hi * 0x10000 + lo) >>> 0;
}

export function i32From(regs, base) {
  const v = u32From(regs, base);
  return v >= 0x80000000 ? v - 0x100000000 : v;
}

export function u16From(regs, base) {
  return u16(regs[base] ?? 0);
}

/**
 * Decode a raw SYS register slice into typed, display-ready system status.
 * @param {number[]} regs - array of 16-bit register values, index 0 == SYS+0
 */
export function decodeSystemStatus(regs) {
  const tempMilliC = i32From(regs, SYS.CPU_TEMP_MILLIC);
  return {
    cpuTempC: tempMilliC / 1000,
    uptimeSec: u32From(regs, SYS.UPTIME_SEC),
    epoch: u32From(regs, SYS.EPOCH),
    heartbeat: u16From(regs, SYS.HEARTBEAT),
  };
}

/** Human "1d 04:12:33" style uptime. */
export function formatUptime(sec) {
  if (!Number.isFinite(sec) || sec < 0) return "--";
  const d = Math.floor(sec / 86400);
  const h = Math.floor((sec % 86400) / 3600);
  const m = Math.floor((sec % 3600) / 60);
  const s = Math.floor(sec % 60);
  const pad = (n) => String(n).padStart(2, "0");
  return (d > 0 ? `${d}d ` : "") + `${pad(h)}:${pad(m)}:${pad(s)}`;
}

/** Epoch seconds -> local date-time string (or "--" when clock not set). */
export function formatEpoch(epoch) {
  if (!epoch || epoch < 1000000000) return "--";
  return new Date(epoch * 1000).toLocaleString();
}
