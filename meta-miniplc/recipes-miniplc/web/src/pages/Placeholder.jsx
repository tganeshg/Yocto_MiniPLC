/*
 * Placeholder --- temporary content for sections not yet built.
 * Each will be replaced by its real page as the POC progresses
 * (Protocols, DLMS, IoT, PLC, Settings, About).
 */
export default function Placeholder({ name }) {
  return (
    <div className="mdcu-card text-center py-5">
      <i className="bi bi-cone-striped" style={{ fontSize: "2rem", color: "var(--mdcu-warning)" }} />
      <h2 className="h5 mt-3 mb-1">{name}</h2>
      <p className="text-secondary mb-0">This section is part of the roadmap and not built yet.</p>
    </div>
  );
}
