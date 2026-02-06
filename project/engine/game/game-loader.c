// project/engine/game/game-loader.c

#include <stdio.h>
#include <string.h>

#include "../_common/dll.h"
#include "../_common/file.h"
#include "game-loader.h"

// ═══════════════════════════════════════════════════════════════════════════
// HELPER FUNCTION - Initialize stub game code
// ═══════════════════════════════════════════════════════════════════════════

de100_file_scoped_fn inline GameCode create_stub_game_main_code(void) {
  GameCode result = {0};
  result.update_and_render = game_update_and_render_stub;
  result.get_audio_samples = game_get_audio_samples_stub;
  result.startup = game_startup_stub;
  result.init = game_init_stub;
  result.is_valid = false;
  result.last_write_time = (PlatformTimeSpec){0};
  result.game_code_lib.handle = NULL;
  result.game_code_lib.error_code = DLL_SUCCESS;

  return result;
}

de100_file_scoped_fn inline GameCode create_stub_pre_game_main_code(void) {
  GameCode result = {0};
  result.startup = game_startup_stub;
  result.init = game_init_stub;
  result.is_valid = false;
  result.last_write_time = (PlatformTimeSpec){0};
  result.game_code_lib.handle = NULL;
  result.game_code_lib.error_code = DLL_SUCCESS;

  return result;
}

de100_file_scoped_fn inline int load_game_assets(GameCode *game_code,
                                                 GameCode *stub_game_code,
                                                 char *source_lib_name,
                                                 char *temp_lib_name) {

  // ─────────────────────────────────────────────────────────────────────
  // Validate inputs parameters
  // ─────────────────────────────────────────────────────────────────────

  if (!source_lib_name) {
    fprintf(stderr, "❌ load_game_code: NULL source_lib_name\n");
    *game_code = *stub_game_code;
    return 1;
  }

  if (!temp_lib_name) {
    fprintf(stderr, "❌ load_game_code: NULL temp_lib_name\n");
    *game_code = *stub_game_code;
    return 1;
  }

  printf("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n");
  printf("🔧 Loading game code\n");
  printf("   Source: %s\n", source_lib_name);
  printf("   Temp:   %s\n", temp_lib_name);
  printf("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n");

  // ─────────────────────────────────────────────────────────────────────
  // Get modification time of source file
  // ─────────────────────────────────────────────────────────────────────

  De100FileTimeResult mod_time = de100_file_get_mod_time(source_lib_name);

  if (!mod_time.success) {
    fprintf(stderr, "❌ Failed to get modification time\n");
    fprintf(stderr, "   File: %s\n", source_lib_name);
    fprintf(stderr, "   Code: %s\n", de100_file_strerror(mod_time.error_code));
    fprintf(stderr, "⚠️  Using stub functions\n");
    *game_code = *stub_game_code;
    return 1;
  }

  stub_game_code->last_write_time = mod_time.value;
  printf("📅 Source file last modified: %0.2f\n",
         platform_timespec_to_seconds(&stub_game_code->last_write_time));

  // ─────────────────────────────────────────────────────────────────────
  // Copy library file
  // ─────────────────────────────────────────────────────────────────────

  printf("📦 Copying library...\n");
  printf("   %s → %s\n", source_lib_name, temp_lib_name);

  De100FileResult copy_result = de100_file_copy(source_lib_name, temp_lib_name);

  if (!copy_result.success) {
    fprintf(stderr, "❌ Failed to copy game library\n");
    fprintf(stderr, "   Source: %s\n", source_lib_name);
    fprintf(stderr, "   Dest: %s\n", temp_lib_name);
    fprintf(stderr, "   Code: %s\n",
            de100_file_strerror(copy_result.error_code));
    fprintf(stderr, "⚠️  Using stub functions\n");
    *game_code = *stub_game_code;
    return 1;
  }

  printf("✅ Library copied successfully\n");

  // ─────────────────────────────────────────────────────────────────────
  // Load the library with de100_dll_open
  // ─────────────────────────────────────────────────────────────────────

  printf("📂 Loading library: %s\n",
         temp_lib_name); // Changed back to temp_lib_name

#if defined(__linux__) || defined(__APPLE__)
  stub_game_code->game_code_lib = de100_dll_open(
      temp_lib_name, RTLD_NOW | RTLD_LOCAL); // Changed back to temp_lib_name
#else
  result->game_code_lib =
      de100_dll_open(temp_lib_name, 0); // Changed back to temp_lib_name
#endif

  if (!de100_dll_is_valid(stub_game_code->game_code_lib)) {
    fprintf(stderr, "❌ Failed to load library\n");
    fprintf(stderr, "   Library: %s\n",
            temp_lib_name); // Changed back to temp_lib_name
    fprintf(stderr, "   Code: %s\n",
            de100_dll_strerror(stub_game_code->game_code_lib.error_code));
    fprintf(stderr, "⚠️  Using stub functions\n");

    // Reset to stub state
    *stub_game_code = create_stub_game_main_code();
    *game_code = *stub_game_code;
    return 1;
  }

  printf("✅ Library loaded successfully\n");

  return 0;
}

