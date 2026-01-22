#ifndef DE100_DLL_H
#define DE100_DLL_H

#include <stdio.h>
#include <stdbool.h>

#if defined(__linux__) || defined(__APPLE__) || defined(__FreeBSD__) ||        \
    defined(__unix__) || defined(__MACH__)
#include <dlfcn.h>

#elif defined(_WIN32)
#include <windows.h>
#endif

// ═══════════════════════════════════════════════════════════════════════════
// STATUS CODES
// ═══════════════════════════════════════════════════════════════════════════

enum de100_dll_status_code {
    DE100_DLL_SUCCESS = 0,
    DE100_DLL_ERROR_FILE_NOT_FOUND,
    DE100_DLL_ERROR_INVALID_FORMAT,
    DE100_DLL_ERROR_SYMBOL_NOT_FOUND,
    DE100_DLL_ERROR_ALREADY_LOADED,
    DE100_DLL_ERROR_ACCESS_DENIED,
    DE100_DLL_ERROR_OUT_OF_MEMORY,
    DE100_DLL_ERROR_INVALID_HANDLE,
    DE100_DLL_ERROR_UNKNOWN
};

// ═══════════════════════════════════════════════════════════════════════════
// DLL HANDLE STRUCTURE
// ═══════════════════════════════════════════════════════════════════════════

struct de100_dll_t {
#if defined(__linux__) || defined(__APPLE__) || defined(__FreeBSD__) ||        \
    defined(__unix__) || defined(__MACH__)
    void *dll_handle;
#elif defined(_WIN32)
    HINSTANCE dll_handle;
#endif
    enum de100_dll_status_code last_error;
    char error_message[512];
};

// ═══════════════════════════════════════════════════════════════════════════
// API FUNCTIONS
// ═══════════════════════════════════════════════════════════════════════════

/**
 * Load a dynamic library.
 * 
 * @param file Path to the library file (.so, .dylib, or .dll)
 * @param flags Platform-specific flags (RTLD_NOW, RTLD_LAZY on Unix; ignored on Windows)
 * @return DLL handle structure with error information
 * 
 * Check result.last_error == DE100_SUCCESS to verify success.
 * On error, result.dll_handle will be NULL and error_message will contain details.
 */
struct de100_dll_t de100_dlopen(const char *file, int flags);

/**
 * Load a symbol from a dynamic library.
 * 
 * @param dll DLL handle from de100_dlopen
 * @param symbol Name of the symbol to load
 * @return Pointer to the symbol, or NULL on error
 * 
 * On error, dll.last_error and dll.error_message will be updated.
 */
void *de100_dlsym(struct de100_dll_t *dll, const char *symbol);

/**
 * Close a dynamic library.
 * 
 * @param dll Pointer to DLL handle structure
 * @return Status code indicating success or failure
 * 
 * Updates dll->last_error and dll->error_message on error.
 */
enum de100_dll_status_code de100_dlclose(struct de100_dll_t *dll);

/**
 * Get a human-readable error message for a status code.
 * 
 * @param code Status code
 * @return Static string describing the error
 */
const char *de100_dlstrerror(enum de100_dll_status_code code);

/**
 * Check if a DLL handle is valid.
 * 
 * @param dll DLL handle structure
 * @return true if valid, false otherwise
 */
bool de100_dlvalid(struct de100_dll_t dll);

#endif // DE100_DLL_H
