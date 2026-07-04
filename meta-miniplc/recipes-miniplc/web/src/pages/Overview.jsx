import { Fragment, useEffect, useRef, useState } from "react";
import { useSystemStatus } from "../hooks/useSystemStatus.js";
import { formatUptime } from "../domain/sysmap.js";

/*
 * Overview --- "Command Center" dashboard.
 *
 * LIVE data (from the SYS register pool via useSystemStatus): CPU temperature,
 * uptime, and the LIVE/STALE/DEMO freshness pill.
 *
 * The blocks below (device identity, CPU load / memory gauges, protocol status,
 * network & fieldbus interfaces) have no backend feed yet. They render
 * representative PLACEHOLDER values so the layout is complete; wiring each to a
 * real endpoint later is a data-only swap (replace the const with a hook).
 */

// ---- static placeholders (pending firmware / API) ------------------------
const DEVICE = { model: "FXD-iX-R1", firmware: "v0.4.1", ui: "v0.1.0", serial: "FX24-000137" };
const LOAD_PCT = 37; // CPU load %  (placeholder)
const MEM_PCT = 61; // memory %    (placeholder)

const PROTOCOLS = [
  { name: "Modbus TCP", state: "ok", detail: "Running \u00B7 12 tags" },
  { name: "MQTT", state: "ok", detail: "Connected" },
  { name: "OPC UA", state: "warn", detail: "Idle" },
  { name: "PLC Runtime", state: "ok", detail: "Scan 5 ms" },
];

const UPLINKS = [
  {
    name: "LAN 1", type: "Ethernet", state: "ok", statusText: "Up",
    rows: [["IP Address", "10.42.0.252"], ["Netmask", "255.255.255.0"], ["Gateway", "10.42.0.1"]],
  },
  {
    name: "LAN 2", type: "Ethernet", state: "ok", statusText: "Up",
    rows: [["IP Address", "192.168.1.50"], ["Netmask", "255.255.255.0"], ["Gateway", "192.168.1.1"]],
  },
  {
    name: "Wi-Fi", type: "Wireless", state: "ok", tag: { text: "STA", kind: "sta" },
    rows: [
      ["SSID", "PlantFloor-5G"], ["Signal", "-58 dBm"], ["IP Address", "192.168.4.23"],
      ["Netmask", "255.255.255.0"], ["Gateway", "192.168.4.1"],
    ],
  },
];
const UPLINK_COMMON = { defaultRoute: "LAN 1 \u00B7 10.42.0.1", dns: "8.8.8.8, 1.1.1.1" };

const FIELDBUS = [
  {
    name: "Serial 1", type: "RS-232", state: "ok", statusText: "Up", bound: "",
    rows: [["Baud", "115200"], ["Frame", "8 / None / 1"], ["Flow", "None"]],
  },
  {
    name: "Serial 2", type: "RS-485", state: "ok", tag: { text: "2-Wire", kind: "sta" }, bound: "Modbus RTU",
    rows: [["Baud", "9600"], ["Frame", "8 / Even / 1"], ["Termination", "On (120\u03A9)"]],
  },
  {
    name: "CAN 1", type: "CAN 2.0B", state: "ok", statusText: "Up", bound: "",
    rows: [["Bitrate", "250 kbit/s"], ["Mode", "Normal"], ["Bus Load", "12%"]],
  },
];

const STALE_AFTER_MS = 5000;

const tempPct = (c) => Math.max(0, Math.min(100, (c / 85) * 100));
const tempColor = (c) =>
  c >= 75 ? "var(--mdcu-danger)" : c >= 60 ? "var(--mdcu-warning)" : "var(--mdcu-primary)";

function Dot({ state }) {
  return <span className={`ov-dot ov-${state}`} />;
}

function Gauge({ label, value, unit, pct, color }) {
  return (
    <div className="ov-gauge">
      <div className="ov-ring" style={{ "--p": `${pct}%`, "--gc": color }}>
        <div className="ov-ring-in">
          <b>{value}</b>
          <span>{unit}</span>
        </div>
      </div>
      <div className="ov-gauge-label">{label}</div>
    </div>
  );
}

function Iface({ item }) {
  return (
    <div className="ov-iface">
      <div className="ov-iface-head">
        <Dot state={item.state} />
        <div>
          <div className="ov-iface-name">{item.name}</div>
          <div className="ov-iface-type">{item.type}</div>
        </div>
        {item.tag ? (
          <span className={`ov-mode ov-mode-${item.tag.kind}`}>{item.tag.text}</span>
        ) : (
          <span className="ov-iface-status">
            <Dot state={item.state} />
            {item.statusText}
          </span>
        )}
      </div>
      <dl className="ov-net">
        {item.rows.map(([k, v]) => (
          <Fragment key={k}>
            <dt>{k}</dt>
            <dd>{v}</dd>
          </Fragment>
        ))}
      </dl>
      {item.bound !== undefined && (
        <div className="ov-bound">
          Bound to <b>{item.bound || "\u2014"}</b>
          {item.bound ? "" : " (free)"}
        </div>
      )}
    </div>
  );
}

