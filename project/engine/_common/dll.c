#include "dll.h"

#include <stdio.h>
#include <string.h>

// ═══════════════════════════════════════════════════════════════════════════
// DEBUG ERROR DETAIL STORAGE (Thread-Local)
// ═══════════════════════════════════════════════════════════════════════════

#if DE100_INTERNAL && DE100_SLOW

#if defined(_WIN32)
de100_file_scoped_global_var __declspec(thread) char g_last_error_detail[1024];
#else
de100_file_scoped_global_var __thread char g_last_error_detail[1024];
#endif

#define SET_ERROR_DETAIL(...)                                                  \
  snprintf(g_last_error_detail, sizeof(g_last_error_detail), __VA_ARGS__)

#define CLEAR_ERROR_DETAIL() (g_last_error_detail[0] = '\0')

#else

#define SET_ERROR_DETAIL(...) ((void)0)
#define CLEAR_ERROR_DETAIL() ((void)0)

#endif // DE100_INTERNAL && DE100_SLOW

// ═══════════════════════════════════════════════════════════════════════════
// PLATFORM ERROR TRANSLATION
// ═══════════════════════════════════════════════════════════════════════════

#if defined(_WIN32)

de100_file_scoped_fn De100DllErrorCode
win32_error_to_dll_error(DWORD error_code) {
  switch (error_code) {
  case ERROR_SUCCESS:
    return DLL_SUCCESS;

  case ERROR_FILE_NOT_FOUND:
  case ERROR_PATH_NOT_FOUND:
  case ERROR_MOD_NOT_FOUND:
    return DLL_ERROR_FILE_NOT_FOUND;

  case ERROR_BAD_EXE_FORMAT:
  case ERROR_INVALID_DLL:
    return DLL_ERROR_INVALID_FORMAT;

  case ERROR_ACCESS_DENIED:
  case ERROR_SHARING_VIOLATION:
    return DLL_ERROR_ACCESS_DENIED;

  case ERROR_NOT_ENOUGH_MEMORY:
  case ERROR_OUTOFMEMORY:
    return DLL_ERROR_OUT_OF_MEMORY;

  case ERROR_INVALID_HANDLE:
    return DLL_ERROR_INVALID_HANDLE;

  default:
    return DLL_ERROR_UNKNOWN;
  }
}

#if DE100_INTERNAL && DE100_SLOW
de100_file_scoped_fn void win32_set_error_detail(const char *operation,
                                                 const char *path,
                                                 DWORD error_code) {
  char sys_msg[512];
  FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                 NULL, error_code, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                 sys_msg, sizeof(sys_msg), NULL);

  // Remove trailing newline
  size_t len = strlen(sys_msg);
  while (len > 0 && (sys_msg[len - 1] == '\n' || sys_msg[len - 1] == '\r')) {
    sys_msg[--len] = '\0';
  }

  SET_ERROR_DETAIL("[%s] '%s': %s (Win32 error %lu)", operation,
                   path ? path : "(null)", sys_msg, error_code);
}
#endif

#else // POSIX

de100_file_scoped_fn De100DllErrorCode
posix_dlerror_to_dll_error(const char *error_str) {
  if (!error_str) {
    return DLL_ERROR_UNKNOWN;
  }

  // Parse common error patterns from dlerror()
  if (strstr(error_str, "No such file") || strstr(error_str, "cannot open") ||
      strstr(error_str, "not found")) {
    return DLL_ERROR_FILE_NOT_FOUND;
  }

  if (strstr(error_str, "invalid ELF header") ||
      strstr(error_str, "wrong ELF class") ||
      strstr(error_str, "file too short") ||
      strstr(error_str, "not a dynamic")) {
    return DLL_ERROR_INVALID_FORMAT;
  }

  if (strstr(error_str, "Permission denied")) {
    return DLL_ERROR_ACCESS_DENIED;
  }

  if (strstr(error_str, "Cannot allocate memory") ||
      strstr(error_str, "out of memory")) {
    return DLL_ERROR_OUT_OF_MEMORY;
  }

  if (strstr(error_str, "undefined symbol") ||
      strstr(error_str, "symbol not found")) {
    return DLL_ERROR_SYMBOL_NOT_FOUND;
  }

  return DLL_ERROR_UNKNOWN;
}

#if DE100_INTERNAL && DE100_SLOW
de100_file_scoped_fn void posix_set_error_detail(const char *operation,
                                                 const char *path,
                                                 const char *dlerror_msg) {
  SET_ERROR_DETAIL("[%s] '%s': %s", operation, path ? path : "(null)",
                   dlerror_msg ? dlerror_msg : "Unknown error");
}
#endif

#endif // Platform selection

// ═══════════════════════════════════════════════════════════════════════════
// RESULT HELPERS
// ═══════════════════════════════════════════════════════════════════════════

