/* ---------------------------------------------------------------------------
elm: errors, logging and malloc.
*/ 

#define _GNU_SOURCE
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <unistd.h>
#include <stdint.h>

#include <sys/resource.h>

#include "0unit.h"
#include "elm.h"

/* Set this to one to test some emergency-fail code. */
#ifndef FAKE_FAIL
#define FAKE_FAIL 0
#endif


// Errors ---------------------------------------------------------------------

Error *elm_mkerr(const char *file, int line, const char *func) 
/* malloc()s an error & fills out the metadata. */
{
        Error* e = malloc(sizeof(Error));
        if(!e) {
                exit(1);
                // FIX: panic.
        }

        e->meta.file = file;
        e->meta.line = line;
        e->meta.func = func;
        return e;
}

void error_destroy(Error *e)
/* calls an error's cleanup method, and then free()s the error. */
{
        assert(e);
        assert(e->type);
        assert(e->type->cleanup);
        e->type->cleanup(e);
        free(e);
}

// -- Message Error - Just wraps a message string in an error object.


int merror_fwrite(Error *e, FILE *out)
/* Write error to stdio in human readable form. */
{
        return fwrite(e->data, 1, strlen(e->data), out);
}

void merror_cleanup(Error *e)
/* Called when the error is discarded, before it is free()d. */
{
        free(e->data);
}

static const ErrorType _merror_type = {
        fwrite    : merror_fwrite,
        cleanup   : merror_cleanup
};

const ErrorType *const merror_type = &_merror_type;

Error *init_merror(Error *e, const char *ztext)
{
        e->type = merror_type;
        e->data = strdup(ztext);
        return e;
}

// -- Test Error --

static int chk_error( Error *err, const ErrorType *type, 
                                  const char *zvalue )
{
        size_t size;
        char *buf;
        FILE *mstream;

        CHK( err->type == type );
        CHK( mstream = open_memstream(&buf, &size) );
        CHK( type->fwrite(err, mstream) == strlen(zvalue) );
        fclose(mstream); 

        CHK( size == strlen(zvalue) );
        CHK( !memcmp(zvalue, buf, size) );
        free(buf);

        return 1;
}

static int test_errors()
{
        int pre_line = __LINE__;
        Error *e = MERROR("goodbye world!");

        CHK(chk_error(e, merror_type, "goodbye world!"));
        CHK(!strcmp(e->meta.file, __FILE__));
        CHK(!strcmp(e->meta.func, __func__));
        CHK(e->meta.line == pre_line + 1);

        error_destroy(e);
        PASS();
}

// Raw Stderr -----------------------------------------------------------------

static void emergency_write(const char *str) 
{
        write(2, str, strlen(str));
}

static void emergency_message(const char *pre, LogMeta *meta, const char *post)
/* Log to stderr without using stdio. meta is ignored if it equals NULL. */
{
        emergency_write(pre);

        if(meta) {
                emergency_write(" (in");
                emergency_write(meta->file);
                emergency_write(":"); 
                emergency_write(meta->func);
                emergency_write(")"); 
        }

        emergency_write(": ");
        emergency_write(post);
        emergency_write("\n");
        fsync(2);
}

// Logs -----------------------------------------------------------------------

struct Logger {
        /*Loggers decorate messages, and send them to a stream. Or drop them.*/
        int  ready;   
        FILE *stream;         // the output stream
        const char *zname;    // prefix text emitted before each message


        /* User redefinable functions (methods) */
        int (*vprintf)(Logger *lg, LogMeta *meta, const char *msg, va_list va);
        int (*fwrite_prefix)(Logger *lg, LogMeta *meta);
};

static void init_static_logger(Logger *lg)
/* Idempotently ensures initialisation of builtin loggers before each use. */
{
        if(lg->ready)
                return;

        switch( (uintptr_t)lg->stream ) {
        case 0: break; /* leave NULL logs alone */
        case 1: lg->stream = stdout; break;
        case 2: lg->stream = stderr; break;
        default :
                assert( !"init_static_logger: invalid stream constant!" );
                break;
        }

        lg->ready = 1;
}

