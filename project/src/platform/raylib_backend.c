#include <raylib.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>

#define file_scoped_fn static
#define local_persist_var static
#define global_var static

global_var void *g_PixelData = NULL;
global_var int g_BufferWidth = 0;
global_var int g_BufferHeight = 0;
global_var int g_BytesPerPixel = 4;
global_var Texture2D g_BackBufferTexture = {0};
global_var bool g_HasTexture = false;

inline file_scoped_fn void RenderWeirdGradient(int blue_offset,
                                               int green_offset) {
  int pitch = g_BufferWidth * g_BytesPerPixel;
  uint8_t *row = (uint8_t *)g_PixelData;

  for (int y = 0; y < g_BufferHeight; ++y) {
    uint32_t *pixels = (uint32_t *)row;
    for (int x = 0; x < g_BufferWidth; ++x) {
      uint8_t blue = (x + blue_offset);
      uint8_t green = (y + green_offset);

      *pixels++ = (0xFF000000 | (green << 8) | (blue << 16));
    }
    row += pitch;
  }
}

/********************************************************************
 RESIZE BACKBUFFER
 - Free old CPU memory
 - Free old GPU texture
 - Allocate new CPU pixel memory
 - Create new Raylib texture (GPU)
*********************************************************************/
file_scoped_fn void ResizeBackBuffer(int width, int height) {
  printf("Resizing back buffer → %dx%d\n", width, height);

  if (width <= 0 || height <= 0) {
    printf("Rejected resize: invalid size\n");
    return;
  }

  int old_width = g_BufferWidth;
  int old_height = g_BufferHeight;

  // Update first!
  g_BufferWidth = width;
  g_BufferHeight = height;

  // ---- 1. FREE OLD PIXEL MEMORY
  // -------------------------------------------------
  if (g_PixelData && old_width > 0 && old_height > 0) {
    munmap(g_PixelData, old_width * old_height * g_BytesPerPixel);
  }
  g_PixelData = NULL;

  // ---- 2. FREE OLD TEXTURE
  // ------------------------------------------------------
  if (g_HasTexture) {
    UnloadTexture(g_BackBufferTexture);
    g_HasTexture = false;
  }
  // ---- 3. ALLOCATE NEW BACKBUFFER
  // ----------------------------------------------
  int buffer_size = width * height * g_BytesPerPixel;
  g_PixelData = mmap(NULL, buffer_size, PROT_READ | PROT_WRITE,
                     MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);

  if (g_PixelData == MAP_FAILED) {
    g_PixelData = NULL;
    fprintf(stderr, "mmap failed: could not allocate %d bytes\n", buffer_size);
    return;
  }

  // memset(g_PixelData, 0, buffer_size); // Raylib does not auto-clear like
  // mmap
  memset(g_PixelData, 0, buffer_size);

  g_BufferWidth = width;
  g_BufferHeight = height;

  // ---- 4. CREATE RAYLIB TEXTURE
  // -------------------------------------------------
  Image img = {.data = g_PixelData,
               .width = g_BufferWidth,
               .height = g_BufferHeight,
               .format = PIXELFORMAT_UNCOMPRESSED_R8G8B8A8,
               .mipmaps = 1};
  g_BackBufferTexture = LoadTextureFromImage(img);
  g_HasTexture = true;
  printf("Raylib texture created successfully\n");
}

/********************************************************************
 UPDATE WINDOW (BLIT)
 Equivalent to XPutImage or StretchDIBits
*********************************************************************/
file_scoped_fn void UpdateWindowFromBackBuffer(void) {
  if (!g_HasTexture || !g_PixelData)
    return;

  // Upload CPU → GPU
  UpdateTexture(g_BackBufferTexture, g_PixelData);

  // Draw GPU texture → screen
  DrawTexture(g_BackBufferTexture, 0, 0, WHITE);
}

/**
 * MAIN FUNCTION
 *
 * Same purpose as X11 version, but MUCH simpler!
 */
