#ifndef EXCEPTION_H_
#define EXCEPTION_H_

#ifdef __cplusplus
    #error "This header is C-only."
#endif

#ifndef EXCEPTIONDEF
    #define EXCEPTIONDEF
#endif /* EXCEPTIONDEF */

#if !defined(EXCEPTION_ASSERT) || !defined(EXCEPTION_STATIC_ASSERT)
    #include <assert.h>
    #ifndef EXCEPTION_ASSERT
        #define EXCEPTION_ASSERT assert
    #endif /* EXCEPTION_ASSERT */
    #ifndef EXCEPTION_STATIC_ASSERT
        #if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L
            #define EXCEPTION_STATIC_ASSERT static_assert
        #else
            #define EXCEPTION_CONCAT(a, b)       EXCEPTION_CONCAT_INNER(a, b)
            #define EXCEPTION_CONCAT_INNER(a, b) a##b
            #define EXCEPTION_STATIC_ASSERT(cond, msg) \
                typedef char EXCEPTION_CONCAT(         \
                    static_assertion_, __LINE__)[(cond) ? 1 : -1]
        #endif
    #endif /* EXCEPTION_STATIC_ASSERT */
#endif

#if !defined(EXCEPTION_CALLOC) || !defined(EXCEPTION_FREE)
    #include <stdlib.h>
    #ifndef EXCEPTION_CALLOC
        #define EXCEPTION_CALLOC calloc
    #endif /* EXCEPTION_CALLOC */
    #ifndef EXCEPTION_FREE
        #define EXCEPTION_FREE free
    #endif /* EXCEPTION_FREE */
#endif

