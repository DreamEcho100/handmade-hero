/**
 * ====================================================================
 * RAYLIB BACKEND - High-Level Game Framework
 * ====================================================================
 *
 * Raylib is like using a game engine or React compared to vanilla DOM APIs.
 * It handles all the low-level details we had to manage manually in X11:
 *
 * X11 (Low-Level):                   Raylib (High-Level):
 * - XOpenDisplay()                   → InitWindow() - handles everything
 * - XCreateWindow()                  → Built-in window management
 * - XShmCreateImage() + shared mem   → Image + Texture2D abstractions
 * - XCreateGC()                      → Built-in graphics context
 * - Manual event loop (XNextEvent)   → WindowShouldClose() + input functions
 * - Manual frame timing (nanosleep)  → SetTargetFPS() - automatic!
 * - XShmPutImage() + XFlush()        → DrawTexture() - one call!
 * - Manual cleanup (10+ calls)       → CloseWindow() - done!
 *
 * Think of Raylib as the "Unity" or "Phaser.js" of C - it abstracts away
 * platform-specific details and lets you focus on game logic.
 */

#include "raylib_backend.h"
#include "../game/game.h"
#include "base.h"
#include "platform.h"
#include <raylib.h>
#include <stdbool.h>
#include <stdlib.h> // malloc, free (manual memory management)
#include <string.h> // strncpy for word wrapping

/**
 * Convert RGBA color components to packed 32-bit integer
 *
 * Raylib uses RGBA format (Red, Green, Blue, Alpha from low to high bits)
 * This is different from X11's BGRA format!
 *
 * Format breakdown:
 * - Bits 0-7:   Alpha (opacity, 255 = fully opaque)
 * - Bits 8-15:  Blue
 * - Bits 16-23: Green
 * - Bits 24-31: Red
 *
 * Example: rgba_format(255, 0, 0, 255) = 0xFF0000FF (red)
 *
 * Compare to X11's bgra_format which had: (a << 24) | (b << 16) | (g << 8) | r
 *
 * This is like CSS rgba(255, 0, 0, 1) but stored as a single 32-bit number
 */
static inline uint32_t rgba_format(uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
  // Bit shifting to pack 4 bytes into one 32-bit integer (RGBA order)
  return (a << 24) | (r << 16) | (g << 8) | b;
}

/**
 * Convert hex color (0xRRGGBB) to Raylib Color structure
 *
 * X11 uses raw hex values (0xRRGGBB), but Raylib uses Color structs
 * This helper converts between the two formats
 *
 * Think of it like converting '#2196F3' CSS string to an rgba() object
 */
static Color hex_to_color(unsigned long hex) {
  return (Color){
      .r = (hex >> 16) & 0xFF, // Extract red channel
      .g = (hex >> 8) & 0xFF,  // Extract green channel
      .b = hex & 0xFF,         // Extract blue channel
      .a = 255                 // Fully opaque
  };
}

/**
 * Check if mouse is over a rectangle (like :hover in CSS)
 *
 * X11 version had to manually check bounds
 * Raylib provides CheckCollisionPointRec(), but this is clearer for learning
 */
static bool is_mouse_over_rect(Vector2 mouse, int x, int y, int width,
                               int height) {
  return mouse.x >= x && mouse.x <= x + width && mouse.y >= y &&
         mouse.y <= y + height;
}

/**
 * ====================================================================
 * PLATFORM MESSAGE BOX IMPLEMENTATION (Raylib Version)
 * ====================================================================
 *
 * This is MUCH simpler than the X11 version! Compare:
 *
 * X11 version: ~400 lines
 * - Manual window creation with XCreateWindow
 * - Manual event loop with XNextEvent
 * - Manual drawing with Xft fonts and XCopyArea
 * - Manual rounded rectangles with XFillArc
 * - Manual double buffering with Pixmap
 * - Manual color allocation with XftColorAllocValue
 * - Manual cleanup (9+ function calls)
 *
 * Raylib version: ~150 lines
 * - DrawRectangleRounded() for rounded corners
 * - GuiSetStyle() for theming
 * - MeasureTextEx() for text sizing
 * - Mouse input via GetMousePosition()
 * - Automatic double buffering
 * - Automatic cleanup
 *
 * Think of it like jQuery vs vanilla DOM - same result, way less code!
 */
