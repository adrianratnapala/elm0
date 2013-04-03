/*----------------------------------------------------------------------------
  elm: errors, logging and malloc.

  The elm module provies three commonly used utilities which are conceptually
  quite different, but can entangle at the impelmentation level.  These
  are:

  errors  - describes error events and helps handle them (for now by exiting
            the program, but we might do exceptions in future).

  logging - writes (log) events to a stdio stream.

  malloc  - wrapper for malloc() to allocate memory or die trying.  We never
            return anything but success (in future it will be possible to trap
            failures).


  Copyright (C) 2012, Adrian Ratnapala, under the ISC license. See file LICENSE.
*/

#ifndef ELM_H
#define ELM_H

#ifndef CHECK_FMT
# ifdef __GNUC__
#  define CHECK_FMT(N) __attribute__((format(printf,(N),(N)+1)))
# else
#  define CHECK_FMT(N)
# endif
#endif

#include <stdio.h>
#include <setjmp.h>

// ---------------------------------------------------------------------------------

/*
  Elm version IDs are utf-8 strings comprised of "elm0-" followed by a sequence
  of one or more numbers:
        * Each number is in the range [0, 1000).
        * Each number is space padded to three characters.
        * Each number (including the last!) is followed a "." or "-".

  IDs can be converted reversibly into a conventional looking version strings
  by stripping out the spaces and any trailing '.'; unlike conventional
  strings, IDs can be compared using strcmp().

  Here are some examples:
        0.5 release         "elm0-  0.  5."
        0.42 pre            "elm0-  0. 42-"
        0.42 pre 2          "elm0-  0. 42-  2."   // try not to do this.
        0.42 release        "elm0-  0. 42."
        0.42 post           "elm0-  0. 42.   ."
        0.42.3 release      "elm0-  0. 42.  3."

  Here the unnumbered "pre" and "post" describe everyday builds done during
  debugging.

  You can obtain the version of elm that you compiled against using the macro:
*/

#ifndef ELM_VERSION
#define ELM_VERSION "elm0-  0.  5.   ."
#endif

/* At run time you can get the version ID of the linked library using:*/
extern const char *elm_version();

/*
  Many parts of elm use the LogMeta struct to hold metadata about various
  events that happen in the program.  For now these metadata are only the
  source code location (filename, line number, function) where the event
  occurred.  In future we might include things like timestamps.
*/
typedef struct LogMeta LogMeta;
struct LogMeta {
        const char *func;
        const char *file;
        int line;
};


/*-- Errors -------------------------------------------------------------------
  Errors are our poor man's exception objects.  They are created when some bad
  event happens, and contain data to describe that event.
*/
typedef struct Error Error;


/*
  In principle, errors can come in multiple, polymorphic types because each
  error object has a pointer to an method table which is a C struct of type
  ErrorType.  Error types are purely runtime entities, not C types; at compile
  time, errors are all of type "Error".)
*/
typedef struct ErrorType ErrorType;
struct ErrorType {
        /* fwrite sends a human readable representation to a stdio stream. */
        int  (*fwrite)(Error *e, FILE *out);
        /* cleanup is called once the object is no longer needed.  */
        void (*cleanup)(void *data);
};

/*
  To define a new error type named "my_error", you need to define the two
  methods above, and then define an ErrorType struct which points to them.
  Finally you need to write a constructor function of the signature *and name*:

        Error *init_my_error(Error *e)

  Where "e" is an already allocated object which must be initialised.  The
  error struct is defined as:
*/
struct Error {
        const ErrorType *type;  // method table
        void      *data;  // different error types define meanings for this
        LogMeta    meta;  // type invariant meta data (where & when)
};

/*
  The constructor must set the "type" and "data" fields, but don't worry about
  "meta", it will be set for you.  The constructor must return the same pointer
  "e" that it was given.
*/

/*
  You will create an error of arbitrary type T using the macro.
        ERROR_WITH(T, type specific arguments)
*/
#define ERROR_WITH(T, ...) \
        init_##T( elm_mkerr(__FILE__,__LINE__,__func__), __VA_ARGS__ )

extern Error *elm_mkerr(const char *file, int line, const char *func);

/*
  Elm has two predfined error types.  The most basic is simply called "error";
  it just wraps a message string.  Although you can create error objects with

        ERROR_WITH(error, "some message"),

  you will normally use the shortcut:

        ERROR("some message")

  Both of these return a pointer to a newly alocated error that must
  be destroyed using destroy_error()
*/
extern Error *init_error(Error *e, const char *zfmt, ...) CHECK_FMT(2);
extern const ErrorType *const error_type;
#define ERROR(...) ERROR_WITH(error, __VA_ARGS__ )