static int log_prefix(Logger *lg, LogMeta *meta)
{
        return fprintf(lg->stream, "%s: ", lg->zname);
}

static int dbg_prefix(Logger *lg, LogMeta *meta)
{
        return fprintf(lg->stream, "%s (%s:%d in %s): ",
                lg->zname, meta->file, meta->line, meta->func);
}

static int log_vprintf(Logger *lg, LogMeta *meta, const char *msg, va_list va)
/* method: format a message vprintf style, then log it. */
{
        init_static_logger(lg);

        int nprefix = lg->fwrite_prefix(lg, meta);
        if ( nprefix <= 0 ) 
                goto no_write;

        int nbody = vfprintf(lg->stream, msg, va);
        if ( nbody <= 0 ) 
                goto no_write;

        if ( fputc('\n', lg->stream) == EOF)
                goto no_write;

        if( fflush(lg->stream) == EOF )
                goto no_write;

        if(!FAKE_FAIL)
                return nbody + nprefix + 1;

no_write:
        emergency_message("LOGFAILED", meta, msg);
        return -1;
}

int log_error(Logger *lg, Error *err)
/* Convert an error to a string, then log it. Metadata come from the error. */
{
        init_static_logger(lg);
        if(!lg->stream) // is this a null log?
                return 0;

        const ErrorType *etype = err->type;
        assert(etype);
        
        if(FAKE_FAIL)
                goto no_write;

        int nprefix = lg->fwrite_prefix(lg, &err->meta);
        if ( nprefix <= 0 ) 
                goto no_write;

        // ask the error to write its own text
        int nbody = etype->fwrite(err, lg->stream);
        if ( nbody <= 0 ) 
                goto no_write;

        if ( fputc('\n', lg->stream) == EOF)
                goto no_write;

        if( fflush(lg->stream) == EOF )
                goto no_write;

        return nbody + nprefix + 1;

no_write:
        emergency_message("LOGFAILED", &err->meta, "Error logging error.");
        return -1;
}

// A handful of builtin loggers are statically allocated. 
static Logger _std_log = {
        ready   : 0,
        stream  : (FILE*)1,
        zname   : "LOG",
        vprintf : log_vprintf,
        fwrite_prefix : log_prefix,
};

static Logger _err_log = {
        ready   : 0,
        stream  : (FILE*)2,
        zname   : "ERROR",
        vprintf : log_vprintf,
        fwrite_prefix : log_prefix,
};

static Logger _dbg_log = {
        ready   : 0,
        stream  : (FILE*)2,
        zname : "DBG",
        vprintf : log_vprintf,
        fwrite_prefix : dbg_prefix,
};

static Logger _null_log = {
        ready   : 0,
        stream  : (FILE*)0,
        zname : "NULL",
        vprintf : log_vprintf,
        fwrite_prefix : dbg_prefix,
};

Logger *std_log = &_std_log, 
       *err_log = &_err_log,
       *dbg_log = &_dbg_log,
       *null_log = &_null_log;


// User created loggers ------.

Logger *logger_new(const char *zname, FILE *stream)
/* Create a standard logger that writes to "stream". */
{
        Logger *lg = malloc( sizeof(Logger) );
        if( !lg ) {
                //FIX: panic.
                return 0;
        }
        assert(zname);
                
        lg->stream  = stream;
        lg->vprintf = log_vprintf;
        lg->fwrite_prefix = log_prefix;
        lg->zname = strdup(zname);
        if( !lg ) {
                //fix: panic.
                return 0;
        }

        lg->ready = 1;
        return lg;
}

Logger *debug_logger_new(const char *zname, FILE *stream)
/* Create a logger that includes source location in prefixes. */
{
        Logger *lg = logger_new(zname, stream);
        lg->fwrite_prefix = dbg_prefix;
        return lg;
}

void logger_destroy(Logger *lg)
/* Frees a non-builtin logger. DO NOT USE ON STATIC LOGGERS */
{
        free((char*)lg->zname);
        free(lg);
}


int log_f(Logger *lg, 
           const char *file,
           int         line, 
           const char *func, 
           const char *msg, ...)