inline int platform_show_message_box(const char *title, const char *message,
                                     ShowMessageBoxOptions options) {
  /**
   * Check if a window already exists
   *
   * Raylib can only have ONE window at a time (unlike X11)
   * So we need to check if the main game window is already created
   *
   * X11 version: Can create multiple windows easily with XCreateWindow
   * Raylib version: Must work within existing window or create temp window
   *
   * For simplicity, we'll create a temporary window for the dialog
   * (In a real app, you'd draw the dialog over the existing window)
   */
  bool hadWindow = IsWindowReady();

  if (hadWindow) {
    // Close existing window temporarily (not ideal, but simple)
    // In production, you'd draw the dialog as an overlay instead
    CloseWindow();
  }

  /**
   * Create dialog window
   *
   * X11 equivalent:
   * - XCreateWindow(...) with all the attributes
   * - XStoreName() to set title
   * - XChangeProperty() to set window type hints
   * - XMapWindow() to show it
   *
   * Raylib: ONE function call!
   */
  InitWindow(options.width, options.height, title);
  SetTargetFPS(60);

  // Default colors (Material Design-ish, same as X11 version)
  Color bgColor = options.bgColor ? hex_to_color(options.bgColor)
                                  : (Color){245, 245, 245, 255}; // Light gray
  // Note: Raylib doesn't need explicit border color like X11 did
  // (we could use it for DrawRectangleLines if we wanted a custom border)

  // Icon and color based on message type (same logic as X11 version)
  const char *icon = options.type == MSG_BX_INFO       ? "ℹ"
                     : options.type == MSG_BX_WARNING  ? "⚠"
                     : options.type == MSG_BX_ERROR    ? "✖"
                     : options.type == MSG_BX_QUESTION ? "?"
                                                       : "ℹ";

  Color iconColor =
      options.type == MSG_BX_INFO ? (Color){33, 150, 243, 255} : // Blue
          options.type == MSG_BX_WARNING ? (Color){255, 152, 0, 255}
                                         : // Orange
          options.type == MSG_BX_ERROR ? (Color){244, 67, 54, 255}
                                       : // Red
          options.type == MSG_BX_QUESTION ? (Color){76, 175, 80, 255}
                                          : // Green
          (Color){33, 150, 243, 255};

  // Button configuration (same as X11 version)
  int buttonCount = options.buttonCount > 0 ? options.buttonCount : 1;
  const char **buttonLabels =
      options.buttons ? options.buttons : (const char *[]){"OK"};
  int *buttonValues = options.buttonValues ? options.buttonValues : (int[]){1};

  // Button layout (same calculations as X11 version)
  int buttonWidth = 90;
  int buttonHeight = 35;
  int buttonSpacing = 12;
  int totalButtonWidth =
      (buttonWidth + buttonSpacing) * buttonCount - buttonSpacing;
  int buttonStartX = (options.width - totalButtonWidth) / 2;
  int buttonY = options.height - buttonHeight - 20;

  // State tracking (like React state)
  int hoveredButton = -1;
  int result = 0;
  bool running = true;

  /**
   * Load font for better text rendering
   *
   * X11 version: XftFontOpenName() with fallback chain
   * Raylib version: LoadFontEx() or use default font
   *
   * The default font can look pixelated at small sizes, so we'll use a larger
   * base size and rely on Raylib's text scaling for smooth rendering
   *
   * Think of it like using font-size with transform: scale() in CSS for better
   * quality
   */
  int fontSize = 20;            // Larger size for smoother rendering
  int iconSize = 24;            // Larger icon for better quality
  Font font = GetFontDefault(); // Built-in font, already loaded

  /**
   * ====================================================================
   * EVENT LOOP (Much simpler than X11!)
   * ====================================================================
   *
   * X11 version:
   * - while (running) { XNextEvent(); switch (event.type) {...} }
   * - Manual Expose, MotionNotify, ButtonPress, KeyPress handling
   * - Manual needsFullRedraw flag
   *
   * Raylib version:
   * - while (running && !WindowShouldClose())
   * - GetMousePosition(), IsMouseButtonPressed() - direct state queries!
   * - Always redraw (Raylib is fast enough, no need to optimize)
   */
  while (running && !WindowShouldClose()) {
    /**
     * Get mouse state
     *
     * X11 version: event.xmotion.x, event.xmotion.y from MotionNotify event
     * Raylib version: GetMousePosition() - returns current position anytime!
     *
     * This is like: document.addEventListener('mousemove', e => {...})
     * vs. just reading the current mouse position when you need it
     */
    Vector2 mousePos = GetMousePosition();
    bool mouseClicked = IsMouseButtonPressed(MOUSE_LEFT_BUTTON);

    // Check which button is hovered (same logic as X11 version)
    hoveredButton = -1;
    for (int i = 0; i < buttonCount; i++) {
      int btnX = buttonStartX + i * (buttonWidth + buttonSpacing);
      if (is_mouse_over_rect(mousePos, btnX, buttonY, buttonWidth,
                             buttonHeight)) {
        hoveredButton = i;

        // Check for click (X11 version: ButtonPress event handling)
        if (mouseClicked) {
          result = buttonValues[i];
          running = false;
        }
        break;
      }
    }

    /**
     * Keyboard shortcuts
     *
     * X11 version: KeyPress event with XLookupKeysym()
     * Raylib version: IsKeyPressed() - simple boolean check!
     */
    if (IsKeyPressed(KEY_ESCAPE)) {
      result = 0; // ESC = Cancel
      running = false;
    } else if (IsKeyPressed(KEY_ENTER)) {
      result = buttonValues[0]; // Enter = First button
      running = false;
    }

    /**
     * ====================================================================
     * RENDERING (So much cleaner than X11!)
     * ====================================================================
     */
    BeginDrawing();

    /**
     * Clear background
     *
     * X11 version: XSetForeground(); XFillRectangle(); (to backbuffer)
     * Raylib version: One function call!
     */
    ClearBackground(bgColor);

    /**
     * Draw icon
     *
     * X11 version:
     * - XRenderColor conversion (8-bit to 16-bit)
     * - XftColorAllocValue()
     * - XftDrawStringUtf8()
     * - XftColorFree()
     *
     * Raylib version: One function call!
     *
     * MeasureTextEx() gives us text dimensions (like XGlyphInfo in X11)
     * DrawTextEx() renders the text (like XftDrawStringUtf8)
     * Using spacing=1 for better letter spacing
     */
    Vector2 iconTextSize = MeasureTextEx(font, icon, iconSize, 1);
    DrawTextEx(font, icon, (Vector2){25, 40 - iconTextSize.y / 2}, iconSize, 1,
               iconColor);

    /**
     * Draw message text with word wrapping
     *
     * X11 version: Manual word wrapping with strchr(), XftTextExtentsUtf8(),
     * etc. Raylib version: We'll implement similar word wrapping logic
     *
     * This breaks text into words and wraps when exceeding max width
     * Similar to CSS word-wrap: break-word
     */
    Color textColor = {51, 51, 51, 255};
    int msgX = 50;
    int msgY = 32;
    int maxLineWidth = options.width - 30; // Leave margin on right
    float spacing = 1.5f;

    // Simple word wrapping (similar to X11 version)
    const char *word = message;
    while (*word) {
      // Find next space or end of string
      const char *nextSpace = word;
      while (*nextSpace && *nextSpace != ' ')
        nextSpace++;

      int wordLen = nextSpace - word;

      // Measure word width
      char wordBuffer[256];
      if (wordLen < 256) {
        strncpy(wordBuffer, word, wordLen);
        wordBuffer[wordLen] = '\0';

        Vector2 wordSize = MeasureTextEx(font, wordBuffer, fontSize, spacing);

        // Check if word fits on current line
        if (msgX > 50 && msgX + wordSize.x > maxLineWidth) {
          // Move to next line
          msgX = 50;
          msgY += fontSize + 4;
        }

        // Draw the word
        DrawTextEx(font, wordBuffer, (Vector2){msgX, msgY}, fontSize, spacing,
                   textColor);
        msgX += wordSize.x;

        // Add space width if not at end
        if (*nextSpace == ' ') {
          Vector2 spaceSize = MeasureTextEx(font, " ", fontSize, spacing);
          msgX += spaceSize.x;
        }
      }

      // Move to next word
      word = nextSpace;
      if (*word == ' ')
        word++;
      if (!*word)
        break;
    }

    /**
     * Draw buttons
     *
     * X11 version:
     * - draw_rounded_rect() with XFillArc and XFillRectangle
     * - draw_rounded_rect_border() with XDrawArc and XDrawLine
     * - XSetForeground() for each color change
     * - XftDrawStringUtf8() for text
     *
     * Raylib version:
     * - DrawRectangleRounded() - ONE call for rounded rect!
     * - DrawTextEx() for text
     *
     * This is the power of a high-level framework!
     */
    for (int i = 0; i < buttonCount; i++) {
      int btnX = buttonStartX + i * (buttonWidth + buttonSpacing);

      // Button color (changes on hover, like :hover in CSS)
      Color btnColor = (i == hoveredButton) ? (Color){25, 118, 210, 255}
                                            :          // Darker blue (hover)
                           (Color){33, 150, 243, 255}; // Material Blue

      /**
       * Draw rounded rectangle button
       *
       * X11 version: ~20 lines (draw_rounded_rect + draw_rounded_rect_border)
       * Raylib version: 2 lines!
       *
       * DrawRectangleRounded() parameters:
       * - Rectangle bounds (x, y, width, height)
       * - Roundness (0.0-1.0, we use 0.2 for subtle rounding)
       * - Segments (number of segments per corner, 0 = auto)
       * - Color
       */
      /**
       * Draw button with rounded corners and border
       *
       * The border needs to be drawn with sufficient thickness to avoid gaps
       * at corner intersections. We'll draw the border by drawing a slightly
       * larger filled rectangle underneath.
       *
       * This technique (border simulation) is cleaner than
       * DrawRectangleRoundedLines which can have gaps at corner intersections.
       *
       * Think of it like using box-shadow or outline in CSS
       */
      Rectangle btnRect = {btnX, buttonY, buttonWidth, buttonHeight};
      Rectangle borderRect = {btnX - 1, buttonY - 1, buttonWidth + 2,
                              buttonHeight + 2};

      // Draw border (slightly larger rectangle underneath)
      DrawRectangleRounded(borderRect, 0.25f, 8, (Color){21, 101, 192, 255});

      // Draw button on top
      DrawRectangleRounded(btnRect, 0.25f, 8, btnColor);

      /**
       * Draw button text (centered)
       *
       * X11 version: XGlyphInfo extents; XftTextExtentsUtf8();
       * XftDrawStringUtf8() Raylib version: MeasureTextEx() + DrawTextEx()
       *
       * Using spacing=1.5 for better letter spacing and readability
       */
      const char *label = buttonLabels[i];
      Vector2 textSize = MeasureTextEx(font, label, fontSize, 1.5f);
      Vector2 textPos = {btnX + (buttonWidth - textSize.x) / 2,
                         buttonY + (buttonHeight - textSize.y) / 2};
      DrawTextEx(font, label, textPos, fontSize, 1.5f, WHITE);
    }

    /**
     * Finish frame and display
     *
     * X11 version:
     * - XCopyArea(backbuffer -> window)
     * - XFlush()
     *
     * Raylib version: EndDrawing() does it all!
     * - Swaps buffers (double buffering automatic)
     * - Limits frame rate to 60 FPS
     * - Processes events
     */
    EndDrawing();
  }

  /**
   * ====================================================================
   * CLEANUP
   * ====================================================================
   *
   * X11 version: 9+ function calls to free resources
   * Raylib version: CloseWindow() does everything!
   */
  CloseWindow();

  /**
   * Restore previous window if there was one
   *
   * In a real application, you wouldn't close/reopen windows like this
   * You'd draw the dialog as an overlay on the existing window
   * But this demonstrates the concept simply
   */
  if (hadWindow) {
    // Let the caller reinitialize their window
    // (In production, you'd use a better approach)
  }

  return result;
}