de100_file_scoped_fn De100DllHandle make_dll_error(De100DllErrorCode code) {
  De100DllHandle result = {0};
  result.handle = NULL;
  result.error_code = code;
  result.is_valid = false;
  return result;
}

de100_file_scoped_fn De100DllHandle make_dll_success(void *handle) {
  De100DllHandle result = {0};
#if defined(_WIN32)
  result.handle = (HMODULE)handle;
#else
  result.handle = handle;
#endif
  result.error_code = DLL_SUCCESS;
  result.is_valid = true;
  CLEAR_ERROR_DETAIL();
  return result;
}

// ═══════════════════════════════════════════════════════════════════════════
// DLL OPEN
// ═══════════════════════════════════════════════════════════════════════════

De100DllHandle de100_dll_open(const char *filepath, int flags) {
  // ─────────────────────────────────────────────────────────────────────
  // Validate inputs
  // ─────────────────────────────────────────────────────────────────────
  if (!filepath) {
    SET_ERROR_DETAIL("[de100_dll_open] NULL filepath provided");
    return make_dll_error(DLL_ERROR_FILE_NOT_FOUND);
  }

  if (filepath[0] == '\0') {
    SET_ERROR_DETAIL("[de100_dll_open] Empty filepath provided");
    return make_dll_error(DLL_ERROR_FILE_NOT_FOUND);
  }

#if defined(_WIN32)
  // ─────────────────────────────────────────────────────────────────────
  // WINDOWS: LoadLibraryA
  // ─────────────────────────────────────────────────────────────────────
  (void)flags; // Windows doesn't use RTLD_* flags

  SetLastError(0);
  HMODULE handle = LoadLibraryA(filepath);

  if (!handle) {
    DWORD error_code = GetLastError();
#if DE100_INTERNAL && DE100_SLOW
    win32_set_error_detail("de100_dll_open:LoadLibraryA", filepath, error_code);
#endif
    return make_dll_error(win32_error_to_dll_error(error_code));
  }

  return make_dll_success((void *)handle);

#else
  // ─────────────────────────────────────────────────────────────────────
  // POSIX: dlopen
  // ─────────────────────────────────────────────────────────────────────
  dlerror(); // Clear any existing error

  void *handle = dlopen(filepath, flags);

  if (!handle) {
    const char *error_str = dlerror();
    De100DllErrorCode code = posix_dlerror_to_dll_error(error_str);

#if DE100_INTERNAL && DE100_SLOW
    posix_set_error_detail("de100_dll_open:dlopen", filepath, error_str);
#endif
    return make_dll_error(code);
  }

  return make_dll_success(handle);

#endif
}

// ═══════════════════════════════════════════════════════════════════════════
// DLL SYM
// ═══════════════════════════════════════════════════════════════════════════

void *de100_dll_sym(De100DllHandle *dll, const char *symbol_name) {
  // ─────────────────────────────────────────────────────────────────────
  // Validate inputs
  // ─────────────────────────────────────────────────────────────────────
  if (!dll) {
    return NULL;
  }

  if (!dll->handle || !dll->is_valid) {
    dll->error_code = DLL_ERROR_INVALID_HANDLE;
    SET_ERROR_DETAIL("[de100_dll_sym] Invalid DLL handle (NULL or not loaded)");
    return NULL;
  }

  if (!symbol_name || symbol_name[0] == '\0') {
    dll->error_code = DLL_ERROR_SYMBOL_NOT_FOUND;
    SET_ERROR_DETAIL("[de100_dll_sym] NULL or empty symbol name");
    return NULL;
  }

#if defined(_WIN32)
  // ─────────────────────────────────────────────────────────────────────
  // WINDOWS: GetProcAddress
  // ─────────────────────────────────────────────────────────────────────
  SetLastError(0);

  FARPROC sym = GetProcAddress(dll->handle, symbol_name);

  if (!sym) {
    DWORD error_code = GetLastError();
    dll->error_code = DLL_ERROR_SYMBOL_NOT_FOUND;

#if DE100_INTERNAL && DE100_SLOW
    char detail[256];
    snprintf(detail, sizeof(detail), "symbol '%s'", symbol_name);
    win32_set_error_detail("de100_dll_sym:GetProcAddress", detail, error_code);
#endif
    return NULL;
  }

  dll->error_code = DLL_SUCCESS;
  CLEAR_ERROR_DETAIL();
  return (void *)sym;

#else
  // ─────────────────────────────────────────────────────────────────────
  // POSIX: dlsym
  // ─────────────────────────────────────────────────────────────────────
  dlerror(); // Clear any existing error

  void *sym = dlsym(dll->handle, symbol_name);

  // Note: dlsym can return NULL for valid symbols (e.g., a NULL pointer)
  // We must check dlerror() to distinguish error from valid NULL
  const char *error_str = dlerror();

  if (error_str) {
    dll->error_code = DLL_ERROR_SYMBOL_NOT_FOUND;

#if DE100_INTERNAL && DE100_SLOW
    char detail[256];
    snprintf(detail, sizeof(detail), "symbol '%s'", symbol_name);
    posix_set_error_detail("de100_dll_sym:dlsym", detail, error_str);
#endif
    return NULL;
  }

  dll->error_code = DLL_SUCCESS;
  CLEAR_ERROR_DETAIL();
  return sym;

#endif
}

