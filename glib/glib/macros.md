Title: Macros

# Macros

GLib provides a set of C pre-processor macros and symbols for commonly-used
language and platform features.

## Platform

`G_OS_WIN32`
:   This macro is defined only on Windows, so you can bracket Windows-specific
    code using `#ifdef G_OS_WIN32 ... #endif`.

`G_OS_UNIX`

:   This macro is defined only on UNIX and UNIX-like systems, so you can bracket
    UNIX-specific code in `#ifdef G_OS_UNIX ... #endif`. To detect whether to
    compile features that require a specific kernel or operating system, check
    for the appropriate OS-specific predefined macros instead, for example:

    - Linux kernel (any libc, including glibc, musl or Android): `#ifdef __linux__`
    - Linux kernel and GNU user-space: `#if defined(__linux__) && defined(__GLIBC__)`
    - FreeBSD kernel (any libc, including glibc): `#ifdef __FreeBSD_kernel__`
    - FreeBSD kernel and user-space: `#ifdef __FreeBSD__`
    - Apple operating systems (macOS, iOS, tvOS), regardless of whether
      Cocoa/Carbon toolkits are available: `#ifdef __APPLE__`

    See <https://sourceforge.net/p/predef/wiki/OperatingSystems/> for more.


`G_DIR_SEPARATOR`
:   The directory separator character. This is `'/'` on UNIX machines and `'\'` under Windows.

`G_DIR_SEPARATOR_S`
:   The directory separator as a string. This is `"/"` on UNIX machines and `"\"` under Windows.

`G_IS_DIR_SEPARATOR(ch)`
:   Checks whether a character is a directory separator. It returns true for `'/'` on UNIX machines and for `'\'` or `'/'` under Windows. Available since 2.6.

`G_SEARCHPATH_SEPARATOR`
:   The search path separator character. This is `':'` on UNIX machines and `';'` under Windows.

`G_SEARCHPATH_SEPARATOR_S`
:   The search path separator as a string. This is `":"` on UNIX machines and `";"` under Windows.

## Values

`TRUE`
:   Defines the true value for the `gboolean` type.

`FALSE`
:   Defines the false value for the `gboolean` type.

`NULL`
:   Defines the standard `NULL` pointer.

## Maths

`MIN(a, b)`
:   Calculates the minimum of `a` and `b`.

`MAX(a, b)`
:   Calculates the maximum of `a` and `b`.

`ABS(value)`

:   Calculates the absolute value of a given numerical value. The absolute value
    is simply the number with any negative sign taken away.

    For example,

    - ABS(-10) is 10.
    -ABS(10) is also 10.


`CLAMP(value, low, high)`

:   Ensures that a value is between the limits set by `low` and `high`. If `low` is
    greater than `high` the result is undefined.

    For example,

    - CLAMP(5, 10, 15) is 10.
    - CLAMP(15, 5, 10) is 10.
    - CLAMP(20, 15, 25) is 20.


`G_APPROX_VALUE(a, b, epsilon)`

:   Evaluates to true if the absolute difference between the given numerical
    values `a` and `b` is smaller than `epsilon`, and to false otherwise.

    For example,

    - `G_APPROX_VALUE (5, 6, 2)` evaluates to true
    - `G_APPROX_VALUE (3.14, 3.15, 0.001)` evaluates to false
    - `G_APPROX_VALUE (n, 0.f, FLT_EPSILON)` evaluates to true if `n` is within
      the single precision floating point epsilon from zero

    Available since: 2.58


## Structure Access

`G_STRUCT_MEMBER(member_type, struct_pointer, offset)`
:   Returns a member of a structure at a given offset, using the given type.

`G_STRUCT_MEMBER_P(struct_pointer, offset)`
:   Returns an untyped pointer to a given offset of a struct.

`G_STRUCT_OFFSET(struct_type, member_name)`
:   Returns the offset, in bytes, of a member of a struct.
    Consider using standard C `offsetof()`, available since at least C89
    and C++98, in new code (but note that `offsetof()` returns a `size_t`
    rather than a `long`).


## Array Utilties

`G_N_ELEMENTS(array)`
:   Determines the number of elements in an array. The array must be
    declared so the compiler knows its size at compile-time; this
    macro will not work on an array allocated on the heap, only static
    arrays or arrays on the stack.


## Miscellaneous Macros

These macros provide more specialized features which are not needed so often
by application programmers.

`G_STMT_START`
:   Starts a multi-statement macro block so that it can be used in places
    where only one statement is expected by the compiler.

`G_STMT_END`
:   Ends a multi-statement macro block so that it can be used in places
    where only one statement is expected by the compiler.

`G_BEGIN_DECLS`
:   Used (along with `G_END_DECLS`) to bracket C header files that may be
    included by C++ sources. If the compiler in use is a C++ compiler, starts
    an `extern "C"` around the header.

`G_END_DECLS`
:   Used (along with `G_BEGIN_DECLS`) to bracket C header files that may be
    included by C++ sources, or compiled by a C++ compiler. If the compiler
    in use is a C++ compiler, ends the `extern "C"` block around the header.


`G_VA_COPY(ap1, ap2)`