#if defined(_MSC_VER) && !defined(__clang__)
    #pragma section(".CRT$XCU", read)
    #define EXCEPTION_CONSTRUCTOR(f)                                     \
        static void f(void);                                             \
        __declspec(allocate(".CRT$XCU")) static void (*f##_p)(void) = f; \
        static void f(void)
#else
    #define EXCEPTION_CONSTRUCTOR(f) \
        __attribute__((constructor)) static void f(void)
#endif

#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L
    #define EXCEPTION_NORETURN _Noreturn
#elif defined(__GNUC__) || defined(__clang__)
    #define EXCEPTION_NORETURN __attribute__((noreturn))
#elif defined(_MSC_VER)
    #define EXCEPTION_NORETURN __declspec(noreturn)
#else
    #define EXCEPTION_NORETURN
#endif

#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L
    #define EXCEPTION_THREAD_LOCAL _Thread_local
#elif defined(__GNUC__) || defined(__clang__)
    #define EXCEPTION_THREAD_LOCAL __thread
#elif defined(_MSC_VER)
    #define EXCEPTION_THREAD_LOCAL __declspec(thread)
#else
    #define EXCEPTION_THREAD_LOCAL
#endif

#ifdef _WIN32
    #ifdef _MSC_VER
        #include <Windows.h>
    #else
        #include <windows.h>
    #endif
typedef DWORD exception_thread_id_t;
    #define exception_thread_self()     GetCurrentThreadId()
    #define exception_thread_exit(code) ExitThread((DWORD)(code))
#else
    #include <pthread.h>
typedef pthread_t exception_thread_id_t;
    #define exception_thread_self()     pthread_self()
    #define exception_thread_exit(code) pthread_exit((void *)(intptr_t)(code))
#endif

#ifndef _WIN32
    #include <err.h>
#endif

#include <setjmp.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#define try                                                                   \
    for (int _exception_try_code = exception_try(setjmp(                      \
                 (exception_control_flow_push(&(control_flow_node_t){0})      \
                         ->head->jmp_buf))),                                  \
             _exception_try_handled = 0, _exception_try = 1;                  \
        _exception_try &&                                                     \
        (((_exception_try_code != 0) ? (void)exception_control_flow_pop()     \
                                     : (void)0),                              \
            1);                                                               \
        (_exception_try_code == 0                                             \
                ? (void)exception_control_flow_pop()                          \
                : (_exception_try_handled == 0                                \
                          ? (exception_rethrow(),                             \
                                exception_exit(__func__, __FILE__, __LINE__)) \
                          : exception_cleanup())),                            \
             _exception_try = 0)                                              \
        if (_exception_try_code == 0)

#define catch(code) else if ((_exception_try_handled = exception_catch(code)))

#define throw(code, ...)                                          \
    do {                                                          \
        EXCEPTION_STATIC_ASSERT(code != 0,                        \
            "throw(): exception code 0 is invalid: if the value " \
            "argument to longjmp is 0, the value returned by "    \
            "the corresponding setjmp is 1.");                    \
        exception_throw(code, __VA_ARGS__);                       \
        exception_exit(__func__, __FILE__, __LINE__);             \
    } while (0)

#define rethrow()                                     \
    do {                                              \
        exception_rethrow();                          \
        exception_exit(__func__, __FILE__, __LINE__); \
    } while (0)

#define EXCEPTION_ALL 0

typedef struct exception_s {
    const char *message;
    int code;
} exception_t;

typedef struct control_flow_node_s {
    jmp_buf jmp_buf;
    struct control_flow_node_s *next;
} control_flow_node_t;

typedef struct control_flow_s {
    control_flow_node_t *head;
} control_flow_t;

typedef struct exception_state_s {
    exception_t exception;
    control_flow_t control_flow;
} exception_state_t;

control_flow_t *exception_control_flow_push(control_flow_node_t *cflow);
control_flow_node_t *exception_control_flow_pop(void);

EXCEPTIONDEF const exception_t *exception(void);
EXCEPTIONDEF int exception_try(int code);
EXCEPTIONDEF bool exception_catch(int code);
EXCEPTIONDEF void exception_throw(int code, const char *fmt, ...);
EXCEPTIONDEF void exception_set_message(const char *fmt, va_list varg);
EXCEPTIONDEF void exception_rethrow(void);
EXCEPTIONDEF void exception_cleanup(void);
EXCEPTIONDEF EXCEPTION_NORETURN void exception_exit(
    const char *func, const char *file, int line);

#endif // EXCEPTION_H_

#ifdef EXCEPTION_IMPLEMENTATION

static EXCEPTION_THREAD_LOCAL exception_thread_id_t exception_main_thread_id;

static EXCEPTION_THREAD_LOCAL exception_state_t exception_state = {
    .exception = {.message = NULL, .code = 0},
    .control_flow = {.head = NULL},
};

EXCEPTION_CONSTRUCTOR(exception_init)
{
    exception_main_thread_id = exception_thread_self();
    (void)atexit(exception_cleanup);
}

EXCEPTIONDEF void exception_set_message(const char *fmt, va_list varg)
{
    char *message = NULL;
    va_list varg_copy;
    int size = 0;

    if (fmt != NULL) {
        va_copy(varg_copy, varg);
        size = vsnprintf(NULL, 0, fmt, varg_copy);
        va_end(varg_copy);
        if (size > 0) {
            message = EXCEPTION_CALLOC((size_t)size + 1, sizeof(char));
            if (message) {
                (void)vsnprintf(message, (size_t)size + 1, fmt, varg);
            }
        }
    }
    if (exception_state.exception.message != NULL) {
        EXCEPTION_FREE((void *)exception_state.exception.message);
    }
    exception_state.exception.message = message;
}

EXCEPTIONDEF control_flow_t *exception_control_flow_push(
    control_flow_node_t *cflow)
{
    EXCEPTION_ASSERT(cflow != NULL);

    cflow->next = exception_state.control_flow.head;
    exception_state.control_flow.head = cflow;
    return &exception_state.control_flow;
}

EXCEPTIONDEF control_flow_node_t *exception_control_flow_pop(void)
{
    control_flow_node_t *tmp = exception_state.control_flow.head;

    if (tmp != NULL) {
        exception_state.control_flow.head = tmp->next;
    }
    return tmp;
}

EXCEPTIONDEF const exception_t *exception(void)
{
    return &exception_state.exception;
}

EXCEPTIONDEF int exception_try(int code)
{
    exception_state.exception.code = code;
    return code;
}

EXCEPTIONDEF bool exception_catch(int code)
{
    if (code == EXCEPTION_ALL || exception_state.exception.code == code) {
        return true;
    }
    return false;
}

EXCEPTIONDEF void exception_throw(int code, const char *fmt, ...)
{
    EXCEPTION_ASSERT(
        code != 0 &&
        "exception_throw(): exception code 0 is invalid: if the value argument "
        "to longjmp is 0, the value returned by the corresponding setjmp is "
        "1.");

    control_flow_node_t *cflow = exception_state.control_flow.head;
    va_list varg;

    va_start(varg, fmt);
    exception_set_message(fmt, varg);
    va_end(varg);

    exception_state.exception.code = code;

    if (cflow != NULL) {
        longjmp(cflow->jmp_buf, code);
    }
}

EXCEPTIONDEF void exception_rethrow(void)
{
    control_flow_node_t *cflow = exception_state.control_flow.head;

    if (cflow != NULL) {
        longjmp(cflow->jmp_buf, exception_state.exception.code);
    }
}

EXCEPTIONDEF void exception_cleanup(void)
{
    if (exception_state.exception.message != NULL) {
        EXCEPTION_FREE((void *)exception_state.exception.message);
        exception_state.exception.message = NULL;
    }
    exception_state.exception.code = 0;
}

#ifdef _WIN32

static void warnx(const char *fmt, ...)
{
    char path[MAX_PATH];

    if (GetModuleFileNameA(NULL, path, sizeof(path)) != 0) {
        char *filename = path;
        for (char *p = path; *p; ++p) {
            if (*p == '\\' || *p == '/') {
                filename = p + 1;
            }
        }
        if (filename != path) {
            fprintf(stderr, "%s: ", filename);
        }
    }

    va_list ap;
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    fprintf(stderr, "\n");
    va_end(ap);
}

#endif

EXCEPTIONDEF void exception_exit(const char *func, const char *file, int line)
{
    const char *msg = exception_state.exception.message;
    int code = exception_state.exception.code;
    exception_thread_id_t thread_id = exception_thread_self();

    if (thread_id != exception_main_thread_id) {

#ifndef NDEBUG
        warnx("thread %ld: %s:%d: %s(): %s (code %d)", thread_id, file, line,
            func, msg, code);
#else
        warnx("thread %ld: %s (code %d)", thread_id, msg, code);
#endif

        exception_cleanup();
        exception_thread_exit(code);
    }

#ifndef NDEBUG
    warnx("%s:%d: %s(): %s (code %d)", file, line, func, msg, code);
#else
    warnx("%s (code %d)", msg, code);
#endif

    exception_cleanup();
    exit(code);
}

#endif // EXCEPTION_IMPLEMENTATION
