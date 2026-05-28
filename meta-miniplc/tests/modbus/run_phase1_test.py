#!/usr/bin/env python3
"""
End-to-end Phase 1 test: Modbus TCP master inside miniplc-hmi.

Flow:
  1. Start modbus_sim.py on the host (port 5020, unit 1, preset reg values).
  2. Over SSH, write /tmp/modbus_config.txt on the Pi pointing at HOST_IP:5020.
  3. Restart /etc/init.d/miniplc-hmi.
  4. Poll /var/log/miniplc-hmi.log for `[MODBUS] Connected` and a read of the
     expected register values.
  5. Tear down: stop the sim, clear the config, restart the HMI clean.

Pass criteria:
  * HMI log shows `[MODBUS] Connected to <HOST>:5020 slave=1`
  * HMI log shows `[MODBUS] self-test OK: R0=1111 R1=2222 ... R9=10000`
"""
import os
import re
import signal
import subprocess
import sys
import time
from pathlib import Path

HOST_IP   = os.environ.get("HOST_IP",   "10.42.0.1")
PI_IP     = os.environ.get("PI_IP",     "10.42.0.252")
SIM_PORT  = int(os.environ.get("SIM_PORT", "5020"))
SSH_OPTS  = ["-o", "UserKnownHostsFile=/tmp/known_hosts_pi",
             "-o", "StrictHostKeyChecking=no"]

# Expected pattern in the HMI log after a successful poll.
EXPECT_CONNECT  = re.compile(rf"\[MODBUS\] Connected to {re.escape(HOST_IP)}:{SIM_PORT}")
EXPECT_SELFTEST = re.compile(
    r"\[MODBUS\] self-test OK:"
    r"\s*R0=1111\s+R1=2222\s+R2=3333\s+R3=4444\s+R4=5555"
    r"\s+R5=6666\s+R6=7777\s+R7=8888\s+R8=9999\s+R9=10000"
)


def ssh(cmd: str, check: bool = True) -> str:
    full = ["ssh"] + SSH_OPTS + [f"root@{PI_IP}", cmd]
    r = subprocess.run(full, capture_output=True, text=True, timeout=20)
    if check and r.returncode != 0:
        print(f"[ssh FAIL] cmd={cmd!r}\n  stdout={r.stdout!r}\n  stderr={r.stderr!r}",
              file=sys.stderr)
        raise RuntimeError(f"ssh failed: {cmd}")
    return r.stdout


def header(msg: str) -> None:
    print(f"\n=== {msg} " + "=" * (60 - len(msg)), flush=True)


def start_sim() -> subprocess.Popen:
    sim = Path(__file__).parent / "modbus_sim.py"
    proc = subprocess.Popen(
        [sys.executable, str(sim), "--port", str(SIM_PORT)],
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        text=True,
        preexec_fn=os.setsid,
    )
    # wait until it prints "listening"
    deadline = time.time() + 5
    while time.time() < deadline:
        line = proc.stdout.readline()
        if not line:
            time.sleep(0.1)
            continue
        print("  sim>", line.rstrip(), flush=True)
        if "listening" in line:
            return proc
    raise RuntimeError("simulator failed to come up")


def stop_sim(proc: subprocess.Popen) -> None:
    try:
        os.killpg(os.getpgid(proc.pid), signal.SIGTERM)
        proc.wait(timeout=3)
    except Exception:
        try:
            os.killpg(os.getpgid(proc.pid), signal.SIGKILL)
        except Exception:
            pass


def write_pi_config() -> None:
    # /tmp/modbus_config.txt format (see mb_load_config in modbus.c):
    #   line1: ip
    #   line2: port
    #   line3: slave_id
    #   line4: start_address
    #   line5: num_registers
    #   line6: enabled (0/1)
    body = f"{HOST_IP}\n{SIM_PORT}\n1\n0\n10\n1\n"
    # printf -e style with %b — busybox-friendly
    cmd = (
        "cat > /tmp/modbus_config.txt <<'EOF'\n"
        f"{body}"
        "EOF\n"
        "cat /tmp/modbus_config.txt"
    )
    out = ssh(cmd)
    print(out.rstrip())


def restart_hmi() -> None:
    ssh("/etc/init.d/miniplc-hmi stop ; sleep 1 ; "
        ": > /var/log/miniplc-hmi.log ; "
        "/etc/init.d/miniplc-hmi start", check=False)


def tail_log(seconds: int) -> str:
    """Poll the HMI log over SSH until both expected patterns appear or
    the deadline expires.  Returns whatever we collected."""
    deadline = time.time() + seconds
    last = ""
    while time.time() < deadline:
        try:
            last = ssh("cat /var/log/miniplc-hmi.log 2>/dev/null || true",
                       check=False)
        except Exception as e:
            print(f"  (ssh poll error: {e})", flush=True)
            time.sleep(1)
            continue
        if EXPECT_CONNECT.search(last) and EXPECT_SELFTEST.search(last):
            return last
        time.sleep(1)
    return last


def cleanup() -> None:
    ssh("/etc/init.d/miniplc-hmi stop ; "
        "rm -f /tmp/modbus_config.txt ; "
        ": > /var/log/miniplc-hmi.log ; "
        "/etc/init.d/miniplc-hmi start", check=False)


def main() -> int:
    header(f"Phase 1 test  HOST={HOST_IP}  PI={PI_IP}  PORT={SIM_PORT}")

    sim = None
    try:
        header("Start Modbus simulator on host")
        sim = start_sim()

        header("Write Modbus config on Pi")
        write_pi_config()

        header("Restart miniplc-hmi (HMI auto-connects on startup)")
        restart_hmi()

        header("Watch /var/log/miniplc-hmi.log on Pi (up to 20s)")
        log = tail_log(20)
        print(log.rstrip())

        header("Verdict")
        ok_conn = bool(EXPECT_CONNECT.search(log))
        ok_read = bool(EXPECT_SELFTEST.search(log))
        print(f"  connect log line      : {'PASS' if ok_conn else 'FAIL'}")
        print(f"  read self-test values : {'PASS' if ok_read else 'FAIL'}")
        rc = 0 if (ok_conn and ok_read) else 1

        return rc
    finally:
        if sim is not None:
            header("Stop simulator")
            stop_sim(sim)
        header("Cleanup Pi (disable Modbus, fresh log)")
        cleanup()


if __name__ == "__main__":
    sys.exit(main())
