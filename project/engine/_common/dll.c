#include "dll.h"
#include <string.h>

// ═══════════════════════════════════════════════════════════════════════════
// HELPER FUNCTIONS
// ═══════════════════════════════════════════════════════════════════════════

#if defined(_WIN32)
/**
 * Convert Windows error code to our status code.
 */
static enum de100_status_code win32_error_to_status(DWORD error_code) {
  switch (error_code) {
  case ERROR_FILE_NOT_FOUND:
  case ERROR_PATH_NOT_FOUND:
  case ERROR_MOD_NOT_FOUND:
    return DE100_DLL_ERROR_FILE_NOT_FOUND;

  case ERROR_BAD_EXE_FORMAT:
  case ERROR_INVALID_DLL:
    return DE100_DLL_ERROR_INVALID_FORMAT;

  case ERROR_ACCESS_DENIED:
  case ERROR_SHARING_VIOLATION:
    return DE100_DLL_ERROR_ACCESS_DENIED;

  case ERROR_NOT_ENOUGH_MEMORY:
  case ERROR_OUTOFMEMORY:
    return DE100_DLL_ERROR_OUT_OF_MEMORY;

  case ERROR_INVALID_HANDLE:
    return DE100_DLL_ERROR_INVALID_HANDLE;

  default:
    return DE100_DLL_ERROR_UNKNOWN;
  }
}

/**
 * Get Windows error message and store it in the DLL structure.
 */
static void win32_get_error_message(struct de100_dll_t *dll, DWORD error_code) {
  FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                 NULL, error_code, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                 dll->error_message, sizeof(dll->error_message), NULL);

  // Remove trailing newline if present
  size_t len = strlen(dll->error_message);
  if (len > 0 && dll->error_message[len - 1] == '\n') {
    dll->error_message[len - 1] = '\0';
    if (len > 1 && dll->error_message[len - 2] == '\r') {
      dll->error_message[len - 2] = '\0';
    }
  }
}
#endif

// ═══════════════════════════════════════════════════════════════════════════
// DLOPEN
// ═══════════════════════════════════════════════════════════════════════════

struct de100_dll_t de100_dlopen(const char *file, int flags) {
  struct de100_dll_t dll;
  memset(&dll, 0, sizeof(dll));

  if (!file) {
    dll.last_error = DE100_DLL_ERROR_FILE_NOT_FOUND;
    snprintf(dll.error_message, sizeof(dll.error_message),
             "NULL file path provided");
    return dll;
  }

#if defined(__linux__) || defined(__APPLE__) || defined(__FreeBSD__) ||        \
    defined(__unix__) || defined(__MACH__)
  // ─────────────────────────────────────────────────────────────────────
  // UNIX/LINUX/MACOS
  // ─────────────────────────────────────────────────────────────────────
  dlerror(); // Clear any existing error

  dll.dll_handle = dlopen(file, flags);

  if (!dll.dll_handle) {
    const char *error_str = dlerror();

    // Try to determine error type from message
    if (error_str) {
      strncpy(dll.error_message, error_str, sizeof(dll.error_message) - 1);
      dll.error_message[sizeof(dll.error_message) - 1] = '\0';

      // Parse common error patterns
      if (strstr(error_str, "No such file") ||
          strstr(error_str, "cannot open")) {
        dll.last_error = DE100_DLL_ERROR_FILE_NOT_FOUND;
      } else if (strstr(error_str, "invalid ELF header") ||
                 strstr(error_str, "wrong ELF class") ||
                 strstr(error_str, "file too short")) {
        dll.last_error = DE100_DLL_ERROR_INVALID_FORMAT;
      } else if (strstr(error_str, "Permission denied")) {
        dll.last_error = DE100_DLL_ERROR_ACCESS_DENIED;
      } else if (strstr(error_str, "Cannot allocate memory")) {
        dll.last_error = DE100_DLL_ERROR_OUT_OF_MEMORY;
      } else {
        dll.last_error = DE100_DLL_ERROR_UNKNOWN;
      }
    } else {
      dll.last_error = DE100_DLL_ERROR_UNKNOWN;
      snprintf(dll.error_message, sizeof(dll.error_message),
               "dlopen failed with no error message");
    }
  } else {
    dll.last_error = DE100_DLL_SUCCESS;
    snprintf(dll.error_message, sizeof(dll.error_message), "Success");
  }

#elif defined(_WIN32)
  // ─────────────────────────────────────────────────────────────────────
  // WINDOWS
  // ─────────────────────────────────────────────────────────────────────
  SetLastError(0); // Clear any existing error

