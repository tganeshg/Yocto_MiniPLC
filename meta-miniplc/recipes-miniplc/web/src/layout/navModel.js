/*
 * navModel --- single source of truth for the sidebar menu AND the route table.
 * Sidebar.jsx renders this tree (recursively); App.jsx flattens the leaves into
 * <Route>s so the nav and the router can never drift apart.
 *
 * A node is either:
 *   - a leaf  : { to, label, icon, end? }            -> NavLink + Route
 *   - a group : { label, icon, children: [node...] }  -> collapsible dropdown
 * Groups may nest (Configuration > Industrial Protocols > Modbus).
 *
 * Top-level model:
 *   Overview | Configuration | Status | Settings | About
 *   - Configuration holds the protocol groups + PLC (everything you set up).
 *   - Status will list only *configured* protocols' live status (empty today).
 *   - Settings = general system (network / Ethernet-IP, RTC clock, ...).
 *   - About = product model, firmware version, UI version, ...
 */
export const NAV_MENU = [
  { to: "/", label: "Overview", icon: "bi-speedometer2", end: true, ready: true },
  {
    label: "Configuration",
    icon: "bi-sliders2",
    children: [
      {
        label: "Industrial Protocols",
        icon: "bi-hdd-network",
        children: [
          { to: "/protocols/modbus", label: "Modbus", icon: "bi-hdd-network" },
          { to: "/protocols/profinet", label: "PROFINET", icon: "bi-ethernet" },
          { to: "/protocols/profibus", label: "PROFIBUS", icon: "bi-hdd-stack" },
          { to: "/protocols/ethercat", label: "EtherCAT", icon: "bi-diagram-2" },
          { to: "/protocols/ethernet-ip", label: "EtherNet/IP", icon: "bi-router" },
          { to: "/protocols/iec104", label: "IEC 60870-5-104", icon: "bi-broadcast-pin" },
          { to: "/protocols/iec102", label: "IEC 60870-5-102", icon: "bi-broadcast" },
          { to: "/protocols/iec61850", label: "IEC 61850", icon: "bi-lightning" },
          { to: "/protocols/dlms", label: "DLMS / COSEM", icon: "bi-lightning-charge" },
        ],
      },
      {
        label: "IoT Protocols",
        icon: "bi-cloud",
        children: [
          { to: "/iot/mqtt", label: "MQTT", icon: "bi-cloud-arrow-up" },
          { to: "/iot/opcua", label: "OPC UA", icon: "bi-cloud" },
        ],
      },
      { to: "/plc", label: "PLC", icon: "bi-diagram-3" },
    ],
  },
  { to: "/status", label: "Status", icon: "bi-activity" },
  { to: "/settings", label: "Settings", icon: "bi-gear" },
  { to: "/about", label: "About", icon: "bi-info-circle" },
];

// All routable leaves (groups recursively flattened) --- consumed by the router.
function collectLeaves(nodes) {
  return nodes.flatMap((n) => (n.children ? collectLeaves(n.children) : n));
}
export const NAV_LEAVES = collectLeaves(NAV_MENU);