/*
  The other predefined error type is sys_error, which wraps up `errno` like
  error codes.  If you have an errno, you can do:

        SYS_ERROR(errno, msg_prefix)

  If your error is linked to a specific file which you know the name of, then
  you should use:

        IO_ERROR(filename, errno, msg_prefix)

  But in spite of the name, IO_ERROR actually produces a normal SYS_ERROR
  object.
*/
extern Error *init_sys_error(Error *e, const char* zname, int errnum,
                                       const char *zmsg);
extern const ErrorType *const sys_error_type;
#define SYS_ERROR(N,M) ERROR_WITH(sys_error,  0, (N), (M))
#define IO_ERROR(F,N,M) ERROR_WITH(sys_error, F, (N), (M))

#define SYS_PANIC(N,M) panic(SYS_ERROR(N,M))
#define IO_PANIC(F,N,M) panic(IO_ERROR(F,N,M))


/*
  To discard an error object, call destroy_error.  This will first call the
  cleanup() method, and then free the object itself.
*/
extern void destroy_error(Error *e);


/*-- Panic --------------------------------------------------------------------
  Extreme errors can be handled using panic(), which either:
        -> Logs a standard error and then calls exit(), or
        -> Unwinds the stack much like exception handling.

  Be warned, this is C, there is no garbage collection or automatic destructor
  calling, which can make stack unwinding less useful than in other languages.
*/

/* You can panic using an valid (and non-NULL) error object by calling. */
extern void panic(Error *e);

/*
   Or you can create a new error in analogy to ERROR_WITH and ERROR, and then
   immediately panic with it.
*/
#define PANIC_WITH(T, ...) panic(ERROR_WITH(T, __VA_ARGS__))
#define PANIC(...) panic(ERROR(__VA_ARGS__)) // a new message error


/*
    By default, panic() will result in a call to exit().  If you don't want
    this to happen, you must first allocate a PanicReturn object R and then call
    the macro TRY(R)
*/
typedef struct PanicReturn PanicReturn;
struct PanicReturn {
        jmp_buf jmp_buf; /*MUST be first*/
        PanicReturn *prev;
        Error       *error;
};

/*
    TRY returns either NULL or an error raised by a panic.  If it returns NULL,
    code exectues as normal until a panic, at which point execution returns to
    the TRY call which will now return the corresponding error.  In addition
    the ".error" member of the PanicReturn object will also be set to the same
    error.  Once you no longer want to protect your errors this way, call
    NO_WORRIES.  Every TRY must have a corresponding NO_WORRIES; any number of
    TRY / NO_WORRIES pairs can be nested.

    One good way to use this is

        PanicReturn ret;

        if(ret.error = TRY(ret)) { // the assignment he is redundant, but nice.
                // handle the error
                destroy_error(ret.error);     // remember to do this!
                return -1;
        }

        ... do something which might panic() ...

        NO_WORRIES(ret)

    If you get an error that you can't handle, you can always panic again.
    This is because code which executes when "TRY(...) != NULL" behaves as if
    NO_WORRIES has already been called.  The above example avoids a double call
    to NO_WORRIES because of the "return"; if you want to carry on inside your
    function after trapping an error, you need

            PanicReturn ret;
            if( TRY(ret) ) {
                ... clean up after error ...
            } else {
                ... something dangerous
                NO_WORRIES(ret);
            }

            ... rest of function ...
*/
#define TRY(R) (_PANIC_SET(R) ? _panic_pop(&(R)) :  0)
#define NO_WORRIES(R) _panic_pop(&(R))

/*
   If you ever want to know whether or not you are inside a TRY/NO_WORRIES
   pair, you can call
 */
int panic_is_caught();

#define _PANIC_SET(R) (_panic_set_return(&(R))||setjmp(*(jmp_buf*)(&R)))
extern Error *_panic_pop(PanicReturn *check);
extern int _panic_set_return(PanicReturn *ret);

/* One common reason to catch serious errors is in unit tests - to see that
   code is throwing them when it should.  Assuming you use 0unit, you can make
   these tests cleaner using:
*/
#define CHK_PANIC(T, R)\
        {if( (R).error = TRY((R)) ){          \
                CHK((R).error->type ==(T));   \
                destroy_error((R).error);     \
        } else {

#define CHK_PANIC_END(R)                               \
                NO_WORRIES((R));                       \
                CHK(!"Expected panic never happened!");\
        }}

/* The idea is you do:

        PanicReturn ret;
        CHK_PANIC(expected_error_type, ret);
                ... some code here that MUST panic() with an error of
                    expected_error_type ...
        END_CHK_PANIC(ret);
*/



