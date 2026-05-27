#!/usr/bin/env bash
# Smoke test MiniPLC stack on a running device (after flash).
# Usage: ./miniplc-smoke-test.sh [device_ip]
#   or:  MINIPLC_IP=192.168.1.50 ./miniplc-smoke-test.sh
set -euo pipefail

IP="${1:-${MINIPLC_IP:-}}"
if [[ -z "${IP}" ]]; then
  echo "Usage: $0 <device_ip>   or set MINIPLC_IP"
  exit 1
fi

BASE="http://${IP}"
API="${BASE}/api"

echo "=== MiniPLC smoke test → ${BASE} ==="
echo

pass=0
fail=0

check() {
  local name="$1" url="$2"
  local code
  code=$(curl -sS -o /tmp/miniplc-st.json -w "%{http_code}" "${url}" || echo "000")
  if [[ "${code}" =~ ^2 ]]; then
    echo "[OK]  ${name} (${code})"
    ((pass++)) || true
    if [[ "${VERBOSE:-}" == "1" ]]; then
      head -c 400 /tmp/miniplc-st.json | sed 's/^/      /' || true
      echo
    fi
  else
    echo "[FAIL] ${name} HTTP ${code}"
    ((fail++)) || true
  fi
}

echo "--- HTTP / SPA ---"
check "nginx + SPA root" "${BASE}/"
echo

echo "--- REST (via nginx proxy) ---"
check "GET /api/status" "${API}/status"
check "GET /api/system/version" "${API}/system/version"
check "GET /api/config" "${API}/config"
check "GET /api/registers (holding)" "${API}/registers?type=holding&start=0&count=4"
check "GET /api/io" "${API}/io"
check "GET /api/plugins" "${API}/plugins"
check "GET /api/system/update/status" "${API}/system/update/status"
check "GET /api/hmi/status" "${API}/hmi/status"
echo

echo "--- Summary: ${pass} passed, ${fail} failed ---"
if [[ "${fail}" -gt 0 ]]; then
  echo "Hints: ping ${IP}; ssh root@${IP} 'systemctl status plc-firmware nginx'"
  exit 1
fi
exit 0
