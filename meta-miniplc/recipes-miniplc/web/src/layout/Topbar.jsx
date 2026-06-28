import { useTheme } from "../theme/ThemeContext.jsx";

/*
 * Topbar --- page title + global actions (theme toggle).
 * The theme toggle is the visible Level-1 re-skin control.
 */
export default function Topbar({ title, onToggleNav }) {
  const { theme, toggleTheme } = useTheme();
  return (
    <header className="mdcu-topbar">
      <div className="d-flex align-items-center gap-2">
        <button
          type="button"
          className="mdcu-iconbtn mdcu-navtoggle"
          onClick={onToggleNav}
          aria-label="Toggle navigation"
        >
          <i className="bi bi-list" />
        </button>
        <h1 className="h5 mb-0 fw-semibold">{title}</h1>
      </div>
      <div className="d-flex align-items-center gap-2">
        <button
          type="button"
          className="mdcu-iconbtn"
          onClick={toggleTheme}
          title={theme === "light" ? "Switch to dark theme" : "Switch to light theme"}
          aria-label="Toggle theme"
        >
          <i className={`bi ${theme === "light" ? "bi-moon-stars" : "bi-sun"}`} />
        </button>
      </div>
    </header>
  );
}
