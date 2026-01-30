#ifndef DE100_DLL_H
#define DE100_DLL_H

#include "base.h"
#include <stdbool.h>

// ═══════════════════════════════════════════════════════════════════════════
// PLATFORM-SPECIFIC INCLUDES
// ═══════════════════════════════════════════════════════════════════════════

#if defined(__linux__) || defined(__APPLE__) || defined(__FreeBSD__) || \
    defined(__unix__) || defined(__MACH__)
    #include <dlfcn.h>
#elif defined(_WIN32)
    #define WIN32_LEAN_AND_MEAN
    #define NOMINMAX
    #include <windows.h>
#else
    #error "Unsupported platform for DLL operations"
#endif

// ═══════════════════════════════════════════════════════════════════════════
// ERROR CODES
// ═══════════════════════════════════════════════════════════════════════════

typedef enum {
    DLL_SUCCESS = 0,
    DLL_ERROR_FILE_NOT_FOUND,
    DLL_ERROR_INVALID_FORMAT,
    DLL_ERROR_SYMBOL_NOT_FOUND,
    DLL_ERROR_ALREADY_LOADED,
    DLL_ERROR_ACCESS_DENIED,
    DLL_ERROR_OUT_OF_MEMORY,
    DLL_ERROR_INVALID_HANDLE,
    DLL_ERROR_UNKNOWN,
    
    DLL_ERROR_COUNT  // Sentinel for validation
} DllErrorCode;

// ═══════════════════════════════════════════════════════════════════════════
// DLL HANDLE STRUCTURE (Lean - No Error Message Buffer)
// ═══════════════════════════════════════════════════════════════════════════

typedef struct {
#if defined(_WIN32)
    HMODULE handle;
#else
    void *handle;
#endif
    DllErrorCode error_code;
    bool is_valid;
} DllHandle;

// ═══════════════════════════════════════════════════════════════════════════
// API FUNCTIONS
// ═══════════════════════════════════════════════════════════════════════════

/**
 * Load a dynamic library.
 *
 * @param filepath Path to the library file (.so, .dylib, or .dll)
 * @param flags Platform-specific flags:
 *              - POSIX: RTLD_NOW, RTLD_LAZY, RTLD_LOCAL, RTLD_GLOBAL
 *              - Windows: Ignored (uses default LoadLibrary behavior)
 * @return DllHandle with is_valid=true on success
 *
 * Example:
 *   DllHandle lib = dll_open("./libgame.so", RTLD_NOW);
 *   if (!lib.is_valid) {
 *       printf("Failed: %s\n", dll_strerror(lib.error_code));
 *   }
 */
DllHandle dll_open(const char *filepath, int flags);

/**
 * Get a symbol (function or variable) from a loaded library.
 *
 * @param dll Pointer to DllHandle from dll_open()
 * @param symbol_name Name of the symbol to find
 * @return Pointer to the symbol, or NULL on error
 *
 * On error, dll->error_code is updated.
 *
 * Example:
 *   typedef void (*GameUpdateFn)(float dt);
 *   GameUpdateFn update = (GameUpdateFn)dll_sym(&lib, "game_update");
 */
void *dll_sym(DllHandle *dll, const char *symbol_name);

/**
 * Close a dynamic library.
 *
 * @param dll Pointer to DllHandle to close
 * @return Error code (DLL_SUCCESS on success)
 *
 * After closing, dll->handle is set to NULL and is_valid to false.
 * Safe to call multiple times (idempotent).
 */
DllErrorCode dll_close(DllHandle *dll);

/**
 * Check if a DLL handle is valid and loaded.
 *
 * @param dll DllHandle to check
 * @return true if valid and loaded, false otherwise
 */
static inline bool dll_is_valid(DllHandle dll) {
    return dll.is_valid && dll.handle != NULL && dll.error_code == DLL_SUCCESS;
}

// ═══════════════════════════════════════════════════════════════════════════
// ERROR HANDLING
// ═══════════════════════════════════════════════════════════════════════════

/**
 * Get a human-readable error message for an error code.
 *
 * @param code Error code from any DLL operation
 * @return Static string describing the error (never NULL)
 */
const char *dll_strerror(DllErrorCode code);

// ═══════════════════════════════════════════════════════════════════════════
// DEBUG UTILITIES (DE100_INTERNAL && DE100_SLOW only)
// ═══════════════════════════════════════════════════════════════════════════

#if DE100_INTERNAL && DE100_SLOW

/**
 * Get detailed error information from the last failed operation.
 * Thread-local storage - only valid immediately after a failed call.
 *
 * @return Detailed error string with platform-specific info, or NULL
 */
const char *dll_get_last_error_detail(void);

/**
 * Log a DLL operation result to stderr (debug builds only).
 */
void dll_debug_log_result(const char *operation, const char *path, DllHandle dll);

#endif // DE100_INTERNAL && DE100_SLOW

#endif // DE100_DLL_H