:   Portable way to copy `va_list` variables.

    In order to use this function, you must include `string.h` yourself,
    because this macro may use `memmove()` and GLib does not include
    `string.h` for you.

    Each invocation of `G_VA_COPY (ap1, ap2)` must be matched with a
    corresponding `va_end (ap1)` call in the same function.

    This is equivalent to standard C `va_copy()`, available since C99
    and C++11, which should be preferred in new code.


`G_STRINGIFY(macro_or_string)`

:   Accepts a macro or a string and converts it into a string after
    preprocessor argument expansion. For example, the following code:

        #define AGE 27
        const gchar *greeting = G_STRINGIFY (AGE) " today!";

    is transformed by the preprocessor into (code equivalent to):

        const gchar *greeting = "27 today!";


`G_PASTE(identifier1, identifier2)`

:   Yields a new preprocessor pasted identifier `identifier1identifier2` from its expanded
    arguments `identifier1` and `identifier2`. For example,the following code:

        #define GET(traveller,method) G_PASTE(traveller_get_, method) (traveller)
        const char *name = GET (traveller, name);
        const char *quest = GET (traveller, quest);
        Color *favourite = GET (traveller, favourite_colour);

    is transformed by the preprocessor into:

        const char *name = traveller_get_name (traveller);
        const char *quest = traveller_get_quest (traveller);
        Color *favourite = traveller_get_favourite_colour (traveller);

    Available since: 2.20


`G_STATIC_ASSERT(expr)`

:   The `G_STATIC_ASSERT()` macro lets the programmer check a condition at
    compile time. The condition needs to be compile time computable. The
    macro can be used in any place where a `typedef` is valid.

    A `typedef` is generally allowed in exactly the same places that
    a variable declaration is allowed. For this reason, you should
    not use `G_STATIC_ASSERT()` in the middle of blocks of code.

    The macro should only be used once per source code line.

    Since: 2.20


`G_STATIC_ASSERT_EXPR(expr)`

:   The `G_STATIC_ASSERT_EXPR()` macro lets the programmer check a condition
    at compile time. The condition needs to be compile time computable.

    Unlike `G_STATIC_ASSERT()`, this macro evaluates to an expression
    and, as such, can be used in the middle of other expressions.
    Its value should be ignored. This can be accomplished by placing
    it as the first argument of a comma expression.

        #define ADD_ONE_TO_INT(x) \
          (G_STATIC_ASSERT_EXPR(sizeof (x) == sizeof (int)), ((x) + 1))

    Since: 2.30


`G_GNUC_EXTENSION`
:   Expands to `__extension__` when GCC is used as the compiler. This simply
    tells GCC not to warn about the following non-standard code when compiling
    with the `-pedantic` option.


`G_GNUC_CHECK_VERSION(major, minor)`

:   Expands to a check for a compiler with `__GNUC__` defined and a version
    greater than or equal to the major and minor numbers provided. For example,
    the following would only match on compilers such as GCC 4.8 or newer.

        #if G_GNUC_CHECK_VERSION(4, 8)
        // ...
        #endif

    Since: 2.42


`G_GNUC_BEGIN_IGNORE_DEPRECATIONS`

:   Tells GCC (if it is a new enough version) to temporarily stop emitting
    warnings when functions marked with `G_GNUC_DEPRECATED` or
    `G_GNUC_DEPRECATED_FOR` are called. This is useful for when you have
    one deprecated function calling another one, or when you still have
    regression tests for deprecated functions.

    Use `G_GNUC_END_IGNORE_DEPRECATIONS` to resume warning again. (If you
    are not compiling with `-Wdeprecated-declarations` then neither macro
    has any effect.)

    This macro can be used either inside or outside of a function body,
    but must appear on a line by itself.

        static void
        test_deprecated_function (void)
        {
          G_GNUC_BEGIN_IGNORE_DEPRECATIONS
          g_assert_cmpint (my_mistake (), ==, 42);
          G_GNUC_END_IGNORE_DEPRECATIONS
        }

    Both this macro and the corresponding `G_GNUC_END_IGNORE_DEPRECATIONS`
    are considered statements, so they should not be used around branching
    or loop conditions; for instance, this use is invalid:

        G_GNUC_BEGIN_IGNORE_DEPRECATIONS
        if (check == some_deprecated_function ())
        G_GNUC_END_IGNORE_DEPRECATIONS
          {
            do_something ();
          }

    and you should move the deprecated section outside the condition

        // Solution A
        some_data_t *res;

        G_GNUC_BEGIN_IGNORE_DEPRECATIONS
        res = some_deprecated_function ();
        G_GNUC_END_IGNORE_DEPRECATIONS

        if (check == res)
          {
            do_something ();
          }

        // Solution B
        G_GNUC_BEGIN_IGNORE_DEPRECATIONS
        if (check == some_deprecated_function ())
          {
            do_something ();
          }
        G_GNUC_END_IGNORE_DEPRECATIONS

    Since: 2.32


`G_GNUC_END_IGNORE_DEPRECATIONS`