// ═══════════════════════════════════════════════════════════════════════════
// DLL CLOSE
// ═══════════════════════════════════════════════════════════════════════════

De100DllErrorCode de100_dll_close(De100DllHandle *dll) {
  // ─────────────────────────────────────────────────────────────────────
  // Validate inputs
  // ─────────────────────────────────────────────────────────────────────
  if (!dll) {
    return DLL_ERROR_INVALID_HANDLE;
  }

  // Already closed - idempotent operation
  if (!dll->handle) {
    dll->is_valid = false;
    dll->error_code = DLL_SUCCESS;
    CLEAR_ERROR_DETAIL();
    return DLL_SUCCESS;
  }

#if defined(_WIN32)
  // ─────────────────────────────────────────────────────────────────────
  // WINDOWS: FreeLibrary
  // ─────────────────────────────────────────────────────────────────────
  SetLastError(0);

  if (!FreeLibrary(dll->handle)) {
    DWORD error_code = GetLastError();
    dll->error_code = win32_error_to_dll_error(error_code);

#if DE100_INTERNAL && DE100_SLOW
    win32_set_error_detail("de100_dll_close:FreeLibrary", NULL, error_code);
#endif
    return dll->error_code;
  }

#else
  // ─────────────────────────────────────────────────────────────────────
  // POSIX: dlclose
  // ─────────────────────────────────────────────────────────────────────
  dlerror(); // Clear any existing error

  if (dlclose(dll->handle) != 0) {
    const char *error_str = dlerror();
    dll->error_code = DLL_ERROR_UNKNOWN;

#if DE100_INTERNAL && DE100_SLOW
    posix_set_error_detail("de100_dll_close:dlclose", NULL, error_str);
#endif
    return dll->error_code;
  }

#endif

  // Success - mark as closed
  dll->handle = NULL;
  dll->is_valid = false;
  dll->error_code = DLL_SUCCESS;
  CLEAR_ERROR_DETAIL();

  return DLL_SUCCESS;
}

// ═══════════════════════════════════════════════════════════════════════════
// ERROR STRING TRANSLATION
// ═══════════════════════════════════════════════════════════════════════════

const char *de100_dll_strerror(De100DllErrorCode code) {
  switch (code) {
  case DLL_SUCCESS:
    return "Success";

  case DLL_ERROR_FILE_NOT_FOUND:
    return "Library file not found or path invalid";

  case DLL_ERROR_INVALID_FORMAT:
    return "Invalid library format (wrong architecture, corrupted, or not a "
           "shared library)";

  case DLL_ERROR_SYMBOL_NOT_FOUND:
    return "Symbol not found in library";

  case DLL_ERROR_ALREADY_LOADED:
    return "Library already loaded";

  case DLL_ERROR_ACCESS_DENIED:
    return "Access denied (permission error or file locked)";

  case DLL_ERROR_OUT_OF_MEMORY:
    return "Out of memory while loading library";

  case DLL_ERROR_INVALID_HANDLE:
    return "Invalid library handle (NULL or already closed)";

  case DLL_ERROR_UNKNOWN:
  default:
    return "Unknown DLL error";
  }
}

// ═══════════════════════════════════════════════════════════════════════════
// DEBUG UTILITIES
// ═══════════════════════════════════════════════════════════════════════════

#if DE100_INTERNAL && DE100_SLOW

const char *de100_dll_get_last_error_detail(void) {
  if (g_last_error_detail[0] == '\0') {
    return NULL;
  }
  return g_last_error_detail;
}

void de100_dll_debug_log_result(const char *operation, const char *path,
                                De100DllHandle dll) {
  if (dll.is_valid) {
    fprintf(stderr, "[DLL] %s('%s') = OK (handle=%p)\n", operation,
            path ? path : "(null)", (void *)dll.handle);
  } else {
    const char *detail = de100_dll_get_last_error_detail();
    fprintf(stderr, "[DLL] %s('%s') = FAILED: %s\n", operation,
            path ? path : "(null)", de100_dll_strerror(dll.error_code));
    if (detail) {
      fprintf(stderr, "      Detail: %s\n", detail);
    }
  }
}

#endif // DE100_INTERNAL && DE100_SLOW
