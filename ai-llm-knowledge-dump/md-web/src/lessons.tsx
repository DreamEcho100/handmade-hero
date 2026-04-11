import { A, type RouteSectionProps } from "@solidjs/router";
import { For } from "solid-js";
import { clientOnly } from "@solidjs/start";

function Sidebar() {
  /* Glob all markdown files inside the lessons directory at build time */
  const modules = import.meta.glob("./lessons/**/*.md");

  interface NavLink {
    href?: string;
    label?: string;
    children?: NavLink[];
  }

  type NavLinkToMetaEntry = {
    index: number;
    children: NavLinkToMeta;
  };

  type NavLinkToMeta = Record<string, NavLinkToMetaEntry>;

  const navLinks: NavLink[] = [];
  const navLinkToMeta: NavLinkToMeta = {};

  function createFormattedLabel(routePath?: string) {
    if (!routePath) {
      return;
    }

    const label = routePath.split("/").pop();

    if (!label) {
      return;
    }

    /* Capitalize first letter and replace hyphens with spaces for readability */
    return label.charAt(0).toUpperCase() + label.slice(1).replace(/-/g, " ");
  }

  Object.keys(modules)
    .toSorted(
      /* Sort by depth first, then alphabetically */
      (a, b) => {
        const aDepth = a.split("/").length;
        const bDepth = b.split("/").length;
        if (aDepth === bDepth) {
          return a.localeCompare(b);
        }
        return aDepth - bDepth;
      },
    )
    .forEach((path) => {
      const routePath = path.replace("./", "/").replace(".md", "");
      const routePathSegments = routePath.split("/").slice(1);

      let currentChildren = navLinks;
      let currentMeta = navLinkToMeta;

      for (let i = 0; i < routePathSegments.length; i++) {
        const segment = routePathSegments[i];
        const isLast = i === routePathSegments.length - 1;

        if (isLast) {
          // Leaf — actual link
          currentChildren.push({
            href: routePath,
            label: createFormattedLabel(segment),
          });
        } else {
          // Intermediate — folder node
          if (!currentMeta[segment]) {
            const folder: NavLink = {
              label: createFormattedLabel(segment),
              children: [],
              href: undefined,
            };
            currentChildren.push(folder);
            currentMeta[segment] = {
              index: currentChildren.length - 1,
              children: {},
            };
          }
          const entry = currentMeta[segment];
          currentChildren = currentChildren[entry.index].children ??= [];
          currentMeta = entry.children;
        }
      }
    });

  return (
    <nav
      /* Sidebar container: fixed width, full height, respects theme and text direction */
      class="
			sticky top-0
				flex flex-col gap-1 p-5 w-72
				border-e border-gray-200 dark:border-gray-700
				h-screen shadow-sm
				bg-white dark:bg-gray-900
				text-gray-800 dark:text-gray-100
				transition-colors duration-200
			"
    >
      {/* ── Sidebar Header ─────────────────────────────── */}
      <div class="mb-6">
        <h2
          class="
						text-xs font-semibold uppercase tracking-widest
						text-gray-400 dark:text-gray-500
						mb-1
					"
        >
          Navigation
        </h2>
        <p class="text-lg font-bold text-gray-800 dark:text-gray-100">
          Lessons
        </p>
        {/* Decorative divider below the header */}
        <div class="mt-3 h-px bg-linear-to-r from-blue-500/40 to-transparent dark:from-blue-400/30" />
      </div>

      {/* ── Navigation Links List ───────────────────────── */}
      <ul class="flex flex-col gap-0.5 overflow-y-auto">
        <For each={navLinks}>
          {(link) => (
            <li>
              {link.href ? (
                <A
                  href={link.href}
                  activeClass="
												bg-blue-50 dark:bg-blue-900/30
												text-blue-600 dark:text-blue-400
												font-semibold
												border-s-2 border-blue-600 dark:border-blue-400
											"
                  class="
												group
												flex items-center gap-2.5
												ps-3 pe-3 py-2
												rounded-md text-sm
												text-gray-600 dark:text-gray-400
												hover:bg-gray-100 dark:hover:bg-gray-800
												hover:text-gray-900 dark:hover:text-gray-100
												transition-all duration-150
												focus-visible:outline-none
												focus-visible:ring-2 focus-visible:ring-blue-500
											"
                >
                  <span
                    class="
													w-1.5 h-1.5 rounded-full bg-current
													opacity-40 group-hover:opacity-70
													transition-opacity duration-150
													shrink-0
												"
                  />
                  <span class="truncate">{link.label}</span>
                </A>
              ) : (
                <details class="group/details">
                  <summary
                    class="
													flex items-center gap-2.5
													ps-3 pe-3 py-2
													rounded-md text-sm cursor-pointer
													text-gray-700 dark:text-gray-300
													hover:bg-gray-100 dark:hover:bg-gray-800
													hover:text-gray-900 dark:hover:text-gray-100
													transition-all duration-150
													focus-visible:outline-none
													focus-visible:ring-2 focus-visible:ring-blue-500
													list-none
												"
                  >
                    {/* Animated arrow indicator */}
                    <span
                      class="
														w-3.5 h-3.5 shrink-0
														transition-transform duration-200
														group-open/details:rotate-90
													"
                    >
                      <svg
                        xmlns="http://www.w3.org/2000/svg"
                        viewBox="0 0 20 20"
                        fill="currentColor"
                      >
                        <title>Chevron right</title>
                        <path
                          fill-rule="evenodd"
                          d="M7.21 14.77a.75.75 0 01.02-1.06L11.168 10 7.23 6.29a.75.75 0 111.04-1.08l4.5 4.25a.75.75 0 010 1.08l-4.5 4.25a.75.75 0 01-1.06-.02z"
                          clip-rule="evenodd"
                        />
                      </svg>
                    </span>
                    <span class="truncate font-medium">{link.label}</span>
                  </summary>

                  {/* Nested children links */}
                  <ul class="flex flex-col gap-0.5 ms-3 mt-0.5 border-s border-gray-200 dark:border-gray-700 ps-2">
                    <For each={link.children}>
                      {(child) => (
                        <li>
                          {child.href ? (
                            <A
                              href={child.href}
                              activeClass="
																			bg-blue-50 dark:bg-blue-900/30
																			text-blue-600 dark:text-blue-400
																			font-semibold
																			border-s-2 border-blue-600 dark:border-blue-400
																		"
                              class="
																			group
																			flex items-center gap-2.5
																			ps-3 pe-3 py-1.5
																			rounded-md text-sm
																			text-gray-600 dark:text-gray-400
																			hover:bg-gray-100 dark:hover:bg-gray-800
																			hover:text-gray-900 dark:hover:text-gray-100
																			transition-all duration-150
																			focus-visible:outline-none
																			focus-visible:ring-2 focus-visible:ring-blue-500
																		"
                            >
                              <span
                                class="
																				w-1.5 h-1.5 rounded-full bg-current
																				opacity-40 group-hover:opacity-70
																				transition-opacity duration-150
																				shrink-0
																			"
                              />
                              <span class="truncate">{child.label}</span>
                            </A>
                          ) : (
                            <details class="group/details">
                              <summary
                                class="
																				flex items-center gap-2.5
																				ps-3 pe-3 py-1.5
																				rounded-md text-sm cursor-pointer
																				text-gray-700 dark:text-gray-300
																				hover:bg-gray-100 dark:hover:bg-gray-800
																				hover:text-gray-900 dark:hover:text-gray-100
																				transition-all duration-150
																				focus-visible:outline-none
																				focus-visible:ring-2 focus-visible:ring-blue-500
																				list-none
																			"
                              >
                                <span
                                  class="
																					w-3.5 h-3.5 shrink-0
																					transition-transform duration-200
																					group-open/details:rotate-90
																				"
                                >
                                  <svg
                                    xmlns="http://www.w3.org/2000/svg"
                                    viewBox="0 0 20 20"
                                    fill="currentColor"
                                  >
                                    <title>Chevron right</title>
                                    <path
                                      fill-rule="evenodd"
                                      d="M7.21 14.77a.75.75 0 01.02-1.06L11.168 10 7.23 6.29a.75.75 0 111.04-1.08l4.5 4.25a.75.75 0 010 1.08l-4.5 4.25a.75.75 0 01-1.06-.02z"
                                      clip-rule="evenodd"
                                    />
                                  </svg>
                                </span>
                                <span class="truncate font-medium">
                                  {child.label}
                                </span>
                              </summary>

                              <ul class="flex flex-col gap-0.5 ms-3 mt-0.5 border-s border-gray-200 dark:border-gray-700 ps-2">
                                <For each={child.children}>
                                  {(grandChild) => (
                                    <li>
                                      <A
                                        href={grandChild.href ?? "/"}
                                        activeClass="
																								bg-blue-50 dark:bg-blue-900/30
																								text-blue-600 dark:text-blue-400
																								font-semibold
																								border-s-2 border-blue-600 dark:border-blue-400
																							"
                                        class="
																								group
																								flex items-center gap-2.5
																								ps-3 pe-3 py-1.5
																								rounded-md text-sm
																								text-gray-600 dark:text-gray-400
																								hover:bg-gray-100 dark:hover:bg-gray-800
																								hover:text-gray-900 dark:hover:text-gray-100
																								transition-all duration-150
																								focus-visible:outline-none
																								focus-visible:ring-2 focus-visible:ring-blue-500
																							"
                                      >
                                        <span
                                          class="
																									w-1.5 h-1.5 rounded-full bg-current
																									opacity-40 group-hover:opacity-70
																									transition-opacity duration-150
																									shrink-0
																								"
                                        />
                                        <span class="truncate">
                                          {grandChild.label}
                                        </span>
                                      </A>
                                    </li>
                                  )}
                                </For>
                              </ul>
                            </details>
                          )}
                        </li>
                      )}
                    </For>
                  </ul>
                </details>
              )}
            </li>
          )}
        </For>
      </ul>

      {/* ── Sidebar Footer ─────────────────────────────── */}
      <div class="mt-auto pt-4 border-t border-gray-100 dark:border-gray-800">
        <p class="text-xs text-gray-400 dark:text-gray-600 text-center">
          Handmade Hero Lessons
        </p>
      </div>
    </nav>
  );
}

export default function LessonsLayout(props: RouteSectionProps) {
  const ClientOnlyComp = clientOnly(
    () => import("./components/client-only-hash-scroll"),
  );

  return (
    /* Full-height layout: sidebar + scrollable content, respects text direction */
    <section
      class="
				flex min-h-screen
				bg-gray-50 dark:bg-gray-950
				text-gray-900 dark:text-gray-100
				transition-colors duration-200
			"
    >
      <Sidebar />
      <ClientOnlyComp />

      {/* ── Main Content Area ───────────────────────────── */}
      <main data-content-scroll class="flex-1 p-6 sm:p-10 overflow-y-auto">
        {/* Constrained reading width centered in the available space */}
        <div
          class="
						max-w-3xl mx-auto
						prose dark:prose-invert
						prose-headings:font-semibold
						prose-a:text-blue-600 dark:prose-a:text-blue-400
					"
        >
          {props.children}
        </div>
      </main>
    </section>
  );
}