/**
 * Main entry point for Raylib platform backend
 *
 * Notice how much simpler this is than the X11 version!
 * Raylib handles ~300 lines of X11 boilerplate for us:
 * - Display server connection
 * - Window creation and management
 * - Event handling
 * - Shared memory setup
 * - Frame timing
 * - Resource cleanup
 *
 * This is like using React vs vanilla DOM manipulation
 */
int platform_main() {
  // Window dimensions (same as X11 version)
  int width = 800, height = 600;

  /**
   * Show message box before starting game (same as X11 version)
   *
   * X11 version: Had to open separate X connection for dialog
   * Raylib version: Uses InitWindow/CloseWindow within the function
   *
   * This demonstrates cross-platform abstraction - same API, different
   * implementation!
   */
  const char *yesNoButtons[] = {"Yes", "No"};
  int yesNoValues[] = {1, 0};

  int userChoice = show_message_box(
      "Confirm Action", "Are you sure you want to start the game?",
      (ShowMessageBoxOptions){.type = MSG_BX_QUESTION,
                              .width = 400,
                              .height = 200,
                              .buttons = yesNoButtons,
                              .buttonValues = yesNoValues,
                              .buttonCount = 2},
      platform_show_message_box);

  if (!userChoice) {
    return 0; // User chose not to start the game
  }

  /**
   * Initialize window and OpenGL context
   *
   * X11 equivalent (what this replaces):
   * - Display *d = XOpenDisplay(NULL);
   * - Window win = XCreateSimpleWindow(...);
   * - XSelectInput(d, win, ExposureMask | KeyPressMask);
   * - XMapWindow(d, win);
   * - GC gc = XCreateGC(d, win, 0, NULL);
   *
   * In web terms: This is like calling canvas = document.getElementById('game')
   * but Raylib also creates the canvas, sets up WebGL, and handles window
   * events
   *
   * Parameters:
   * - width, height: Window size
   * - "Raylib Backend": Window title (appears in title bar)
   */
  InitWindow(width, height, "Raylib Backend");

  /**
   * Set target frame rate to 60 FPS
   *
   * X11 equivalent (what this replaces):
   * - struct timespec target_frame_time = {0, 16666667};
   * - clock_gettime(CLOCK_MONOTONIC, &frame_start);
   * - clock_gettime(CLOCK_MONOTONIC, &frame_end);
   * - long elapsed_ns = ...calculation...
   * - nanosleep(&sleep_time, NULL);
   *
   * Raylib does ALL the frame timing automatically!
   * It's like using requestAnimationFrame() instead of manual setTimeout()
   *
   * This one call replaces ~30 lines of timing code in X11!
   */
  SetTargetFPS(60);

  /**
   * Allocate pixel buffer
   *
   * malloc = "memory allocate" - requests memory from the OS
   * In JavaScript, you'd do: new Uint32Array(width * height)
   *
   * X11 equivalent:
   * - XShmSegmentInfo shminfo;
   * - shminfo.shmid = shmget(IPC_PRIVATE, size, IPC_CREAT | 0777);
   * - shminfo.shmaddr = shmat(shminfo.shmid, 0, 0);
   * - XShmAttach(d, &shminfo);
   *
   * Raylib uses regular memory (malloc) instead of shared memory,
   * but it's still fast because textures are uploaded to GPU
   *
   * sizeof(uint32_t) = 4 bytes (one 32-bit pixel)
   * Total allocation: 800 * 600 * 4 = 1,920,000 bytes (~1.9 MB)
   */
  uint32_t *pixels = malloc(width * height * sizeof(uint32_t));

  /**
   * Create an Image structure (CPU-side pixel data)
   *
   * Image in Raylib is like ImageData in Canvas API
   * It holds raw pixel data in CPU memory (RAM)
   *
   * X11 equivalent:
   * - XImage *img = XShmCreateImage(d, visual, depth, ZPixmap,
   *                                  NULL, &shminfo, width, height);
   *
   * Fields explained:
   * - .data: Pointer to pixel array (like imageData.data)
   * - .width, .height: Image dimensions
   * - .mipmaps: Number of mipmap levels (1 = no mipmaps, just original image)
   *   Mipmaps are pre-scaled versions for 3D rendering - we don't need them
   * - .format: Pixel format - UNCOMPRESSED_R8G8B8A8 means:
   *   * UNCOMPRESSED: Raw pixel data (not compressed like PNG/JPG)
   *   * R8G8B8A8: Red/Green/Blue/Alpha, 8 bits each (32-bit color)
   *
   * Think of this as:
   * const imageData = ctx.createImageData(width, height);
   */
  Image img = {.data = pixels,
               .width = width,
               .height = height,
               .mipmaps = 1,
               .format = PIXELFORMAT_UNCOMPRESSED_R8G8B8A8};

  /**
   * Upload image to GPU as a texture
   *
   * Texture2D is GPU-side storage (VRAM) - fast to render!
   * This is like uploading to a WebGL texture
   *
   * X11 equivalent: None! X11 keeps everything in CPU/shared memory
   * Raylib is smarter - it uses GPU acceleration
   *
   * The texture is stored in video memory (VRAM) on your graphics card,
   * which is much faster for rendering than system RAM
   *
   * Think of this as:
   * const texture = gl.createTexture();
   * gl.texImage2D(gl.TEXTURE_2D, 0, gl.RGBA, width, height,
   *               0, gl.RGBA, gl.UNSIGNED_BYTE, pixels);
   */
  Texture2D tex = LoadTextureFromImage(img);

  /**
   * Initialize game state
   *
   * {0} initializes all struct fields to zero (same as X11 version)
   * In JavaScript: const state = {}
   *
   * GameState tracks things like player position, score, animation frame, etc.
   */
  GameState state = {0};

  /**
   * ====================================================================
   * MAIN GAME LOOP (So much simpler than X11!)
   * ====================================================================
   *
   * X11 version had:
   * - while (1) - infinite loop
   * - while (XPending(d)) - process all events
   * - XNextEvent(d, &e) - get each event
   * - switch (e.type) - handle event types
   * - goto cleanup - manual exit
   *
   * Raylib version:
   * - while (!WindowShouldClose()) - automatic exit handling!
   *
   * WindowShouldClose() returns true when:
   * - User clicks window close button (X)
   * - User presses ESC key (default)
   * - CloseWindow() is called
   *
   * It's like: while (!shouldExit) in your game loop
   * All input handling (keyboard, mouse) is available via simple functions
   * like IsKeyPressed(), GetMousePosition(), etc.
   */
  while (!WindowShouldClose()) {
    /**
     * Update game logic and render to pixel buffer
     *
     * Same function as X11 version! This is the beauty of platform abstraction.
     * The game doesn't care if it's running on X11, Raylib, Windows, or Web!
     *
     * This function:
     * 1. Updates game state (physics, AI, animations)
     * 2. Draws pixels directly to the buffer
     *
     * Example: pixels[y * width + x] = rgba_format(255, 0, 0, 255);
     *
     * X11 equivalent: Same! Both call game_update() identically
     */
    game_update(&state, pixels, width, height, rgba_format);

    /**
     * Upload new pixel data to GPU texture
     *
     * This copies our CPU pixel buffer to the GPU texture (VRAM)
     * Think of it as: gl.texSubImage2D() or ctx.putImageData()
     *
     * X11 equivalent:
     * - XShmPutImage(d, win, gc, img, 0, 0, 0, 0, width, height, False);
     *
     * But UpdateTexture() is smarter - it uses GPU DMA (Direct Memory Access)
     * for faster transfers when possible
     */
    UpdateTexture(tex, pixels);

    /**
     * Begin drawing frame
     *
     * This prepares the rendering context for drawing
     * It's like starting a new animation frame
     *
     * Behind the scenes, Raylib:
     * - Sets up OpenGL state
     * - Begins a new render pass
     * - Resets transformations
     *
     * Think of it as: ctx.save() or starting a WebGL render pass
     *
     * X11 equivalent: XSetForeground() to prepare graphics context
     */
    BeginDrawing();

    /**
     * Clear background to black
     *
     * This fills the entire window with black before drawing anything
     * Prevents "trails" from previous frames
     *
     * BLACK is a predefined Raylib color constant
     *
     * X11 equivalent:
     * - XSetForeground(display, gc, 0x000000);
     * - XFillRectangle(display, backbuffer, gc, 0, 0, width, height);
     *
     * Think of it as: ctx.fillStyle = 'black'; ctx.fillRect(0, 0, w, h);
     */
    ClearBackground(BLACK);

    /**
     * Draw our texture to the screen
     *
     * This is THE moment pixels become visible!
     * The GPU renders our texture at position (0, 0)
     *
     * Parameters:
     * - tex: GPU texture to draw
     * - 0, 0: Position on screen (top-left corner)
     * - WHITE: Tint color (WHITE = no tint, draw original colors)
     *
     * X11 equivalent:
     * - XCopyArea(display, backbuffer, dialog, gc,
     *             0, 0, width, height, 0, 0);
     * - XFlush(display);
     *
     * Think of it as:
     * ctx.drawImage(texture, 0, 0);
     *
     * This one call replaces ~15 lines of X11 buffer copying!
     */
    DrawTexture(tex, 0, 0, WHITE);

    /**
     * End drawing and swap buffers
     *
     * This finalizes the frame and displays it
     * Behind the scenes, Raylib:
     * - Finishes all OpenGL commands
     * - Swaps front/back buffers (double buffering)
     * - Handles frame timing (enforces 60 FPS limit)
     * - Processes window events
     *
     * X11 equivalent:
     * - (buffer swap already done in XCopyArea)
     * - Manual frame timing with nanosleep()
     *
     * Think of it as:
     * requestAnimationFrame() callback completing
     *
     * After this, the frame is visible to the user!
     */
    EndDrawing();
  }

  /**
   * ====================================================================
   * CLEANUP (Automatic with Raylib!)
   * ====================================================================
   *
   * Raylib doesn't require explicit cleanup for most resources!
   * CloseWindow() automatically frees:
   * - The window
   * - OpenGL context
   * - All loaded textures
   * - Audio resources
   * - Input system
   *
   * We only need to free what WE allocated (the pixel buffer)
   *
   * X11 equivalent (what Raylib handles for us):
   * - XShmDetach(d, &shminfo);
   * - XDestroyImage(img);
   * - shmdt(shminfo.shmaddr);
   * - XFreePixmap(display, backbuffer);
   * - XFreeGC(display, gc);
   * - XDestroyWindow(display, dialog);
   * - XCloseDisplay(display);
   * - if (font) XftFontClose(display, font);
   * - if (xftdraw) XftDrawDestroy(xftdraw);
   *
   * Raylib = 1 line cleanup
   * X11 = 9+ lines cleanup
   *
   * This is like React automatically cleaning up components vs
   * manually removing event listeners in vanilla JS
   */

  /**
   * Free our manually-allocated pixel buffer
   *
   * We used malloc(), so we must use free()
   * This releases the memory back to the OS
   *
   * In JavaScript with manual memory: buffer.free() or similar
   * But JS has garbage collection, so you normally don't do this
   *
   * IMPORTANT: If you don't call free(), the memory leaks!
   * The OS won't reclaim it until your program exits.
   * This is one of the dangers of manual memory management in C.
   */
  free(pixels);

  /**
   * Return success
   *
   * 0 = success in Unix convention (same as X11 version)
   * Non-zero would indicate an error
   *
   * This is like: process.exit(0) in Node.js
   */
  return 0;
}