// ═══════════════════════════════════════════════════════════════════════════
// LOAD GAME CODE
// ═══════════════════════════════════════════════════════════════════════════
int load_game_code(GameCode *game_code, GameCodePaths *game_code_paths,
                   GAME_CODE_CATEGORIES category) {
  GameCode stub_game_code = create_stub_game_main_code();

  if (category & GAME_CODE_CATEGORY_STARTUP) {
    char *source_lib_name = game_code_paths->game_startup_lib_path;
    char *temp_lib_name = game_code_paths->game_startup_lib_tmp_path;

    if (load_game_assets(game_code, &stub_game_code, source_lib_name,
                         temp_lib_name) != 0) {
      return 1;
    };

    // Load Startup
    stub_game_code.startup = (game_startup_t *)de100_dll_sym(
        &stub_game_code.game_code_lib, "game_startup");

    if (!stub_game_code.startup) {
      fprintf(stderr, "❌ Failed to load symbol 'game_startup'\n");
      fprintf(stderr, "   Library: %s\n",
              temp_lib_name); // Changed back to temp_lib_name
      fprintf(stderr, "   Code: %s\n",
              de100_dll_strerror(stub_game_code.game_code_lib.error_code));
      fprintf(stderr, "⚠️  Using stub functions\n");

      // Cleanup and return stub
      de100_dll_close(&stub_game_code.game_code_lib);
      stub_game_code = create_stub_pre_game_main_code();
      *game_code = stub_game_code;
      return 1;
    }

    printf("   ✓ game_startup: %p\n", (void *)stub_game_code.update_and_render);
  }

  if (category & GAME_CODE_CATEGORY_INIT) {
    char *source_lib_name = game_code_paths->game_init_lib_path;
    char *temp_lib_name = game_code_paths->game_init_lib_tmp_path;

    if (load_game_assets(game_code, &stub_game_code, source_lib_name,
                         temp_lib_name) != 0) {
      return 1;
    };

    // Load Init
    stub_game_code.init = (game_init_t *)de100_dll_sym(
        &stub_game_code.game_code_lib, "game_init");

    if (!stub_game_code.init) {
      fprintf(stderr, "❌ Failed to load symbol 'game_init'\n");
      fprintf(stderr, "   Library: %s\n",
              temp_lib_name); // Changed back to temp_lib_name
      fprintf(stderr, "   Code: %s\n",
              de100_dll_strerror(stub_game_code.game_code_lib.error_code));
      fprintf(stderr, "⚠️  Using stub functions\n");

      // Cleanup and return stub
      de100_dll_close(&stub_game_code.game_code_lib);
      stub_game_code = create_stub_game_main_code();
      *game_code = stub_game_code;
      return 1;
    }
    printf("   ✓ game_init: %p\n", (void *)stub_game_code.init);
  }

  if (category & GAME_CODE_CATEGORY_MAIN) {
    char *source_lib_name = game_code_paths->game_main_lib_path;
    char *temp_lib_name = game_code_paths->game_main_lib_tmp_path;

    if (load_game_assets(game_code, &stub_game_code, source_lib_name,
                         temp_lib_name) != 0) {
      return 1;
    };

    // ─────────────────────────────────────────────────────────────────────
    // Load function symbols
    // ─────────────────────────────────────────────────────────────────────

    printf("🔍 Loading symbols...\n");

    // Load UpdateAndRender
    stub_game_code.update_and_render =
        (game_update_and_render_t *)de100_dll_sym(&stub_game_code.game_code_lib,
                                                  "game_update_and_render");

    if (!stub_game_code.update_and_render) {
      fprintf(stderr, "❌ Failed to load symbol 'game_update_and_render'\n");
      fprintf(stderr, "   Library: %s\n",
              temp_lib_name); // Changed back to temp_lib_name
      fprintf(stderr, "   Code: %s\n",
              de100_dll_strerror(stub_game_code.game_code_lib.error_code));
      fprintf(stderr, "⚠️  Using stub functions\n");

      // Cleanup and return stub
      de100_dll_close(&stub_game_code.game_code_lib);
      stub_game_code = create_stub_game_main_code();
      *game_code = stub_game_code;
      return 1;
    }

    printf("   ✓ game_update_and_render: %p\n",
           (void *)stub_game_code.update_and_render);

    // Load GetSoundSamples
    stub_game_code.get_audio_samples =
        (game_get_audio_samples_t *)de100_dll_sym(&stub_game_code.game_code_lib,
                                                  "game_get_audio_samples");

    if (!stub_game_code.get_audio_samples) {
      fprintf(stderr, "❌ Failed to load symbol 'game_get_audio_samples'\n");
      fprintf(stderr, "   Library: %s\n",
              temp_lib_name); // Changed back to temp_lib_name
      fprintf(stderr, "   Code: %s\n",
              de100_dll_strerror(stub_game_code.game_code_lib.error_code));
      fprintf(stderr, "⚠️  Using stub functions\n");

      // Cleanup and return stub
      de100_dll_close(&stub_game_code.game_code_lib);
      stub_game_code = create_stub_game_main_code();
      *game_code = stub_game_code;
      return 1;
    }

    printf("   ✓ game_get_audio_samples: %p\n",
           (void *)stub_game_code.get_audio_samples);
  }

  // ─────────────────────────────────────────────────────────────────────
  // Success!
  // ─────────────────────────────────────────────────────────────────────

  stub_game_code.is_valid = true;

  printf("✅ Game code loaded successfully!\n");
  printf("   startup:           %p %s\n", (void *)stub_game_code.startup,
         stub_game_code.startup == game_startup_stub ? "(STUB!)" : "✓");
  printf("   init:              %p %s\n", (void *)stub_game_code.init,
         stub_game_code.init == game_init_stub ? "(STUB!)" : "✓");
  printf("   update_and_render: %p %s\n",
         (void *)stub_game_code.update_and_render,
         stub_game_code.update_and_render == game_update_and_render_stub
             ? "(STUB!)"
             : "✓");
  printf("   get_audio_samples: %p %s\n",
         (void *)stub_game_code.get_audio_samples,
         stub_game_code.get_audio_samples == game_get_audio_samples_stub
             ? "(STUB!)"
             : "✓");

  printf("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n");

  *game_code = stub_game_code;
  return 0;
}

