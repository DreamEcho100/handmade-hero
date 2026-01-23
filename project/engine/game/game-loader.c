// project/engine/game/game-loader.c

#include "game-loader.h"
#include "../_common/dll.h"
#include "../_common/file.h"
#include <stdio.h>
#include <string.h>

#if defined(__linux__) || defined(__APPLE__)
#include <dlfcn.h>
#endif

// ═══════════════════════════════════════════════════════════════════════════
// HELPER FUNCTION - Initialize stub game code
// ═══════════════════════════════════════════════════════════════════════════

static GameCode create_stub_game_code(void) {
  GameCode result = {0};
  result.update_and_render = game_update_and_render_stub;
  result.get_audio_samples = game_get_audio_samples_stub;
  result.is_valid = false;
  result.last_write_time = 0;
  result.game_code_lib.dll_handle = NULL;
  result.game_code_lib.last_error = DE100_DLL_SUCCESS;
  memset(result.game_code_lib.error_message, 0,
         sizeof(result.game_code_lib.error_message));
  return result;
}

// ═══════════════════════════════════════════════════════════════════════════
// LOAD GAME CODE
// ═══════════════════════════════════════════════════════════════════════════
GameCode load_game_code(const char *source_lib_name,
                        const char *temp_lib_name) {
  GameCode result = create_stub_game_code();

  // ─────────────────────────────────────────────────────────────────────
  // Validate input parameters
  // ─────────────────────────────────────────────────────────────────────

  if (!source_lib_name) {
    fprintf(stderr, "❌ load_game_code: NULL source_lib_name\n");
    return result;
  }

  if (!temp_lib_name) {
    fprintf(stderr, "❌ load_game_code: NULL temp_lib_name\n");
    return result;
  }

  printf("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n");
  printf("🔧 Loading game code\n");
  printf("   Source: %s\n", source_lib_name);
  printf("   Temp:   %s\n", temp_lib_name);
  printf("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n");

  // ─────────────────────────────────────────────────────────────────────
  // STEP 1: Get modification time of source file
  // ─────────────────────────────────────────────────────────────────────

  de100_file_time_result_t mod_time = de100_file_get_mod_time(source_lib_name);

  if (!mod_time.success) {
    fprintf(stderr, "❌ Failed to get modification time\n");
    fprintf(stderr, "   File: %s\n", source_lib_name);
    fprintf(stderr, "   Error: %s\n", mod_time.error_message);
    fprintf(stderr, "   Code: %s\n", de100_file_strerror(mod_time.error_code));
    fprintf(stderr, "⚠️  Using stub functions\n");
    return result;
  }

  result.last_write_time = mod_time.value;
  printf("📅 Source file last modified: %ld\n", (long)result.last_write_time);

  // ─────────────────────────────────────────────────────────────────────
  // STEP 2: Copy library file
  // ─────────────────────────────────────────────────────────────────────

  printf("📦 Copying library...\n");
  printf("   %s → %s\n", source_lib_name, temp_lib_name);

  de100_file_result_t copy_result =
      de100_file_copy(source_lib_name, temp_lib_name);

  if (!copy_result.success) {
    fprintf(stderr, "❌ Failed to copy game library\n");
    fprintf(stderr, "   Source: %s\n", source_lib_name);
    fprintf(stderr, "   Dest: %s\n", temp_lib_name);
    fprintf(stderr, "   Error: %s\n", copy_result.error_message);
    fprintf(stderr, "   Code: %s\n",
            de100_file_strerror(copy_result.error_code));
    fprintf(stderr, "⚠️  Using stub functions\n");
    return result;
  }

  printf("✅ Library copied successfully\n");

  // ─────────────────────────────────────────────────────────────────────
  // STEP 3: Load the library with de100_dlopen
  // ─────────────────────────────────────────────────────────────────────

  printf("📂 Loading library: %s\n",
         temp_lib_name); // Changed back to temp_lib_name

#if defined(__linux__) || defined(__APPLE__)
  result.game_code_lib = de100_dlopen(
      temp_lib_name, RTLD_NOW | RTLD_LOCAL); // Changed back to temp_lib_name
#else
  result.game_code_lib =
      de100_dlopen(temp_lib_name, 0); // Changed back to temp_lib_name
#endif

  if (!de100_dlvalid(result.game_code_lib)) {
    fprintf(stderr, "❌ Failed to load library\n");
    fprintf(stderr, "   Library: %s\n",
            temp_lib_name); // Changed back to temp_lib_name
    fprintf(stderr, "   Error: %s\n", result.game_code_lib.error_message);
    fprintf(stderr, "   Code: %s\n",
            de100_dlstrerror(result.game_code_lib.last_error));
    fprintf(stderr, "⚠️  Using stub functions\n");

    // Reset to stub state
    result = create_stub_game_code();
    return result;
  }

  printf("✅ Library loaded successfully\n");

  // ─────────────────────────────────────────────────────────────────────
  // STEP 4: Load function symbols
  // ─────────────────────────────────────────────────────────────────────

  printf("🔍 Loading symbols...\n");

  // Load UpdateAndRender
  result.update_and_render = (game_update_and_render_t *)de100_dlsym(
      &result.game_code_lib, "game_update_and_render");

  if (!result.update_and_render) {
    fprintf(stderr, "❌ Failed to load symbol 'game_update_and_render'\n");
    fprintf(stderr, "   Library: %s\n",
            temp_lib_name); // Changed back to temp_lib_name
    fprintf(stderr, "   Error: %s\n", result.game_code_lib.error_message);
    fprintf(stderr, "   Code: %s\n",
            de100_dlstrerror(result.game_code_lib.last_error));
    fprintf(stderr, "⚠️  Using stub functions\n");

    // Cleanup and return stub
    de100_dlclose(&result.game_code_lib);
    result = create_stub_game_code();
    return result;
  }

  printf("   ✓ game_update_and_render: %p\n", (void *)result.update_and_render);

  // Load GetSoundSamples
  result.get_audio_samples = (game_get_audio_samples_t *)de100_dlsym(
      &result.game_code_lib, "game_get_audio_samples");

  if (!result.get_audio_samples) {
    fprintf(stderr, "❌ Failed to load symbol 'game_get_audio_samples'\n");
    fprintf(stderr, "   Library: %s\n",
            temp_lib_name); // Changed back to temp_lib_name
    fprintf(stderr, "   Error: %s\n", result.game_code_lib.error_message);
    fprintf(stderr, "   Code: %s\n",
            de100_dlstrerror(result.game_code_lib.last_error));
    fprintf(stderr, "⚠️  Using stub functions\n");

    // Cleanup and return stub
    de100_dlclose(&result.game_code_lib);
    result = create_stub_game_code();
    return result;
  }

  printf("   ✓ game_get_audio_samples: %p\n", (void *)result.get_audio_samples);

  // ─────────────────────────────────────────────────────────────────────
  // Success!
  // ─────────────────────────────────────────────────────────────────────

  result.is_valid = true;

  printf("✅ Game code loaded successfully!\n");
  printf("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n");

  return result;
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
  if (!de100_dlvalid(game_code->game_code_lib)) {
    printf("ℹ️  Game code not loaded or already unloaded\n");

    // Ensure we're in a safe stub state
    game_code->is_valid = false;
    game_code->update_and_render = game_update_and_render_stub;
    game_code->get_audio_samples = game_get_audio_samples_stub;
    game_code->game_code_lib.dll_handle = NULL;
    return;
  }

  printf("🔄 Unloading game code...\n");

  // Close the library
  enum de100_dll_status_code result = de100_dlclose(&game_code->game_code_lib);

  if (result != DE100_DLL_SUCCESS) {
    fprintf(stderr, "⚠️  Failed to unload library\n");
    fprintf(stderr, "   Error: %s\n", game_code->game_code_lib.error_message);
    fprintf(stderr, "   Code: %s\n", de100_dlstrerror(result));
    // Continue anyway - we'll reset to stubs
  } else {
    printf("✅ Library unloaded successfully\n");
  }

  // Reset to safe stub state
  game_code->is_valid = false;
  game_code->update_and_render = game_update_and_render_stub;
  game_code->get_audio_samples = game_get_audio_samples_stub;
  game_code->game_code_lib.dll_handle = NULL;

  printf("✅ Game code reset to stub functions\n");
}

