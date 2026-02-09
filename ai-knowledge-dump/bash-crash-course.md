# Bash Crash Course for Build Scripts

## 1. Variables

### Defining Variables

```bash
# No spaces around '='
NAME="value"

# Wrong (will error):
# NAME = "value"
```

### Using Variables

```bash
NAME="hello"

# Use $ to get value
echo $NAME          # prints: hello
echo "$NAME"        # prints: hello (safer, handles spaces)
echo "${NAME}"      # prints: hello (explicit boundaries)

# Example from build-common.sh:
ENGINE_CC="clang"
$ENGINE_CC src/main.c    # becomes: clang src/main.c
```

### Building Up Variables

```bash
# Start empty
FLAGS=""

# Append to it
FLAGS="$FLAGS -Wall"
FLAGS="$FLAGS -Werror"

# FLAGS is now: " -Wall -Werror"

# Example from build-common.sh:
ENGINE_BASE_FLAGS="-std=c11"
ENGINE_BASE_FLAGS="$ENGINE_BASE_FLAGS -ffunction-sections"
# ENGINE_BASE_FLAGS is now: "-std=c11 -ffunction-sections"
```

---

## 2. Command Substitution

### Capture Command Output

```bash
# $(command) runs command and returns its output
CURRENT_DIR="$(pwd)"
OS_NAME="$(uname -s)"

# Example from build-common.sh:
ENGINE_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

# Breaking it down:
# ${BASH_SOURCE[0]}  → path to current script
# dirname "..."      → get directory part only
# cd "..." && pwd    → go there and print full path
# $(...)             → capture that path into ENGINE_DIR
```

---

## 3. Conditionals

### If Statements

```bash
# Basic structure
if [ condition ]; then
    # do something
elif [ other_condition ]; then
    # do something else
else
    # fallback
fi

# String comparison
if [ "$NAME" = "value" ]; then
    echo "match"
fi

# File exists
if [ -f "build/game" ]; then
    echo "file exists"
fi

# Command succeeded
if command -v clang &> /dev/null; then
    echo "clang is installed"
fi
```

### Double Brackets (Extended Tests)

```bash
# [[ ]] allows pattern matching
if [[ "$OS" == "Linux"* ]]; then
    echo "starts with Linux"
fi

# Example from build-common.sh:
if [[ "$ENGINE_OS_NAME" != "Darwin" ]]; then
    ENGINE_BASE_FLAGS="$ENGINE_BASE_FLAGS -D_POSIX_C_SOURCE=200809L"
fi
```

---

## 4. Case Statements

```bash
# Like switch/case in C
case "$VALUE" in
    "option1")
        echo "matched option1"
        ;;
    "option2"|"option3")
        echo "matched option2 or option3"
        ;;
    *)
        echo "default case"
        ;;
esac

# Example from build-common.sh:
case "$ENGINE_OS_NAME" in
    Linux*)
        ENGINE_OS_TYPE="linux"
        ;;
    Darwin*)
        ENGINE_OS_TYPE="macos"
        ;;
    MINGW*|MSYS*|CYGWIN*)
        ENGINE_OS_TYPE="windows"
        ;;
    *)
        ENGINE_OS_TYPE="posix"
        ;;
esac
```

---

## 5. Functions

### Defining Functions

```bash
# Define
my_function() {
    echo "Hello from function"
}

# Call
my_function
```

### Function Arguments

```bash
greet() {
    local name="$1"    # first argument
    local age="$2"     # second argument
    echo "Hello $name, you are $age"
}

greet "John" "25"    # prints: Hello John, you are 25

# Example from build-common.sh:
engine_set_backend() {
    local backend="$1"    # first argument passed to function

    case "$backend" in
        x11)
            ENGINE_BACKEND_LIBS="-lX11 -lXrandr"
            ;;
    esac
}

# Called from build.sh:
engine_set_backend "$BACKEND"    # passes value of $BACKEND as $1
```

### Return Values

