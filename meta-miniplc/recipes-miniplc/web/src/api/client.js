/*
 * api/client.js --- thin, framework-agnostic HTTP client.
 *
 * All requests are relative to "/api" so they work identically in:
 *   - production: nginx reverse-proxies /api/* to civetweb :5080
 *   - dev: vite proxies /api -> VITE_API_TARGET (the device)
 *
 * No React here. The presentation layer consumes this via hooks only.
 */

const BASE = "/api";
const DEFAULT_TIMEOUT_MS = 5000;

export class ApiError extends Error {
  constructor(message, status) {
    super(message);
    this.name = "ApiError";
    this.status = status;
  }
}

async function request(path, { method = "GET", body, timeout = DEFAULT_TIMEOUT_MS } = {}) {
  const ctrl = new AbortController();
  const timer = setTimeout(() => ctrl.abort(), timeout);
  try {
    const res = await fetch(BASE + path, {
      method,
      headers: body ? { "Content-Type": "application/json" } : undefined,
      body: body ? JSON.stringify(body) : undefined,
      signal: ctrl.signal,
    });
    if (!res.ok) throw new ApiError(`HTTP ${res.status} on ${path}`, res.status);
    const text = await res.text();
    return text ? JSON.parse(text) : null;
  } finally {
    clearTimeout(timer);
  }
}

export const api = {
  get: (path, opts) => request(path, { ...opts, method: "GET" }),
  post: (path, body, opts) => request(path, { ...opts, method: "POST", body }),
};
