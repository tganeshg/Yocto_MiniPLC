import { useState } from "react";
import { NavLink, useLocation } from "react-router-dom";
import { NAV_MENU } from "./navModel.js";

/*
 * Sidebar --- primary navigation, rendered recursively from navModel.js so the
 * menu can nest (Configuration > Industrial Protocols > Modbus). Groups are
 * collapsible dropdowns; the chain of groups containing the active route starts
 * expanded. Presentation only: all colours come from --mdcu-* tokens.
 */

function leafIsActive(node, pathname) {
  if (!node.to) return false;
  return node.to === "/" ? pathname === "/" : pathname.startsWith(node.to);
}

function subtreeHasActive(node, pathname) {
  if (node.children) return node.children.some((c) => subtreeHasActive(c, pathname));
  return leafIsActive(node, pathname);
}

// Expand every group that is an ancestor of the active leaf.
function initialOpenState(nodes, pathname, acc = {}) {
  for (const node of nodes) {
    if (node.children) {
      if (node.children.some((c) => subtreeHasActive(c, pathname))) acc[node.label] = true;
      initialOpenState(node.children, pathname, acc);
    }
  }
  return acc;
}

function NavTree({ nodes, depth, openGroups, toggle, onNavigate }) {
  const nested = depth > 0 ? " mdcu-nav-child" : "";
  return nodes.map((node) =>
    node.children ? (
      <div key={node.label} className="mdcu-nav-group">
        <button
          type="button"
          className={`mdcu-nav-link mdcu-nav-parent${nested}${openGroups[node.label] ? " is-open" : ""}`}
          aria-expanded={!!openGroups[node.label]}
          onClick={() => toggle(node.label)}
        >
          <i className={`bi ${node.icon}`} />
          <span>{node.label}</span>
          <i className="bi bi-chevron-down mdcu-nav-caret" />
        </button>
        {openGroups[node.label] && (
          <div className="mdcu-nav-sub">
            <NavTree
              nodes={node.children}
              depth={depth + 1}
              openGroups={openGroups}
              toggle={toggle}
              onNavigate={onNavigate}
            />
          </div>
        )}
      </div>
    ) : (
      <NavLink
        key={node.to}
        to={node.to}
        end={node.end}
        className={`mdcu-nav-link${nested}`}
        onClick={onNavigate}
      >
        <i className={`bi ${node.icon}`} />
        <span>{node.label}</span>
      </NavLink>
    )
  );
}

export default function Sidebar({ open = false, onNavigate }) {
  const { pathname } = useLocation();
  const [openGroups, setOpenGroups] = useState(() => initialOpenState(NAV_MENU, pathname));

  const toggle = (label) =>
    setOpenGroups((prev) => ({ ...prev, [label]: !prev[label] }));

  return (
    <aside className={`mdcu-sidebar${open ? " is-open" : ""}`}>
      <div className="mdcu-brand">
        <i className="bi bi-cpu" />
        <span>FlexiDon iX</span>
      </div>

      <nav className="mdcu-nav">
        <NavTree
          nodes={NAV_MENU}
          depth={0}
          openGroups={openGroups}
          toggle={toggle}
          onNavigate={onNavigate}
        />
      </nav>

      <div className="mdcu-nav-credit">Powered by Ganesh Thiru</div>
    </aside>
  );
}
