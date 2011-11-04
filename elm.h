/* ---------------------------------------------------------------------------
  elm: errors, logging and malloc.

  The elm module provies three commonly used utilities which are conceptually
  quite different, but can entangle at the impelmentation level.  These 
  are:
errors  - describes error events, and help handle them (for now by exiting the program, but we might do exceptions in future).

  logging - writes (log) events to a stdio stream.

  malloc  - wrapper for malloc() to allocate memory or die trying.  Never
            returns anything but success.          
*/ 

#ifndef ELM_H
#define ELM_H

#include <setjmp.h>

/*
  Many parts of elm use the LogMeta struct to hold metadata about various
  events that happen in the program.  For now these metadata are only the
  source code location (filename, line number, function) where the even
  occurred.  In future we might include things like timestamps.
*/
typedef struct LogMeta LogMeta;
struct LogMeta {
        const char *func;
        const char *file;
        int line;
};


/*-- Errors -------------------------------------------------------------------
  Errors are our poor man's version of an exception object.  They are created
  when some bad event happens, and contain data to describe that event.  
*/
typedef struct Error Error;

/*
  In principle, errors can come in multiple, polymorphic types because each
  error object has a pointer to an method table struct of type ErrorType.
*/
typedef struct ErrorType ErrorType;
struct ErrorType {
        /* fwrite sends a human readable representation to a stdio stream. */
        int  (*fwrite)(Error *e, FILE *out); 
        /* cleanup is called once the object is no longer needed.  */
        void (*cleanup)(Error *e);
};

/* 
  To define a new error type named "my_error", you need to define those two
  methods, and then define an ErrorType struct which points to them.  Finally
  you need to write a constructor function of the signature *and name*:

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
        ERROR(T, type specific arguments )
*/
#define ERROR(T, ...) \
        init_##T( elm_mkerr(__FILE__,__LINE__,__func__), __VA_ARGS__ )

extern Error *elm_mkerr(const char *file, int line, const char *func);

/*
  Elm has just one predfined error type "merror" (short for "message error"),
  which just wraps a message string.  You can create merror objects with

        ERROR(merror, "some message" ).   

  you can also use the shortcut:
        MERROR("some messsage")
*/

extern Error *init_merror(Error *e, const char *ztext);
extern const ErrorType *const merror_type;
#define MERROR(MSG) ERROR(merror, MSG )


/*
  To discard an error object, call error_destroy.  This will first call the
  cleanup() method, and then free the object itself.
*/
extern void error_destroy(Error *e);



/*-- Panic --------------------------------------------------------------------
  Extreme errors can be handled using panic(), which either:
        -> Logs a standard error and then calls exit(), or
        -> Unwind's the stack much like exception handling.

  Be warned, this is C, there is no garbage collection or automatic destructor
  calling, which can make stack unwinding less useful than in other languages.
*/

/* You can panic using an valid (and non-NULL) error object by calling. */
extern void panic(Error *e); 

/* 
   Or you can create a new error in analogy to ERROR and MERROR, and then
   immediately panic with it.   
*/
#define PANIC(T, ...) panic(ERROR(T, __VA_ARGS__)) // a new error of type T,
#define MPANIC(MSG) panic(MERROR(MSG)) // a new message error


/*
    By default, panic() will result in a call to exit().  If you don't want
    this to happen, you must first alloate a PanicReturn object R and then call
    the macro TRY(R)
*/
typedef struct PanicReturn PanicReturn;
struct PanicReturn {
        jmp_buf jmp_buf; /*MUST be first*/
        PanicReturn *prev;
        Error       *error;
};