/*-- Logging ------------------------------------------------------------------
Logger objects take human-readable messages about events in your program,
decorate them with metadata and then (optionally) write them some stdio stream
(FILE*).  Different loggers can decorate messages differently, and write them
to different streams.  Loggers might also just swallow the messages; this makes
it possible to log verbosely, but then suppress annoying messages without
changing much code.
*/

typedef struct Logger Logger;

/*
  Elm defines three loggers, they are always available, without initialisation.
*/
extern Logger
       _elm_null_log,// Log to nowhere, swallows all messages,
       _elm_std_log, // Log to standard output
       _elm_err_log, // Log to standard error
       _elm_dbg_log; // Log to standard error, but include medatadata
                 //     (FILENAME:LINENUM in FUNCNAME)


#define null_log (&_elm_null_log)
#define std_log (&_elm_std_log)
#define err_log (&_elm_err_log)
#define dbg_log (&_elm_dbg_log)

/*
  One common use for loggers (over printf), is to allow you to write code that
  always sends log messages which can be switched off en masse without changing
  the bulk of the code.  You can do this by referring to these four logs by
  aliases, such a "info", "verbose" or just "log".  Then you can assign these
  aliases to "null_log" when you don't want to see the corresponding output.

  For more flexibility, you might want to create you own loggers by calling
*/
extern Logger *new_logger(const char *zname, FILE *stream, const char *opts);
/*
  which creates a logger that writes to "stream". Its name "zname", is
  prepended before all output messages (along with some punctuation).  If
  "stream" is NULL, you will get a null logger; it will silently ignore all
  messages.

  You can modify the style of logging by setting "opts" to be non-NULL, this
  string is just a list of option charactors, the only one defined so far is
  'd' which causes the logger to print out the source location metadata (like
  the debug logger).  All other option characters are ignored, in this version
  of elm.  opts==NULL is equivalent to opts="".

  Loggers are reference counted, you can increment and decrement references
  using:
*/
Logger *ref_logger(Logger *lg);
Error *destroy_logger(Logger *lg);
/*
  (In spite of its name, `destroy_logger` only destroys the logger when
  the reference count drops to zero).  These function do nothing at all
  to the standard (statically allocated) loggers.
*/

/*
  To log a message, call
        LOG_F(logger, fmt, ...)
  Where "logger" is the logger,
        "fmt"    is a printf-style format string,
        "..."    are zero or more arguments to munge into the string.

  This macro returns the number of bytes written to the output stream, or -1 on
  error, in which case errno is set appropriately.
*/

#define LOG_F(L,...) log_f(L, __FILE__, __LINE__, __func__,  __VA_ARGS__)
extern int log_f(Logger *lg,
           const char *file,
           int         line,
           const char *func,
           const char *fmt,
           ...) CHECK_FMT(5);

/*
   You can also log an error using log_error.  The metadata (such as the line
   number) will come from the error, not from the location of the logging call.
   The human readable text produced by the err->fwrite() method will also be
   logged.
 */
extern int log_error(Logger *lg, Error *err);

#define LOG_UNLESS(L, T) do {\
                        if(!(T)) log_f(L, __FILE__, __LINE__, __func__, #T); \
               } while(0)

#define DBG_UNLESS(T) LOG_UNLESS(dbg_log, T)



/*-- Malloc ------------------------------------------------------------------

  Code is much simpler when there are no possible ways to fail.  The following
  macros help you in the case where the only possible error is failed memory
  allocation (running out of virtual address space).

  MALLOC() just wraps malloc(), except that it never returns NULL.  If malloc()
  fails, the program simply exits with a detailed error message.  This saves
  you from writing error detection code for events that are very rare and
  almost impossible to recover from.

  If you do detect an out of memory condition yourself, but you want to treat
  it in the same way as a failed MALLOC(), you can call PANIC_NOMEM().  In
  future it will be possible to trap this error using TRY(), bit for now this
  error is always fatal.

  ZALLOC() is the same as MALLOC() except it zeros the allocated memory.
*/


extern void panic_nomem(const char* file, int line, const char *func);
extern void *malloc_or_die(const char* file, int line, const char *func, size_t n);
extern void *zalloc_or_die(const char* file, int line, const char *func, size_t n);
#define PANIC_NOMEM() panic_nomem(__FILE__, __LINE__, __func__)
#define MALLOC(N) malloc_or_die(__FILE__, __LINE__, __func__, N)
#define ZALLOC(N) zalloc_or_die(__FILE__, __LINE__, __func__, N)




#endif /*ELM_H*/