  dll.dll_handle = LoadLibraryA(file);

  if (!dll.dll_handle) {
    DWORD error_code = GetLastError();
    dll.last_error = win32_error_to_status(error_code);
    win32_get_error_message(&dll, error_code);
  } else {
    dll.last_error = DE100_DLL_SUCCESS;
    snprintf(dll.error_message, sizeof(dll.error_message), "Success");
  }

#else
  dll.last_error = DE100_DLL_ERROR_UNKNOWN;
  snprintf(dll.error_message, sizeof(dll.error_message),
           "Unsupported platform");
#endif

  return dll;
}

// ═══════════════════════════════════════════════════════════════════════════
// DLSYM
// ═══════════════════════════════════════════════════════════════════════════

void *de100_dlsym(struct de100_dll_t *dll, const char *symbol) {
  if (!dll) {
    return NULL;
  }

  if (!dll->dll_handle) {
    dll->last_error = DE100_DLL_ERROR_INVALID_HANDLE;
    snprintf(dll->error_message, sizeof(dll->error_message),
             "Invalid DLL handle (NULL)");
    return NULL;
  }

  if (!symbol) {
    dll->last_error = DE100_DLL_ERROR_SYMBOL_NOT_FOUND;
    snprintf(dll->error_message, sizeof(dll->error_message),
             "NULL symbol name provided");
    return NULL;
  }

#if defined(__linux__) || defined(__APPLE__) || defined(__FreeBSD__) ||        \
    defined(__unix__) || defined(__MACH__)
  // ─────────────────────────────────────────────────────────────────────
  // UNIX/LINUX/MACOS
  // ─────────────────────────────────────────────────────────────────────
  dlerror(); // Clear any existing error

  void *sym = dlsym(dll->dll_handle, symbol);

  const char *error_str = dlerror();
  if (error_str) {
    dll->last_error = DE100_DLL_ERROR_SYMBOL_NOT_FOUND;
    snprintf(dll->error_message, sizeof(dll->error_message),
             "Symbol '%s' not found: %s", symbol, error_str);
    return NULL;
  }

  dll->last_error = DE100_DLL_SUCCESS;
  snprintf(dll->error_message, sizeof(dll->error_message), "Success");
  return sym;

#elif defined(_WIN32)
  // ─────────────────────────────────────────────────────────────────────
  // WINDOWS
  // ─────────────────────────────────────────────────────────────────────
  SetLastError(0);

  void *sym = (void *)GetProcAddress(dll->dll_handle, symbol);

  if (!sym) {
    DWORD error_code = GetLastError();
    dll->last_error = DE100_DLL_ERROR_SYMBOL_NOT_FOUND;

    if (error_code != 0) {
      char temp_msg[256];
      win32_get_error_message(dll, error_code);
      snprintf(temp_msg, sizeof(temp_msg), "Symbol '%s' not found: %s", symbol,
               dll->error_message);
      strncpy(dll->error_message, temp_msg, sizeof(dll->error_message) - 1);
    } else {
      snprintf(dll->error_message, sizeof(dll->error_message),
               "Symbol '%s' not found", symbol);
    }
    return NULL;
  }

  dll->last_error = DE100_DLL_SUCCESS;
  snprintf(dll->error_message, sizeof(dll->error_message), "Success");
  return sym;

#else
  dll->last_error = DE100_DLL_ERROR_UNKNOWN;
  snprintf(dll->error_message, sizeof(dll->error_message),
           "Unsupported platform");
  return NULL;
#endif
}