/*
    TRY returns either a NULL pointer or an error raised by a panic.  If it
    returns a NULL, code exectues as normal until a panic, at which point
    execution returns to the TRY call which will now return the corresponding
    error.  In addition the ".error" membor of the PanicReturn object will also
    be set to the same error.  One you no longer wnat to protect your errors
    this way, call NO_WORRIES.

    A good way to use this  is 

        PanicReturn ret;

        if(ret.error = TRY(ret)) { // the assignment he is redundant, but nice.
                // handle the error 
                error_destroy(ret.error);     // remember to do this!
                return -1;
        }

        ... do something which might panic() ...

        NO_WORRIES(ret)


    Any number of TRY / NO_WORRIES pairs can be nested.  If you find you can't
    handle the error, you can always panic again.
*/
#define TRY(R) (_PANIC_SET(R) ? _panic_pop(&(R)) :  0)
#ifdef NDEBUG
#  define NO_WORRIES(R) _panic_pop(&(R))
#else
#  define NO_WORRIES(R) assert(!_panic_pop(&(R)))
#endif


#define _PANIC_SET(R) (_panic_set_return(&(R))||setjmp(*(jmp_buf*)(&R)))
extern Error *_panic_pop(PanicReturn *check);
extern int _panic_set_return(PanicReturn *ret);

/* One common reason to catch serious errors is in unit tests - to see that
   code is throwing them when it should.  Assuming you use 0unit, you
   can meake these tests cleaner using: 
*/
#define CHK_PANIC(T, R)\
        {if( (R).error = TRY((R)) ){          \
                CHK((R).error->type ==(T));   \
                error_destroy((R).error);     \
        } else {

#define END_CHK_PANIC(R)                               \
                NO_WORRIES((R));                       \
                CHK(!"Expected panic never happened!");\
        }}

/* The idea is you do:

        PanicReturn ret;
        CHK_PANIC( expected_error_type, ret);
                ... some code here that MUST panic() with an error of
                    expected_error_type ...
        END_CHK_PANIC( ret );
*/



/*-- Logging ------------------------------------------------------------------
Logger objects take human readable messages about events in your program,
decorate them with metadata, an then (optionally) write them some stdio stream
(FILE*).  Different loggers can decorate messages differently, write them to
different streams.  Loggers might also just swallow the messages, this makes it
possible to log verbosely, but then suppress annoying messages without changing
much code.
*/

typedef struct Logger Logger;

/*
  Elm defines three loggers, they are always available, without initialisation.
*/
extern Logger 
       *null_log,// Log to nowhere, swallows all messages,
       *std_log, // Log to standard output
       *err_log, // Log to standard error
       *dbg_log; // Log to standard error, but include medatadata
                 //     (FILENAME:LINENUM in FUNCNAME)

/*
  One common use for loggers (over printf), is to allow you to write code that
  always sends log messages which can be switched off en masse without changing
  the bulk of the code.  You can do this by referring to these four logs by
  aliases, such a "info", "verbose" or just "log".  Then you ca assign these
  aliases to one "null_log" when you don't want to see the corresponding
  output.

  For more flexibility, you might want to create you own loggers by calling
*/
extern Logger *logger_new(const char *zname, FILE *stream);
/*
  which creates a logger that writes to "stream". It's name "zname", is
  prepended before all output messages (along with some punctuation).  
  If "stream" is NULL, you will get a null logger - it will silently ignore all
  messages
  
  If you want to include source location metadata in messages, use 
*/
extern Logger *debug_logger_new(const char *zname, FILE *stream);

/* These loggers (but NOT the default ones) can be destoryed using. */
void logger_destroy(Logger *lg);

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
           const char *msg, 
           ...);

/* 
   You can also log an error using.  The metadata will come from the error,
   not from the location of the logging call.
 */
extern int log_error(Logger *lg, Error *err);

/*-- Malloc ------------------------------------------------------------------
*/


extern void panic_nomem(const char* file, int line, const char *func);
extern void *malloc_or_die(const char* file, int line, const char *func, size_t n);
extern void *zalloc_or_die(const char* file, int line, const char *func, size_t n);
#define PANIC_NOMEM() panic_nomem(__FILE__, __LINE__, __func__)
#define MALLOC(N) malloc_or_die(__FILE__, __LINE__, __func__, N)
#define ZALLOC(N) zalloc_or_die(__FILE__, __LINE__, __func__, N)




#endif /*ELM_H*/