// ═══════════════════════════════════════════════════════════════════════════
// CHECK IF RELOAD NEEDED
// ═══════════════════════════════════════════════════════════════════════════

bool32 game_code_needs_reload(const GameCode *game_code,
                              const char *source_lib_name) {
  // Validate inputs
  if (!game_code) {
    fprintf(stderr, "⚠️  game_code_needs_reload: NULL game_code pointer\n");
    return false;
  }

  if (!source_lib_name) {
    fprintf(stderr, "⚠️  game_code_needs_reload: NULL source_lib_name\n");
    return false;
  }

  // Get current modification time
  de100_file_time_result_t current_mod_time =
      de100_file_get_mod_time(source_lib_name);

  if (!current_mod_time.success) {
    // Only log if it's not a "file not found" error
    // (file might be temporarily missing during compilation)
    if (current_mod_time.error_code != FILE_ERROR_NOT_FOUND) {
      fprintf(stderr, "⚠️  Failed to check modification time\n");
      fprintf(stderr, "   File: %s\n", source_lib_name);
      fprintf(stderr, "   Error: %s\n", current_mod_time.error_message);
    }
    return false;
  }

  printf("[RELOAD CHECK] Old: %ld, New: %ld, Changed: %s\n",
         (long)game_code->last_write_time, (long)current_mod_time.value,
         (current_mod_time.value != game_code->last_write_time) ? "YES" : "NO");

  // Compare modification times
  if (current_mod_time.value != game_code->last_write_time) {
    printf("🔄 File modification detected\n");
    printf("   Old time: %ld\n", (long)game_code->last_write_time);
    printf("   New time: %ld\n", (long)current_mod_time.value);
    return true;
  }

  return false;
}

// ═══════════════════════════════════════════════════════════════════════════
// STUB FUNCTIONS (Used when game code fails to load)
// ═══════════════════════════════════════════════════════════════════════════

GAME_UPDATE_AND_RENDER(game_update_and_render_stub) {
  (void)memory;
  (void)input;
  (void)buffer;
  // Stub implementation - does nothing
  // This is called when game code fails to load
}

GAME_GET_SOUND_SAMPLES(game_get_audio_samples_stub) {
  (void)memory;
  (void)sound_buffer;
  // Stub implementation - does nothing
  // This is called when game code fails to load
}