/* Format a message vprintf style, then log it. */
{
        va_list va;
        int n;
        init_static_logger(lg);
        if(!lg->stream) // is this a null log?
                return 0;

        assert(lg);
        va_start(va, msg);
                LogMeta m = {
                file : file,
                line : line,
                func : func,
        };
        n = lg->vprintf(lg, &m, msg, va);
        va_end(va);
        return n;
}

static int test_logging()
{
        static const char *expected_text =
                "TEST: Hello Logs!\n"
                "TEST: Hello Logs #2!\n"
                "TEST: goodbye world!\n"
                ;

        size_t size;
        char *buf;

        FILE *mstream = open_memstream(&buf, &size);
        CHK( mstream != NULL );

        Logger *lg = logger_new("TEST", mstream);
        CHK(lg);
        
        Logger *nlg = logger_new("NULL_TEST", 0);
        CHK(nlg);
        
        CHK( LOG_F(nlg, "Hello Logs!") == 0 );
        CHK( LOG_F(lg, "Hello Logs!") == 18);
        CHK( size == 18 );
        CHK( !memcmp(buf, expected_text, size) );

        LOG_F(nlg, "Hello Logs #%d!", 2);
        LOG_F(lg, "Hello Logs #%d!", 2);
        CHK( size == 18 + 21 );
        
        Error *e = ERROR(merror, "goodbye world!");
        CHK( log_error(nlg, e) == 0 );
        CHK( log_error(lg, e) == 21 );
        CHK( size == 18 + 21 + 21 );
        CHK( !memcmp(buf, expected_text, size) );

        logger_destroy(lg);
        logger_destroy(nlg);
        fclose(mstream);
        free(buf);
        error_destroy(e);

        PASS();
}

static int test_debug_logger()
{
        size_t size;
        char *buf, *expect;

        FILE *mstream = open_memstream(&buf, &size);
        CHK( mstream != NULL );

        Logger *lg = debug_logger_new("DTEST", mstream);
        CHK(lg);
       
        char *text = "Eeek, a (pretend) software bug!";
        int line_p = __LINE__;
        LOG_F(lg, text );
        int n = asprintf(&expect, "DTEST (%s:%d in %s): %s\n",
                __FILE__, line_p + 1, __func__, text );
        CHK( n > 0 );

        CHK( strlen(expect) == size );
        CHK( !memcmp(expect, buf, size) );
        
        free(expect);
        logger_destroy(lg);
        fclose(mstream);
        free(buf);

        PASS();
}


// Malloc ---------------------------------------------------------------------
/*
        A malloc() wrapper that checks the results and exits on failure, after
        printing an error message to stderr.  These are macros, not functions,
        because they gather source-location metadata.

        MALLOC(N)     - Allocate N bytes (or die trying).
        ZALLOC(N)     - Allocate N zeroed bytes.
        PANIC_NOMEM() - Called when malloc fails.

        You can call PANIC_NOMEM yourself, if detect an out of memory
        condition.  Or if you want to supply you own metadata, you can call

        void panic_nomem(const char* file, int line, const char *func)

        In future, the exit might not be unconditional.  The panic might
        conditionally do a longjmp to a place where you can try to carry on.
*/


void panic_nomem(const char* file, int line, const char *func)
/* Report an out-of-memory conditions and then exit the program */
{
        LogMeta meta = {
                file : file,
                line : line,
                func : func,
        };
        emergency_message("NOMEM", &meta, "Out of virtual memory");
        exit(1);
}

void *malloc_or_die(const char* file, int line, const char *func, size_t n)
{
        void *ret = malloc(n);
        if( ret )
                return ret;

        panic_nomem(file, line, func);
        return 0;
}

void *zalloc_or_die(const char* file, int line, const char *func, size_t n)
{
        void *ret = calloc(1, n);
        if( ret )
                return ret;

        panic_nomem(file, line, func);
        return 0;
}

#ifdef TEST

