import React from "react";
import ReactDOM from "react-dom/client";
import { BrowserRouter } from "react-router-dom";

// Bootstrap 5 (CSS + JS bundle for dropdowns/collapse) and icon font.
import "bootstrap/dist/css/bootstrap.min.css";
import "bootstrap/dist/js/bootstrap.bundle.min.js";
import "bootstrap-icons/font/bootstrap-icons.css";

// Theme tokens (the swappable layer) MUST load after Bootstrap so our
// --mdcu-* vars and overrides win.
import "./theme/tokens.css";
import "./styles/app.css";

import { ThemeProvider } from "./theme/ThemeContext.jsx";
import App from "./App.jsx";

ReactDOM.createRoot(document.getElementById("root")).render(
  <React.StrictMode>
    <ThemeProvider>
      <BrowserRouter>
        <App />
      </BrowserRouter>
    </ThemeProvider>
  </React.StrictMode>
);