// ═══════════════════════════════════════════════════════════════════════════
// UNLOAD GAME CODE
// ═══════════════════════════════════════════════════════════════════════════

void unload_game_code(GameCode *game_code) {
  if (!game_code) {
    fprintf(stderr, "⚠️  unload_game_code: NULL game_code pointer\n");
    return;
  }

  // Check if there's anything to unload
  if (!de100_dll_is_valid(game_code->game_code_lib)) {
    printf("ℹ️  Game code not loaded or already unloaded\n");

    // Ensure we're in a safe stub state
    game_code->is_valid = false;
    game_code->update_and_render = game_update_and_render_stub;
    game_code->get_audio_samples = game_get_audio_samples_stub;
    game_code->game_code_lib.handle = NULL;
    return;
  }

  printf("🔄 Unloading game code...\n");

  // Close the library
  De100DllErrorCode result = de100_dll_close(&game_code->game_code_lib);

  if (result != DLL_SUCCESS) {
    fprintf(stderr, "⚠️  Failed to unload library\n");
    fprintf(stderr, "   Code: %s\n", de100_dll_strerror(result));
    // Continue anyway - we'll reset to stubs
  } else {
    printf("✅ Library unloaded successfully\n");
  }

  // Reset to safe stub state
  game_code->is_valid = false;
  game_code->update_and_render = game_update_and_render_stub;
  game_code->get_audio_samples = game_get_audio_samples_stub;
  game_code->game_code_lib.handle = NULL;

  printf("✅ Game code reset to stub functions\n");
}

// ═══════════════════════════════════════════════════════════════════════════
// CHECK IF RELOAD NEEDED
// ═══════════════════════════════════════════════════════════════════════════