export default function Overview() {
  const { status, source } = useSystemStatus(2000);
  const [lastUpdate, setLastUpdate] = useState(null);
  const [now, setNow] = useState(Date.now());
  const lastHb = useRef(null);

  // Mark a fresh sample whenever the heartbeat advances.
  useEffect(() => {
    if (status && status.heartbeat !== lastHb.current) {
      lastHb.current = status.heartbeat;
      setLastUpdate(Date.now());
    }
  }, [status]);

  // 1 Hz ticker so "updated Xs ago" and STALE detection stay current.
  useEffect(() => {
    const id = setInterval(() => setNow(Date.now()), 1000);
    return () => clearInterval(id);
  }, []);

  const agoMs = lastUpdate ? now - lastUpdate : null;
  let pill;
  if (source === "demo") pill = { state: "demo", text: "DEMO", icon: "bi-cone-striped" };
  else if (agoMs != null && agoMs > STALE_AFTER_MS)
    pill = { state: "stale", text: "STALE", icon: "bi-exclamation-triangle" };
  else if (source === "live") pill = { state: "live", text: "LIVE", icon: "bi-broadcast" };
  else pill = { state: "demo", text: "\u2026", icon: "bi-hourglass-split" };

  const agoText =
    lastUpdate != null ? `updated ${Math.round(agoMs / 1000)}s ago` : "connecting\u2026";

  const temp = status?.cpuTempC;
  const tempVal = temp != null ? `${Math.round(temp)}\u00B0` : "--";

  return (
    <>
      <div className="d-flex align-items-center justify-content-between mb-3">
        <p className="text-secondary mb-0">System overview &amp; interface status.</p>
        <div className="d-flex align-items-center gap-2">
          <span className="ov-updated">{agoText}</span>
          <span className={`mdcu-pill mdcu-pill-${pill.state}`}>
            <i className={`bi ${pill.icon} me-1`} />
            {pill.text}
          </span>
        </div>
      </div>

      <div className="ov-hero">
        <div className="ov-device">
          <div className="ov-device-head">
            <span className="ov-device-ic">
              <i className="bi bi-hdd-rack" />
            </span>
            <div>
              <div className="ov-device-title">Device</div>
              <div className="ov-device-sub">Flexible Data Collector &amp; Controller</div>
            </div>
          </div>
          <dl className="ov-kv">
            <dt>Model</dt>
            <dd>{DEVICE.model}</dd>
            <dt>Firmware</dt>
            <dd>{DEVICE.firmware}</dd>
            <dt>UI</dt>
            <dd>{DEVICE.ui}</dd>
            <dt>Serial</dt>
            <dd>{DEVICE.serial}</dd>
            <dt>Uptime</dt>
            <dd>{status ? formatUptime(status.uptimeSec) : "--"}</dd>
          </dl>
        </div>

        <div className="mdcu-card">
          <div className="mdcu-stat-label">Health</div>
          <div className="ov-gauges">
            <Gauge label="CPU Load" value={`${LOAD_PCT}%`} unit="LOAD" pct={LOAD_PCT} color="var(--mdcu-primary)" />
          </div>
        </div>

        <div className="mdcu-card">
          <div className="mdcu-stat-label">Thermal</div>
          <div className="ov-gauges">
            <Gauge
              label="CPU Temp"
              value={tempVal}
              unit="TEMP"
              pct={temp != null ? tempPct(temp) : 0}
              color={tempColor(temp ?? 0)}
            />
          </div>
        </div>

        <div className="mdcu-card">
          <div className="mdcu-stat-label">Memory</div>
          <div className="ov-gauges">
            <Gauge label="Memory" value={`${MEM_PCT}%`} unit="RAM" pct={MEM_PCT} color="#7a63e0" />
          </div>
        </div>
      </div>

      <div className="mdcu-card mt-3">
        <div className="mdcu-stat-label">
          Protocol Status <span className="ov-count">&middot; {PROTOCOLS.length} enabled</span>
        </div>
        {PROTOCOLS.length ? (
          <div className="ov-proto">
            {PROTOCOLS.map((p) => (
              <div className="ov-ptile" key={p.name}>
                {p.name}
                <div className="ov-ptile-st">
                  <Dot state={p.state} />
                  {p.detail}
                </div>
              </div>
            ))}
          </div>
        ) : (
          <div className="text-secondary small py-3">
            No protocols enabled &mdash; turn one on in Configuration.
          </div>
        )}
      </div>

      <div className="mdcu-card mt-3">
        <div className="mdcu-stat-label">Network / Uplink Interfaces</div>
        <div className="ov-ifaces">
          {UPLINKS.map((i) => (
            <Iface key={i.name} item={i} />
          ))}
        </div>
        <div className="ov-common">
          <span>
            <b>Default Route</b>
            {UPLINK_COMMON.defaultRoute}
          </span>
          <span>
            <b>DNS Servers</b>
            {UPLINK_COMMON.dns}
          </span>
        </div>
      </div>

      <div className="mdcu-card mt-3">
        <div className="mdcu-stat-label">Fieldbus / Industrial Bus Interfaces</div>
        <div className="ov-ifaces">
          {FIELDBUS.map((i) => (
            <Iface key={i.name} item={i} />
          ))}
        </div>
      </div>
    </>
  );
}
