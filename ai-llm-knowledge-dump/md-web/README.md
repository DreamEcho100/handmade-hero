# SolidStart

Everything you need to build a Solid project, powered by [`solid-start`](https://start.solidjs.com);

## Creating a project

```bash
# create a new project in the current directory
npm init solid@latest

# create a new project in my-app
npm init solid@latest my-app
```

## Developing

Once you've created a project and installed dependencies with `npm install` (or `pnpm install` or `yarn`), start a development server:

```bash
npm run dev

# or start the server and open the app in a new browser tab
npm run dev -- --open
```

## Building

Solid apps are built with _presets_, which optimise your project for deployment to different environments.

By default, `npm run build` will generate a Node app that you can run with `npm start`. To use a different preset, add it to the `devDependencies` in `package.json` and specify in your `app.config.js`.

## This project was created with the [Solid CLI](https://github.com/solidjs-community/solid-cli)

## Adding External Content via Symlinks

Routes under `src/routes/` can link to `.md` files that live outside this project (e.g. in `../generated-courses/`) using symlinks. SolidStart's file-based router and the sidebar glob will both pick them up automatically.

### How it works

- `vite.config.ts` has `resolve.preserveSymlinks: true` so Vite follows symlinks.
- `server.fs.allow` includes `"../generated-courses"` so Vite serves files from that directory.
- The sidebar in `src/components/layout.tsx` uses `import.meta.glob("../routes/**/*.md")` which traverses into symlinked directories.

### Steps to add a new content directory

1. **Create the symlink** from `src/routes/` pointing to the target folder:

   ```bash
   # Run from the workspace root (handmade-hero/)
   # The relative path goes up 3 levels from src/routes/ to reach ai-llm-knowledge-dump/
   ln -s ../../../generated-courses/<course>/<folder> \
     ai-llm-knowledge-dump/md-web/src/routes/<route-name>
   ```

2. **Verify it resolves correctly:**

   ```bash
   readlink -f ai-llm-knowledge-dump/md-web/src/routes/<route-name>
   ls ai-llm-knowledge-dump/md-web/src/routes/<route-name>/
   ```

3. **Update `server.fs.allow`** in `vite.config.ts` **only if** the symlink target is outside `../generated-courses`. Any sub-path under `../generated-courses` is already covered.

4. **Restart the dev server.**

### Example

The existing `lessons` route:

```bash
# src/routes/lessons -> ../../../generated-courses/platform-backend/course/lessons
ln -s ../../../generated-courses/platform-backend/course/lessons \
  ai-llm-knowledge-dump/md-web/src/routes/lessons
```

This makes all `.md` files under `generated-courses/platform-backend/course/lessons/` available at `/lessons/<filename>` and automatically listed in the sidebar.