int platform_main() {

  /**
   * InitWindow() does ALL of this:
   * - XOpenDisplay() - Connect to display server
   * - XCreateSimpleWindow() - Create the window
   * - XStoreName() - Set window title
   * - XSetWMProtocols() - Set up close handler
   * - XSelectInput() - Register for events
   * - XMapWindow() - Show the window
   *
   * Raylib handles all platform differences internally.
   * Works on Windows, Linux, macOS, web, mobile!
   */
  InitWindow(800, 600, "Handmade Hero");
  printf("Window created and shown\n");

  /**
   * ENABLE WINDOW RESIZING
   *
   * By default, Raylib creates a FIXED-SIZE window.
   * This is different from X11/Windows which allow resizing by default.
   *
   * SetWindowState(FLAG_WINDOW_RESIZABLE) tells Raylib:
   * "Allow the user to resize this window"
   *
   * This is like setting these properties in X11:
   * - Setting size hints with XSetWMNormalHints()
   * - Allowing min/max size changes
   *
   * WHY FIXED BY DEFAULT?
   * Games often need a specific resolution and don't want the window resized.
   * But for Casey's lesson, we want to demonstrate resize events!
   *
   * WEB ANALOGY:
   * It's like the difference between:
   * - <div style="resize: none">  (default Raylib)
   * - <div style="resize: both">  (after SetWindowState)
   */
  SetWindowState(FLAG_WINDOW_RESIZABLE);
  printf("Window is now resizable\n");

  /**
   * OPTIONAL: Set target FPS
   *
   * Raylib has built-in frame rate limiting.
   * We don't need this for Day 002, but it's useful later.
   */
  SetTargetFPS(60);

  g_BufferWidth = 800;
  g_BufferHeight = 600;
  int test_x = 0;
  int test_y = 0;
  int blue_offset = 0;
  int green_offset = 0;

  ResizeBackBuffer(800, 600);

  /**
   * EVENT LOOP
   *
   * WindowShouldClose() checks if:
   * - User clicked X button (WM_CLOSE / ClientMessage)
   * - User pressed ESC key
   *
   * It's like: while (!g_Running) in X11
   *
   * Raylib handles the event queue internally - we don't see XNextEvent()
   */
  while (!WindowShouldClose()) {
    /**
     * CHECK FOR WINDOW RESIZE EVENT
     *
     * Casey's WM_SIZE equivalent.
     * In X11, this is ConfigureNotify event.
     *
     * IsWindowResized() returns true when window size changes.
     * This is like checking event.type == ConfigureNotify in X11.
     *
     * When window is resized, we:
     * 1. Toggle the color (like Casey does)
     * 2. Log the new size (like printf in X11 version)
     *
     * WEB ANALOGY:
     * window.addEventListener('resize', () => {
     *   console.log('Window resized!');
     *   toggleColor();
     * });
     */
    if (IsWindowResized()) {
      printf("Window resized to: %dx%d\n", GetScreenWidth(), GetScreenHeight());
      ResizeBackBuffer(GetScreenWidth(), GetScreenHeight());
    }

    // Render gradient into CPU pixel buffer
    RenderWeirdGradient(blue_offset, green_offset);

    /**
     * BEGIN DRAWING (PAINT EVENT)
     *
     * This is Casey's WM_PAINT event!
     * In X11, this is the Expose event.
     *
     * BeginDrawing() is like:
     * - BeginPaint() in Windows
     * - Handling the Expose event in X11
     *
     * IMPORTANT DIFFERENCE FROM X11:
     * - X11: Expose event only fires when window needs repaint (resize,
     * uncover)
     * - Raylib: BeginDrawing() is called EVERY FRAME (game loop style)
     *
     * This is because Raylib is designed for games which redraw constantly,
     * while traditional windowing systems only redraw when necessary.
     *
     * To match Casey's behavior, we toggle color on RESIZE, not every frame.
     */
    BeginDrawing();

    /**
     * CLEAR SCREEN (Toggle between white/black)
     *
     * This is like:
     * - PatBlt() with WHITENESS/BLACKNESS in Casey's Windows code
     * - XFillRectangle() in X11
     *
     * ClearBackground() fills the entire window with a color
     */
    ClearBackground(BLACK);

    UpdateWindowFromBackBuffer(); // CPU → GPU → Screen

    int total_pixels = g_BufferWidth * g_BufferHeight;
    if (test_x + 1 < g_BufferWidth - 1) {
      test_x += 1;
    } else {
      test_x = 0;
      if (test_y + 75 < g_BufferHeight - 1) {
        test_y += 75;
      } else {
        test_y = 0;
      }
    }
    int test_offset = test_y * g_BufferWidth + test_x;

    if (test_offset < total_pixels) {

      // Draw the green pixel
      DrawPixel(test_x, test_y, RED);
    }

    /**
     * END DRAWING
     *
     * This is like:
     * - EndPaint() in Windows
     * - XCopyArea() to flip backbuffer to screen in X11
     *
     * Raylib automatically:
     * - Swaps the backbuffer to the front buffer (double buffering)
     * - Handles vsync
     * - Processes window events (resize, focus, etc.)
     * - Logs these events internally
     */
    EndDrawing();

    blue_offset++;
    // green_offset++;
  }

  /**
   * CLEANUP
   *
   * CloseWindow() does ALL of this:
   * - XDestroyWindow()
   * - XCloseDisplay()
   * - Frees all internal Raylib resources
   *
   * One line vs multiple in X11!
   */
  printf("Cleaning up...\n");
  // Cleanup
  if (g_HasTexture)
    UnloadTexture(g_BackBufferTexture);
  if (g_PixelData)
    // free(g_PixelData);
    munmap(g_PixelData, g_BufferWidth * g_BufferHeight * 4);
  CloseWindow();
  printf("Goodbye!\n");

  return 0;
}
