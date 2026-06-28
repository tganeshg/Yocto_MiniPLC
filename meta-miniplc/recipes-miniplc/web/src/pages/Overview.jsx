import { useEffect, useRef, useState } from "react";
import ReactApexChart from "react-apexcharts";
import { useSystemStatus } from "../hooks/useSystemStatus.js";
import { formatUptime, formatEpoch } from "../domain/sysmap.js";
import { useTheme } from "../theme/ThemeContext.jsx";

function StatCard({ icon, label, value, sub }) {
  return (
    <div className="mdcu-card h-100">
      <div className="d-flex align-items-start justify-content-between">
        <div>
          <div className="mdcu-stat-label">{label}</div>
          <div className="mdcu-stat-value">{value}</div>
          {sub && <div className="text-secondary small mt-1">{sub}</div>}
        </div>
        <div className="mdcu-stat-icon">
          <i className={`bi ${icon}`} />
        </div>
      </div>
    </div>
  );
}

const MAX_POINTS = 30;

export default function Overview() {
  const { status, source, loading } = useSystemStatus(2000);
  const { theme } = useTheme();
  const [temps, setTemps] = useState([]);
  const lastHb = useRef(null);

  // Accumulate a small rolling CPU-temp history for the trend sparkline.
  useEffect(() => {
    if (!status) return;
    // Only push on a fresh sample (heartbeat advanced) to avoid duplicates.
    if (status.heartbeat !== lastHb.current) {
      lastHb.current = status.heartbeat;
      setTemps((prev) => {
        const next = [...prev, Number(status.cpuTempC.toFixed(2))];
        return next.slice(-MAX_POINTS);
      });
    }
  }, [status]);

  // Pull the chart colour from the theme token so the trend honours the
  // swappable-theme layer (recomputed whenever the active theme changes).
  const primary =
    (typeof window !== "undefined" &&
      getComputedStyle(document.documentElement)
        .getPropertyValue("--mdcu-primary")
        .trim()) ||
    "#2e75b6";

  const chartOptions = {
    chart: {
      type: "area",
      sparkline: { enabled: true },
      toolbar: { show: false },
      zoom: { enabled: false },
      animations: { enabled: false },
      parentHeightOffset: 0,
    },
    stroke: { curve: "smooth", width: 2 },
    fill: { type: "gradient", gradient: { opacityFrom: 0.35, opacityTo: 0.04 } },
    colors: [primary],
    dataLabels: { enabled: false },
    // Explicitly suppress axes/grid (sparkline defaults are unreliable across
    // ApexCharts builds, so we hide them directly).
    yaxis: { show: false },
    xaxis: {
      labels: { show: false },
      axisBorder: { show: false },
      axisTicks: { show: false },
      tooltip: { enabled: false },
    },
    grid: { show: false, padding: { left: 8, right: 8, top: 6, bottom: 0 } },
    tooltip: { theme, y: { formatter: (v) => `${v} \u00B0C` } },
  };

  return (
    <>
      <div className="d-flex align-items-center justify-content-between mb-3">
        <p className="text-secondary mb-0">
          Live system status from the MDCU register pool (SYS block).
        </p>
        {source && (
          <span className={`mdcu-pill ${source === "live" ? "mdcu-pill-live" : "mdcu-pill-demo"}`}>
            <i className={`bi ${source === "live" ? "bi-broadcast" : "bi-cone-striped"} me-1`} />
            {source === "live" ? "LIVE" : "DEMO DATA"}
          </span>
        )}
      </div>

      <div className="row g-3">
        <div className="col-12 col-sm-6 col-xl-3">
          <StatCard
            icon="bi-thermometer-half"
            label="CPU Temperature"
            value={status ? `${status.cpuTempC.toFixed(1)} \u00B0C` : "--"}
            sub="i32 millicelsius @ SYS+0"
          />
        </div>
        <div className="col-12 col-sm-6 col-xl-3">
          <StatCard
            icon="bi-clock-history"
            label="Uptime"
            value={status ? formatUptime(status.uptimeSec) : "--"}
            sub="u32 seconds @ SYS+2"
          />
        </div>
        <div className="col-12 col-sm-6 col-xl-3">
          <StatCard
            icon="bi-calendar-event"
            label="RTC / Wall clock"
            value={status ? formatEpoch(status.epoch) : "--"}
            sub="u32 epoch @ SYS+4"
          />
        </div>
        <div className="col-12 col-sm-6 col-xl-3">
          <StatCard
            icon="bi-activity"
            label="Heartbeat"
            value={status ? status.heartbeat : "--"}
            sub="u16 @ SYS+6"
          />
        </div>
      </div>

      <div className="row g-3 mt-1">
        <div className="col-12 col-xl-8">
          <div className="mdcu-card">
            <div className="mdcu-stat-label mb-2">CPU Temperature trend</div>
            {temps.length > 1 ? (
              <ReactApexChart
                type="area"
                height={180}
                series={[{ name: "CPU \u00B0C", data: temps }]}
                options={chartOptions}
              />
            ) : (
              <div className="text-secondary small py-5 text-center">
                {loading ? "Collecting samples\u2026" : "Waiting for samples\u2026"}
              </div>
            )}
          </div>
        </div>
        <div className="col-12 col-xl-4">
          <div className="mdcu-card h-100">
            <div className="mdcu-stat-label mb-2">Device</div>
            <dl className="row mb-0 small">
              <dt className="col-5 text-secondary fw-normal">Product</dt>
              <dd className="col-7">FlexiDon iX</dd>
              <dt className="col-5 text-secondary fw-normal">Pool</dt>
              <dd className="col-7">50,000 × 16-bit</dd>
              <dt className="col-5 text-secondary fw-normal">Data source</dt>
              <dd className="col-7">{source === "live" ? "/api/regs" : "stub (pending)"}</dd>
              <dt className="col-5 text-secondary fw-normal">Profile</dt>
              <dd className="col-7 text-secondary">unset</dd>
            </dl>
          </div>
        </div>
      </div>
    </>
  );
}
