import { Routes, Route } from "react-router-dom";
import AppShell from "./layout/AppShell.jsx";
import Overview from "./pages/Overview.jsx";
import Placeholder from "./pages/Placeholder.jsx";
import { NAV_LEAVES } from "./layout/navModel.js";

/*
 * App --- route table generated from navModel.js so every sidebar entry has a
 * matching route. Overview is the only implemented page; the rest render a
 * roadmap placeholder and are filled in section by section.
 */
const page = (title, element) => <AppShell title={title}>{element}</AppShell>;

export default function App() {
  return (
    <Routes>
      {NAV_LEAVES.map((item) => (
        <Route
          key={item.to}
          path={item.to}
          element={page(
            item.label,
            item.to === "/" ? <Overview /> : <Placeholder name={item.label} />
          )}
        />
      ))}
      <Route path="*" element={page("Not found", <Placeholder name="Page not found" />)} />
    </Routes>
  );
}