// ═══════════════════════════════════════════════════════════════════════════
// DLCLOSE
// ═══════════════════════════════════════════════════════════════════════════

enum de100_dll_status_code de100_dlclose(struct de100_dll_t *dll) {
  if (!dll) {
    return DE100_DLL_ERROR_INVALID_HANDLE;
  }

  if (!dll->dll_handle) {
    dll->last_error = DE100_DLL_ERROR_INVALID_HANDLE;
    snprintf(dll->error_message, sizeof(dll->error_message),
             "Invalid DLL handle (NULL)");
    return DE100_DLL_ERROR_INVALID_HANDLE;
  }

#if defined(__linux__) || defined(__APPLE__) || defined(__FreeBSD__) ||        \
    defined(__unix__) || defined(__MACH__)
  // ─────────────────────────────────────────────────────────────────────
  // UNIX/LINUX/MACOS
  // ─────────────────────────────────────────────────────────────────────
  dlerror(); // Clear any existing error

  if (dlclose(dll->dll_handle) != 0) {
    const char *error_str = dlerror();
    dll->last_error = DE100_DLL_ERROR_UNKNOWN;

    if (error_str) {
      strncpy(dll->error_message, error_str, sizeof(dll->error_message) - 1);
      dll->error_message[sizeof(dll->error_message) - 1] = '\0';
    } else {
      snprintf(dll->error_message, sizeof(dll->error_message),
               "dlclose failed with no error message");
    }

    return dll->last_error;
  }

#elif defined(_WIN32)
  // ─────────────────────────────────────────────────────────────────────
  // WINDOWS
  // ─────────────────────────────────────────────────────────────────────
  SetLastError(0);

  if (!FreeLibrary(dll->dll_handle)) {
    DWORD error_code = GetLastError();
    dll->last_error = win32_error_to_status(error_code);
    win32_get_error_message(dll, error_code);
    return dll->last_error;
  }

#else
  dll->last_error = DE100_DLL_ERROR_UNKNOWN;
  snprintf(dll->error_message, sizeof(dll->error_message),
           "Unsupported platform");
  return dll->last_error;
#endif

  dll->dll_handle = NULL;
  dll->last_error = DE100_DLL_SUCCESS;
  snprintf(dll->error_message, sizeof(dll->error_message), "Success");
  return DE100_DLL_SUCCESS;
}

// ═══════════════════════════════════════════════════════════════════════════
// ERROR HANDLING UTILITIES
// ═══════════════════════════════════════════════════════════════════════════

const char *de100_dlstrerror(enum de100_dll_status_code code) {
  switch (code) {
  case DE100_DLL_SUCCESS:
    return "Success";
  case DE100_DLL_ERROR_FILE_NOT_FOUND:
    return "File not found or path invalid";
  case DE100_DLL_ERROR_INVALID_FORMAT:
    return "Invalid library format or architecture mismatch";
  case DE100_DLL_ERROR_SYMBOL_NOT_FOUND:
    return "Symbol not found in library";
  case DE100_DLL_ERROR_ALREADY_LOADED:
    return "Library already loaded";
  case DE100_DLL_ERROR_ACCESS_DENIED:
    return "Access denied or permission error";
  case DE100_DLL_ERROR_OUT_OF_MEMORY:
    return "Out of memory";
  case DE100_DLL_ERROR_INVALID_HANDLE:
    return "Invalid library handle";
  case DE100_DLL_ERROR_UNKNOWN:
  default:
    return "Unknown error";
  }
}

bool de100_dlvalid(struct de100_dll_t dll) {
  return dll.dll_handle != NULL && dll.last_error == DE100_DLL_SUCCESS;
}
