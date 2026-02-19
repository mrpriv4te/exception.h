# exception.h

Header-only exception handling library for C.

Implements structured `try` / `catch` / `throw` semantics on top of `setjmp` / `longjmp`, with:

* Thread-local exception state
* Nested exception propagation
* Formatted exception messages
* Deterministic termination on uncaught exceptions
* Cross-platform support (POSIX + Windows)

---

## Features

* Header-only (single `exception.h` file)
* C-only (explicitly rejects C++)
* Per-thread exception state (`_Thread_local` / `__thread` / `__declspec(thread)`)
* Nested `try` blocks
* Rethrow support
* Formatted messages via `printf`-style formatting
* Automatic cleanup at program exit
* Safe behavior for uncaught exceptions:

  * Main thread: prints error and exits with exception code
  * Worker thread: prints error and terminates the thread with exception code

---

## Installation

Copy `exception.h` into your project.

In **one** translation unit, define:

```c
#define EXCEPTION_IMPLEMENTATION
#include "exception.h"
```

In all other translation units:

```c
#include "exception.h"
```

---

## Basic Usage

```c
#include "exception.h"

int main(void)
{
    try {
        throw(1, "Something went wrong: %d", 42);
    }
    catch (1) {
        printf("Caught: %s (code %d)\n",
               exception()->message,
               exception()->code);
    }

    return 0;
}
```

### Semantics

* `throw(code, fmt, ...)`

  * `code` must be non-zero.
  * Formats and stores the message.
  * Transfers control to the nearest enclosing `try`.

* `catch(code)`

  * Executes if the thrown code matches.
  * Use `EXCEPTION_ALL` to catch any code.

* `rethrow()`

  * Propagates the current exception to the next outer `try`.

* `exception()`

  * Returns a pointer to the current `exception_t`.

---

## Uncaught Exceptions

If no matching `catch` is found:

* On the **main thread**:

  * Error is printed to `stderr`
  * Process exits with the exception code

* On a **worker thread**:

  * Error is printed to `stderr`
  * Thread exits with the exception code

In debug builds (when `NDEBUG` is not defined), diagnostics include:

* File
* Line
* Function name

In release builds, only the message and code are printed.

---

## Nested Exceptions

Nested calls work transparently:

```c
void inner(void) {
    throw(5, "Inner failure");
}

void outer(void) {
    try {
        inner();
    }
    catch (5) {
        throw(6, "Outer transformed failure");
    }
}
```

---

## Thread Safety

The library maintains exception state per thread using thread-local storage.

Each thread has:

* Independent exception code
* Independent message buffer
* Independent control-flow stack

Uncaught exceptions terminate only the offending thread (unless it is the main thread).

---

## Customization

You may override the following macros before including the header:

* `EXCEPTION_ASSERT`
* `EXCEPTION_STATIC_ASSERT`
* `EXCEPTION_CALLOC`
* `EXCEPTION_FREE`

This allows integration with custom allocators or assertion systems.

---

## Tests

The repository includes a test suite located in `tests/exception_tests.c`.

The tests are written using **cmocka** and validate:

* Basic `throw` / `catch` behavior
* Uncaught exception termination (process and thread)
* Nested exception propagation and rethrow
* Message replacement on consecutive throws
* Multithreaded correctness
* Thread-local isolation

### Building Tests (POSIX)

Example using GCC and cmocka:

```bash
gcc -std=c11 -Wall -Wextra -pthread \
    tests/exception_tests.c -lcmocka -o exception_tests
```

Run:

```bash
./exception_tests
```

### Building Tests (Windows / MSVC)

Ensure cmocka is installed and available in your include/library paths.

Compile the test file as a normal C program. `EXCEPTION_IMPLEMENTATION` is already defined inside the test file.

---

## Design Notes

* Implemented using a linked stack of `jmp_buf` frames.
* Message memory is dynamically allocated per throw.
* Previous message memory is freed automatically.
* `throw(0, ...)` is forbidden (mirrors `longjmp` constraints).

---

## Limitations

* Not compatible with C++.
* Does not unwind automatic storage like C++ exceptions.
* Resources must be released manually or via structured control.
* Relies on `setjmp` / `longjmp` semantics.

---

## License

```
MIT License

Copyright (c) 2025 Mr. Priv4te

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
```
