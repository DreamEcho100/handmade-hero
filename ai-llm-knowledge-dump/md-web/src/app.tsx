import { Router } from "@solidjs/router";
import { FileRoutes } from "@solidjs/start/router";
import { Suspense } from "solid-js";
import "./app.css";
import MainLayout from "~/components/layout";

export default function App() {
  return (
    <Router
      root={(props) => (
        <Suspense>
          <MainLayout {...props} />
        </Suspense>
      )}
    >
      <FileRoutes />
    </Router>
  );
}
