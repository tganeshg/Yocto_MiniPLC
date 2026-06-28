import { defineConfig } from "vite";
import react from "@vitejs/plugin-react";

// The built assets are served statically by nginx at the device root ("/"),
// and "/api/*" is reverse-proxied to civetweb on :5080 (see miniplc.conf).
//
// For local development against a real device, point VITE_API_TARGET at the
// Pi, e.g.  VITE_API_TARGET=http://10.42.0.252 npm run dev
const API_TARGET = process.env.VITE_API_TARGET || "http://10.42.0.252";

export default defineConfig({
  plugins: [react()],
  // Relative base so the bundle works regardless of the mount path.
  base: "./",
  server: {
    host: true,
    port: 5173,
    proxy: {
      "/api": {
        target: API_TARGET,
        changeOrigin: true,
      },
    },
  },
  build: {
    outDir: "dist",
    emptyOutDir: true,
  },
});