```bash
check_something() {
    if [ -f "file.txt" ]; then
        return 0    # success (true)
    else
        return 1    # failure (false)
    fi
}

# Use in if
if check_something; then
    echo "success"
fi

# Or check with !
if ! check_something; then
    echo "failed"
    exit 1
fi

# Example from build.sh:
if ! engine_set_backend "$BACKEND"; then
    exit 1
fi
```

---

## 6. Source (Import)

```bash
# source runs another script in current shell
# All variables/functions become available

# file: config.sh
MY_VAR="hello"
my_func() { echo "hi"; }

# file: main.sh
source ./config.sh
echo $MY_VAR      # prints: hello
my_func           # prints: hi

# Example from build.sh:
source "$SCRIPT_DIR/../engine/build-common.sh"

# After this line, build.sh can use:
# - $ENGINE_CC
# - $ENGINE_BASE_FLAGS
# - $ENGINE_LINKER_FLAGS
# - engine_set_backend()
# - engine_get_all_platform_src()
# etc.
```

---

## 7. Argument Parsing

```bash
# $# = number of arguments
# $1, $2, ... = individual arguments
# $@ = all arguments
# shift = remove first argument, shift others down

while [[ $# -gt 0 ]]; do    # while argument count > 0
    case "$1" in
        --name=*)
            NAME="${1#*=}"    # remove everything up to and including '='
            ;;
        --verbose)
            VERBOSE=true
            ;;
        *)
            echo "Unknown: $1"
            exit 1
            ;;
    esac
    shift    # move to next argument
done

# Example: ./build.sh --backend=x11 --build-game
#
# Iteration 1: $1 = "--backend=x11"
#   BACKEND="${1#*=}" → BACKEND="x11"
#   shift
#
# Iteration 2: $1 = "--build-game"
#   SHOULD_BUILD_GAME=true
#   shift
#
# Iteration 3: $# = 0, loop ends
```

### String Manipulation

```bash
VALUE="--backend=x11"

# ${VAR#pattern} = remove shortest match from start
echo "${VALUE#*=}"    # prints: x11

# ${VAR##pattern} = remove longest match from start
# ${VAR%pattern} = remove shortest match from end
# ${VAR%%pattern} = remove longest match from end
```

---

## 8. Complete Flow: build-common.sh → build.sh

