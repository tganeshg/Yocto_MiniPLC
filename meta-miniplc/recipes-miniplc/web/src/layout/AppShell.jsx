import { useState } from "react";
import Sidebar from "./Sidebar.jsx";
import Topbar from "./Topbar.jsx";

/*
 * AppShell --- the persistent layout frame (sidebar + topbar + main).
 * Page content is passed as children; the active page supplies its title.
 *
 * Responsive: on desktop the sidebar is a fixed grid column; below the `lg`
 * breakpoint it collapses to an off-canvas drawer toggled from the topbar.
 */
export default function AppShell({ title, children }) {
  const [navOpen, setNavOpen] = useState(false);

  return (
    <div className={`mdcu-shell${navOpen ? " nav-open" : ""}`}>
      <Sidebar open={navOpen} onNavigate={() => setNavOpen(false)} />
      <Topbar title={title} onToggleNav={() => setNavOpen((v) => !v)} />
      <main className="mdcu-main">{children}</main>
      <div
        className="mdcu-backdrop"
        onClick={() => setNavOpen(false)}
        aria-hidden="true"
      />
    </div>
  );
}