static int test_malloc(int n)
{
        // ------------------
        char *ttk = ZALLOC(n);
        CHK( ttk != NULL );
        ttk[10] = '5';

        CHK( n > 2048);
        CHK( ttk[0] == 0 );
        CHK( ttk[10] == '5' );
        
        for(int k = n - 1024; k < n; k++ ) 
                CHK( ttk[k] == 0 );

        free(ttk);


        // ------------------
        const char *test = "test";
        char *mlc = MALLOC(strlen(test) + 1);
        CHK( mlc );
        CHK( strcpy(mlc, test) == mlc );
        CHK( !strcmp(mlc, test) );

        free(mlc);

        PASS();
}


static int runtests_malloc_fail(void)
{
        struct rlimit mem_lim;

        mem_lim.rlim_cur = 128*1024*1024;
        mem_lim.rlim_max = mem_lim.rlim_cur;

        int err = setrlimit(RLIMIT_AS, &mem_lim);
        assert(!err);
        test_malloc(128 * 1024);
        test_malloc(mem_lim.rlim_cur);
        return 0;
}

#endif //TEST

// -- Panic ----------------------------

static PanicReturn *_panic_return;

Error *_panic_pop(PanicReturn *check)
{
        assert(_panic_return);
        assert(_panic_return == check);
        _panic_return = _panic_return->prev;
        return check->error;
}

static void throw_panic(Error *err)
{
        assert(_panic_return);
        _panic_return->error = err;
        longjmp(_panic_return->jmp_buf, -1);
}

int _panic_set_return(PanicReturn *ret)
{
        ret->prev  = _panic_return;
        ret->error = 0;
        _panic_return = ret;
        return 0;
}

static void death_panic(Error *e)
{       
        /*A glorious and righteous hack to hijack the most approriate logger.*/
        init_static_logger(dbg_log); // not strictly needed.
        Logger panic_log = *dbg_log;
        panic_log.zname = "PANIC!";

        log_error( &panic_log, e);
        exit(1);
}

void panic(Error *e)
{
        assert(e && e->type);
        if( _panic_return )
                throw_panic(e);
        else
                death_panic(e);
}


#ifdef TEST
static int chk_recursive_panic(int depth)
{
        PanicReturn ret;
        Error *err;
        static int catch_count = 0;

        assert( depth >= 0 && depth <= 10 );
        if(depth == 10)
                MPANIC("You've gone too far this time!");

        if(err = TRY(ret)) {
                catch_count++;
                CHK(chk_error(err, merror_type, 
                        "You've gone too far this time!"));
                CHK(depth);
                if(depth > 1)
                        panic(err);
                else {
                        error_destroy(err);
                        return -depth;
                }
                CHK(!"never");
        }

        CHK( chk_recursive_panic(depth+1) == -1 );
        NO_WORRIES(ret);
        
        CHK( depth == 0 );
        CHK( catch_count == 9);
        catch_count = 0;
        return 1;
}

static int test_recursive_panic()
{
        // do it twice to check the static catch_counted is handled right.
        CHK( chk_recursive_panic(0) );
        CHK( chk_recursive_panic(0) );
        PASS();
}

static int test_try_panic() 
{
        PanicReturn ret;
        Error *err;
        int failed = 0, succeeded = 0;
       
        // throw an error and catch it.
        if ( err = TRY(ret) ) {
                CHK( !_panic_return );
                CHK( err->type == merror_type );
                CHK( chk_error( err, merror_type, "never!" ) );
                error_destroy(err);
                failed = 1;
        } else {
                CHK( _panic_return );
                MPANIC("never!");
                assert(!"never");
                NO_WORRIES(ret);
        }
        CHK( !_panic_return );

        // don't throw an error and don't catch it.
        if ( err = TRY(ret) ) {
                panic(err);
        } else {
                succeeded = 1;
                NO_WORRIES(ret);
        }
        CHK( !_panic_return );

        CHK( failed && succeeded );
        PASS();
}
#endif //TEST

// -- Main -----------------------------


#ifdef TEST
int main() 
{
        test_errors();
        test_logging();
        test_debug_logger();
        LOG_F(null_log, "EEEK!  I'm invisible!  Don't look!");

        test_try_panic();
        test_recursive_panic();
        //MPANIC("You can't even fake this failure!");
        if(FAKE_FAIL) 
                runtests_malloc_fail();
        else 
                test_malloc(128 * 1024);
}
#endif //TEST
