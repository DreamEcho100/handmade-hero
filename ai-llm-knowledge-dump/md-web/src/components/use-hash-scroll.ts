import { useLocation } from "@solidjs/router";
import { createEffect, onCleanup, onMount } from "solid-js";

function findHashTarget(hash: string) {
  const id = decodeURIComponent(hash.replace(/^#/, ""));

  if (!id) {
    return;
  }

  return document.getElementById(id);
}

export function useHashScroll() {
  const location = useLocation();

  const scrollToHash = (behavior: ScrollBehavior = "smooth") => {
    const target = findHashTarget(window.location.hash);

    if (!target) {
      return;
    }

    target.scrollIntoView({
      behavior,
      block: "start",
      inline: "nearest",
    });
    target.setAttribute("tabindex", "-1");
    target.focus({ preventScroll: true });
  };

  const queueScroll = (behavior: ScrollBehavior = "smooth") => {
    requestAnimationFrame(() => {
      requestAnimationFrame(() => {
        scrollToHash(behavior);
      });
    });
  };

  const handleHashChange = () => {
    queueScroll("smooth");
  };

  onMount(() => {
    window.addEventListener("hashchange", handleHashChange);
    queueScroll("auto");
  });

  createEffect(() => {
    location.pathname;
    location.hash;
    queueScroll(location.hash ? "smooth" : "auto");
  });

  onCleanup(() => {
    window.removeEventListener("hashchange", handleHashChange);
  });
}
