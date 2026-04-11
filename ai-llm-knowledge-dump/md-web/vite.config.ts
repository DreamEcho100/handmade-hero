import { defineConfig } from "vite";
import mdx from "@mdx-js/rollup";
import type { PluggableList } from "unified";
import rehypeExpressiveCode from "rehype-expressive-code";
import remarkGfm from "remark-gfm";
import { nitroV2Plugin as nitro } from "@solidjs/vite-plugin-nitro-2";
import { solidStart } from "@solidjs/start/config";
import { createJavaScriptRegexEngine } from "shiki";
import tailwindcss from "@tailwindcss/vite";
import rehypeRaw from "rehype-raw";
import rehypeSlug from "rehype-slug";
import rehypeAutolinkHeadings from "rehype-autolink-headings";

const rehypePlugins: PluggableList = [
  [
    // biome-ignore lint/suspicious/noTsIgnore: Unified/VFile versions disagree in editor types, but the plugin is valid at runtime.
    // @ts-ignore
    rehypeRaw,
    {
      passThrough: [
        "mdxFlowExpression",
        "mdxJsxFlowElement",
        "mdxJsxTextElement",
        "mdxTextExpression",
        "mdxjsEsm",
      ],
    },
  ],
  rehypeSlug,
  [
    rehypeAutolinkHeadings,
    {
      behavior: "append",
      properties: {
        className: ["heading-anchor"],
        ariaLabel: "Link to section",
      },
      content: {
        type: "text",
        value: " #",
      },
    },
  ],
  [
    rehypeExpressiveCode,
    {
      themes: ["material-theme-ocean"],
      langs: [
        "javascript",
        "c",
        "typescript",
        "markdown",
        "mdx",
        "bash",
        "css",
      ],
      engine: createJavaScriptRegexEngine,
    },
  ],
];

export default defineConfig({
  plugins: [
    tailwindcss(),
    {
      ...mdx({
        jsx: true,
        jsxImportSource: "solid-js",
        providerImportSource: "solid-mdx",
        remarkPlugins: [remarkGfm],
        // biome-ignore lint/suspicious/noTsIgnore: Unified/VFile versions disagree in editor types, but the plugin list is valid at runtime.
        // @ts-ignore
        rehypePlugins,
      }),
      enforce: "pre",
    },
    solidStart({
      extensions: ["mdx", "md"],
    }),
    nitro(),
  ],
  resolve: {
    preserveSymlinks: true, // keep symlinked paths as-is (don't resolve to real path)
  },
  build: {
    target: "esnext",
  },
  optimizeDeps: {
    include: ["shiki", "rehype-expressive-code"],
  },
  server: {
    watch: {
      followSymlinks: true, // watch symlinked dirs for HMR
    },
    fs: {
      allow: [".", "../generated-courses"],
    },
  },
});