```
┌─────────────────────────────────────────────────────────────────────────────┐
│ project/engine/build-common.sh                                              │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                             │
│  1. Detect OS                                                               │
│     ENGINE_OS_TYPE="linux"                                                  │
│                                                                             │
│  2. Check for clang                                                         │
│     ENGINE_CC="clang"                                                       │
│                                                                             │
│  3. Set base flags                                                          │
│     ENGINE_BASE_FLAGS="-std=c11 -ffunction-sections..."                     │
│                                                                             │
│  4. Set linker flags                                                        │
│     ENGINE_LINKER_FLAGS="-Wl,--gc-sections"                                 │
│                                                                             │
│  5. Set shared lib flags                                                    │
│     ENGINE_SHARED_FLAGS="-shared -fPIC..."                                  │
│                                                                             │
│  6. Define source file paths                                                │
│     ENGINE_MAIN_SRC="/full/path/to/engine/main.c"                           │
│     ENGINE_COMMON_SRC="/full/path/to/engine/_common/time.c ..."             │
│                                                                             │
│  7. Define functions                                                        │
│     engine_set_backend() { ... }                                            │
│     engine_get_all_platform_src() { ... }                                   │
│     engine_get_exe_name() { ... }                                           │
│     engine_get_shared_name() { ... }                                        │
│                                                                             │
└─────────────────────────────────────────────────────────────────────────────┘
                                    │
                                    │ source
                                    ▼
┌─────────────────────────────────────────────────────────────────────────────┐
│ project/handmadehero/build.sh                                               │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                             │
│  1. Source engine config                                                    │
│     source "$SCRIPT_DIR/../engine/build-common.sh"                          │
│     # Now all ENGINE_* variables and functions are available               │
│                                                                             │
│  2. Parse arguments                                                         │
│     --backend=x11 → BACKEND="x11"                                           │
│     --build-game  → SHOULD_BUILD_GAME=true                                  │
│                                                                             │
│  3. Build COMMON_FLAGS using ENGINE_BASE_FLAGS                              │
│     COMMON_FLAGS="-Isrc $ENGINE_BASE_FLAGS -g -O0 -Wall..."                 │
│     # Result: "-Isrc -std=c11 -ffunction-sections... -g -O0 -Wall..."       │
│                                                                             │
│  4. Get output filenames                                                    │
│     GAME_MAIN_LIB="build/$(engine_get_shared_name main)"                    │
│     # On Linux: "build/libmain.so"                                          │
│     # On macOS: "build/libmain.dylib"                                       │
│     # On Windows: "build/main.dll"                                          │
│                                                                             │
│  5. Build game libraries                                                    │
│     GAME_FLAGS="$COMMON_FLAGS $ENGINE_SHARED_FLAGS -lm"                     │
│     $ENGINE_CC src/main.c -o "$GAME_MAIN_LIB" $GAME_FLAGS                   │
│     # Expands to:                                                           │
│     # clang src/main.c -o build/libmain.so -Isrc -std=c11 ... -shared ...   │
│                                                                             │
│  6. Set backend                                                             │
│     engine_set_backend "$BACKEND"                                           │
│     # Sets ENGINE_BACKEND_SRC and ENGINE_BACKEND_LIBS                       │
│                                                                             │
│  7. Build platform executable                                               │
│     PLATFORM_SRC="$(engine_get_all_platform_src)"                           │
│     $ENGINE_CC $PLATFORM_SRC -o "$PLATFORM_EXE" ... $ENGINE_BACKEND_LIBS    │
│                                                                             │
└─────────────────────────────────────────────────────────────────────────────┘
```

---

## 9. Variable Scope Example

```bash
# === build-common.sh ===
ENGINE_CC="clang"                    # Global, available after source
ENGINE_BASE_FLAGS="-std=c11"         # Global, available after source

engine_set_backend() {
    local backend="$1"               # Local to this function only
    ENGINE_BACKEND_LIBS="-lX11"      # Global, no 'local' keyword
}

# === build.sh ===
source ./build-common.sh

echo $ENGINE_CC                      # "clang" ✓
echo $ENGINE_BASE_FLAGS              # "-std=c11" ✓

engine_set_backend "x11"
echo $ENGINE_BACKEND_LIBS            # "-lX11" ✓
echo $backend                        # "" (empty, was local)
```

---

## 10. Quick Reference

| Syntax                 | Meaning                            |
| ---------------------- | ---------------------------------- |
| `VAR="value"`          | Set variable                       |
| `$VAR` or `${VAR}`     | Get variable value                 |
| `$(command)`           | Run command, capture output        |
| `source file.sh`       | Import/run script in current shell |
| `$1, $2, ...`          | Function/script arguments          |
| `$#`                   | Number of arguments                |
| `local VAR="x"`        | Variable local to function         |
| `[ condition ]`        | Test condition                     |
| `[[ pattern ]]`        | Extended test (pattern matching)   |
| `${VAR#pattern}`       | Remove pattern from start          |
| `return 0`             | Success (in function)              |
| `return 1`             | Failure (in function)              |
| `exit 1`               | Exit script with error             |
| `command &> /dev/null` | Suppress all output                |

---

## 11. Debugging Tips

```bash
# Print each command before running
set -x

# Stop on first error
set -e

# Print variable values
echo "DEBUG: ENGINE_CC = $ENGINE_CC"
echo "DEBUG: FLAGS = $COMMON_FLAGS"

# Check if variable is empty
if [ -z "$VAR" ]; then
    echo "VAR is empty!"
fi
```
