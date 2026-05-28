#!/usr/bin/env python3
"""
Modbus TCP slave (server) simulator used by run_phase1_test.py.

Listens on 0.0.0.0:<port> (default 5020 — unprivileged), unit id 1.
Pre-seeds holding registers 0..9 with the values:

    R0=1111  R1=2222  R2=3333  R3=4444  R4=5555
    R5=6666  R6=7777  R7=8888  R8=9999  R9=10000

so the Pi-side log makes it obvious when a real read succeeds.

Dependencies (host): pymodbus >= 3.13 (uses the new SimData/SimDevice API)
    pip install --user --break-system-packages pymodbus
"""
import argparse
import asyncio
import logging
import sys

from pymodbus.simulator import SimData, SimDevice
from pymodbus.simulator.simdata import DataType
from pymodbus.server import StartAsyncTcpServer

PORT_DEFAULT = 5020
SEED = [1111, 2222, 3333, 4444, 5555, 6666, 7777, 8888, 9999, 10000]


def build_device() -> SimDevice:
    """Build a SimDevice with our SEED values in the holding-register block."""
    # pymodbus 3.13: passing the 4-tuple form crashes when any block is empty
    # (SimDevice.__check_block indexes [0] without a length check).  Pass a
    # flat list instead — datatype=HOLDING_REGISTERS routes it correctly.
    holding = SimData(address=0, count=len(SEED), values=SEED,
                      datatype=DataType.REGISTERS)
    return SimDevice(id=1, simdata=[holding])


async def serve(host: str, port: int) -> None:
    dev = build_device()
    print(f"[SIM] Modbus TCP slave listening on {host}:{port} unit=1", flush=True)
    print(f"[SIM] holding regs 0..{len(SEED)-1} = {SEED}", flush=True)
    await StartAsyncTcpServer(context=[dev], address=(host, port))


def main() -> int:
    p = argparse.ArgumentParser()
    p.add_argument("--host", default="0.0.0.0")
    p.add_argument("--port", type=int, default=PORT_DEFAULT)
    p.add_argument("-v", "--verbose", action="store_true")
    args = p.parse_args()

    if args.verbose:
        logging.basicConfig(level=logging.DEBUG)

    try:
        asyncio.run(serve(args.host, args.port))
    except KeyboardInterrupt:
        print("[SIM] shutdown", flush=True)
    return 0


if __name__ == "__main__":
    sys.exit(main())