:   Undoes the effect of `G_GNUC_BEGIN_IGNORE_DEPRECATIONS`, telling
    GCC to resume outputting warnings again (assuming those warnings
    had been enabled to begin with).

    This macro can be used either inside or outside of a function body,
    but must appear on a line by itself.

    Since: 2.32


`G_DEPRECATED`

:   This macro is similar to `G_GNUC_DEPRECATED`, and can be used to mark
    functions declarations as deprecated. Unlike `G_GNUC_DEPRECATED`, it is
    meant to be portable across different compilers and must be placed
    before the function declaration.

        G_DEPRECATED
        int my_mistake (void);

    Since: 2.32


`G_DEPRECATED_FOR(f)`

:   This macro is similar to `G_GNUC_DEPRECATED_FOR`, and can be used to mark
    functions declarations as deprecated. Unlike `G_GNUC_DEPRECATED_FOR`, it
    is meant to be portable across different compilers and must be placed
    before the function declaration.

        G_DEPRECATED_FOR(my_replacement)
        int my_mistake (void);

    Since: 2.32


`G_UNAVAILABLE(major, minor)`

:   This macro can be used to mark a function declaration as unavailable.
    It must be placed before the function declaration. Use of a function
    that has been annotated with this macros will produce a compiler warning.

    Since: 2.32


`GLIB_DISABLE_DEPRECATION_WARNINGS`
:   A macro that should be defined before including the `glib.h` header.
    If it is defined, no compiler warnings will be produced for uses
    of deprecated GLib APIs.


`G_GNUC_INTERNAL`

:   This attribute can be used for marking library functions as being used
    internally to the library only, which may allow the compiler to handle
    function calls more efficiently. Note that static functions do not need
    to be marked as internal in this way. See the GNU C documentation for
    details.

    When using a compiler that supports the GNU C hidden visibility attribute,
    this macro expands to `__attribute__((visibility("hidden")))`.
    When using the Sun Studio compiler, it expands to `__hidden`.

    Note that for portability, the attribute should be placed before the
    function declaration. While GCC allows the macro after the declaration,
    Sun Studio does not.

        G_GNUC_INTERNAL
        void _g_log_fallback_handler (const gchar    *log_domain,
                                      GLogLevelFlags  log_level,
                                      const gchar    *message,
                                      gpointer        unused_data);

    Since: 2.6


`G_C_STD_VERSION`

:   The C standard version the code is compiling against, it's normally
    defined with the same value of `__STDC_VERSION__` for C standard
    compatible compilers, while it uses the lowest standard version
    in pure MSVC, given that in such compiler the definition depends on
    a compilation flag.

    This is granted to be undefined when compiling with a C++ compiler.

    See also: `G_C_STD_CHECK_VERSION` and `G_CXX_STD_VERSION`

    Since: 2.76


`G_C_STD_CHECK_VERSION(version)`

:   Macro to check if the current compiler supports a specified version
    of the C standard. Such value must be numeric and can be provided both
    in the short form for the well-known versions (e.g. `90`, `99`...) or in
    the complete form otherwise (e.g. `199000L`, `199901L`, `205503L`...).

    When a C++ compiler is used, the macro is defined and evaluates to false.

    This value is compared against `G_C_STD_VERSION`.

        #if G_C_STD_CHECK_VERSION(17)
        // ...
        #endif

    See also: `G_CXX_STD_CHECK_VERSION`

    Since: 2.76


`G_CXX_STD_VERSION`

:   The C++ standard version the code is compiling against, it's defined
    with the same value of `__cplusplus` for C++ standard compatible
    compilers, while it uses `_MSVC_LANG` in MSVC, given that the
    standard definition depends on a compilation flag in such compiler.

    This is granted to be undefined when not compiling with a C++ compiler.

    See also: `G_CXX_STD_CHECK_VERSION` and `G_C_STD_VERSION`

    Since: 2.76


`G_CXX_STD_CHECK_VERSION(version)`

:   Macro to check if the current compiler supports a specified @version
    of the C++ standard. Such value must be numeric and can be provided both
    in the short form for the well-known versions (e.g. `11`, `17`...) or in
    the complete form otherwise (e.g. `201103L`, `201703L`, `205503L`...).

    When a C compiler is used, the macro evaluates to false.

    This value is compared against `G_CXX_STD_VERSION`.

        #if G_CXX_STD_CHECK_VERSION(20)
        // ...
        #endif

    See also: `G_C_STD_CHECK_VERSION`

    Since: 2.76


`G_LIKELY(expr)`

:   Hints the compiler that the expression is likely to evaluate to
    a true value. The compiler may use this information for optimizations.

        if (G_LIKELY (random () != 1))
          g_print ("not one");

    Since: 2.2


`G_UNLIKELY(expr)`

:   Hints the compiler that the expression is unlikely to evaluate to
    a true value. The compiler may use this information for optimizations.

        if (G_UNLIKELY (random () == 1))
          g_print ("a random one");

    Since: 2.2


`G_STRLOC`
:   Expands to a string identifying the current code position.

`G_STRFUNC`
:   Expands to a string identifying the current function. Since: 2.4

`G_HAVE_GNUC_VISIBILITY`
:   Defined to 1 if GCC-style visibility handling is supported.