bool32 game_main_code_needs_reload(GameCode *game_code, char *source_lib_name) {
  // Validate inputs
  if (!game_code) {
    fprintf(stderr, "⚠️  game_main_code_needs_reload: NULL game_code pointer\n");
    return false;
  }

  if (!source_lib_name) {
    fprintf(stderr, "⚠️  game_main_code_needs_reload: NULL \"%s\"\n",
            source_lib_name);
    return false;
  }

  // Get current modification time
  De100FileTimeResult current_mod_time =
      de100_file_get_mod_time(source_lib_name);

  if (!current_mod_time.success) {
    // Only log if it's not a "file not found" error
    // (file might be temporarily missing during compilation)
    if (current_mod_time.error_code != FILE_ERROR_NOT_FOUND) {
      fprintf(stderr, "⚠️  Failed to check modification time\n");
      fprintf(stderr, "   File: %s\n", source_lib_name);
    }
    return false;
  }

#if DE100_INTERNAL
  if (FRAME_LOG_EVERY_FIVE_SECONDS_CHECK) {
    printf("[RELOAD CHECK] Old: %0.2f, New: %0.2f, Changed: %s\n",
           platform_timespec_to_seconds(&game_code->last_write_time),
           platform_timespec_to_seconds(&current_mod_time.value),
           platform_timespec_diff_seconds(&game_code->last_write_time,
                                          &current_mod_time.value) > 0.0
               ? "YES"
               : "NO");
  }
#endif

  // Compare modification times
  if (platform_timespec_diff_seconds(&game_code->last_write_time,
                                     &current_mod_time.value) > 0.0) {
    printf("🔄 File modification detected\n");
    printf("   Old time: %0.2f\n",
           platform_timespec_to_seconds(&game_code->last_write_time));
    printf("   New time: %0.2f\n",
           platform_timespec_to_seconds(&current_mod_time.value));
    return true;
  }

  return false;
}

void handle_game_reload_check(GameCode *game_code,
                              GameCodePaths *game_code_paths) {
  // ═══════════════════════════════════════════════════════════
  // 🔄 HOT RELOAD CHECK (Casey's Day 21/22 pattern)
  // ═══════════════════════════════════════════════════════════
  // Check periodically if game code has been recompiled
  // This allows changing game logic without restarting!
  // ═══════════════════════════════════════════════════════════
  if (g_reload_requested ||
      game_main_code_needs_reload(game_code,
                                  game_code_paths->game_main_lib_path)) {
    if (g_reload_requested) {
      g_reload_requested = false;
      printf("🔄 Hot reload requested by user!\n");
    }

    printf("🔄 Hot reload triggered! at g_frame_counter: %d\n",
           g_frame_counter);
    printf("[HOT RELOAD] Before: update_and_render=%p "
           "get_audio_samples=%p\n",
           (void *)game_code->update_and_render,
           (void *)game_code->get_audio_samples);

    unload_game_code(game_code);

    printf("[HOT RELOAD] After:  update_and_render=%p "
           "get_audio_samples=%p\n",
           (void *)game_code->update_and_render,
           (void *)game_code->get_audio_samples);

    load_game_code(game_code, game_code_paths, GAME_CODE_CATEGORY_MAIN);

    if (game_code->is_valid) {
      printf("✅ Hot reload successful!\n");

      // NOTE: do on a separate thread
      de100_file_delete(
          game_code_paths->game_main_lib_tmp_path); // Clean up temp file
    } else {
      printf("⚠️  Hot reload failed, using stubs\n");
    }
  }
}

// ═══════════════════════════════════════════════════════════════════════════
// STUB FUNCTIONS (Used when game code fails to load)
// ═══════════════════════════════════════════════════════════════════════════

GAME_UPDATE_AND_RENDER(game_update_and_render_stub) {
  (void)thread_context;
  (void)memory;
  (void)inputs;
  (void)buffer;
  // Stub implementation - does nothing
  // This is called when game code fails to load
}

GAME_GET_AUDIO_SAMPLES(game_get_audio_samples_stub) {
  (void)memory;
  (void)audio_buffer;
  // Stub implementation - does nothing
  // This is called when game code fails to load
}

GAME_STARTUP(game_startup_stub) {
  (void)game_config;
  // Stub implementation - does nothing
  // This is called when game code fails to load
  return 0;
}

GAME_INIT(game_init_stub) {
  (void)thread_context;
  (void)memory;
  (void)inputs;
  (void)buffer;
  // Stub implementation - does nothing
  // This is called when game code fails to load
}