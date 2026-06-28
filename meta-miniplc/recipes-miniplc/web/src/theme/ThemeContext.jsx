import { createContext, useContext, useEffect, useState, useCallback } from "react";

/*
 * ThemeContext --- runtime theme switching.
 *
 * Applies the chosen theme by setting two attributes on <html>:
 *   data-mdcu-theme : drives our own --mdcu-* tokens (tokens.css)
 *   data-bs-theme   : drives Bootstrap 5.3's built-in color mode
 *
 * The selection is persisted to localStorage so it survives reloads.
 * This is the Level-1 re-skin mechanism: themes are pure data.
 */

const STORAGE_KEY = "mdcu.theme";
const ThemeContext = createContext(null);

function applyTheme(name) {
  const html = document.documentElement;
  html.setAttribute("data-mdcu-theme", name);
  html.setAttribute("data-bs-theme", name); // bootstrap dark/light
}

export function ThemeProvider({ children }) {
  const [theme, setTheme] = useState(() => {
    return localStorage.getItem(STORAGE_KEY) || "light";
  });

  useEffect(() => {
    applyTheme(theme);
    localStorage.setItem(STORAGE_KEY, theme);
  }, [theme]);

  const toggleTheme = useCallback(() => {
    setTheme((t) => (t === "light" ? "dark" : "light"));
  }, []);

  return (
    <ThemeContext.Provider value={{ theme, setTheme, toggleTheme }}>
      {children}
    </ThemeContext.Provider>
  );
}

export function useTheme() {
  const ctx = useContext(ThemeContext);
  if (!ctx) throw new Error("useTheme must be used within <ThemeProvider>");
  return ctx;
}
