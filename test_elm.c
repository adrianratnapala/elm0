#define _GNU_SOURCE

#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sys/resource.h>

#include "0unit.h"
#include "elm.h"

/* Set this to one to test some emergency-fail code. */
#ifndef FAKE_FAIL
#define FAKE_FAIL 0
#endif



// ----------------------------------------------------------------------------
static int chk_error( Error *err, const ErrorType *type,
                                  const char *zvalue )
{
        size_t size;
        char *buf;
        FILE *mstream;

        CHK( err );
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
        Error *e = ERROR("goodbye world!");

        CHK(chk_error(e, error_type, "goodbye world!"));
        CHK(!strcmp(e->meta.file, __FILE__));
        CHK(!strcmp(e->meta.func, __func__));
        CHK(e->meta.line == pre_line + 1);

        destroy_error(e);
        PASS();
}

static int test_error_format()
{
        int pre_line = __LINE__;
        Error *e[] = {
                ERROR("Happy unbirthday!"),
                ERROR("%04d every year.", 364),
                ERROR("%04d every %xth year.", 365, 4),
        };

        CHK(chk_error(e[0], error_type, "Happy unbirthday!"));
        CHK(chk_error(e[1], error_type, "0364 every year."));
        CHK(chk_error(e[2], error_type, "0365 every 4th year."));

        for(int k = 0; k < 3; k++) {
                CHK(!strcmp(e[k]->meta.file, __FILE__));
                CHK(!strcmp(e[k]->meta.func, __func__));
                CHK(e[k]->meta.line == pre_line + 2 + k);
                destroy_error(e[k]);
        }

        PASS();
}

static int test_system_error()
{
        char *xerror;
        PanicReturn ret;

        Error *eno = SYS_ERROR(EEXIST, "pretending");
        Error *enf = IO_ERROR("hello", ENOENT, "gone");

        asprintf(&xerror, "pretending: %s", strerror(EEXIST));
        CHK( chk_error(eno, sys_error_type, xerror) );
        destroy_error(eno);

        ret.error = NULL;;
        if ( TRY(ret) ) {
                CHK( chk_error( ret.error, sys_error_type, xerror ) );
                destroy_error(ret.error);
        } else {
                SYS_PANIC(EEXIST, "pretending");
                NO_WORRIES(ret);
        }
        CHK(ret.error);
        free(xerror);

        asprintf(&xerror, "gone (hello): %s", strerror(ENOENT));
        CHK( chk_error(enf, sys_error_type, xerror) );
        destroy_error(enf);


        ret.error = NULL;;
        if ( TRY(ret) ) {
                CHK( chk_error( ret.error, sys_error_type, xerror ) );
                destroy_error(ret.error);
        } else {
                IO_PANIC("hello", ENOENT, "gone");
                NO_WORRIES(ret);
        }
        CHK(ret.error);
        free(xerror);

        PASS();
}


// ----------------------------------------------------------------------------

static int test_logging()
{
        static const char *expected_text =
                "TEST: Hello Logs!\n"
                "TEST: Hello Logs #2!\n"
                "TEST: -1+4 == 8\n"
                "TEST: goodbye world!\n"
                ;

        size_t size;
        char *buf;

        FILE *mstream = open_memstream(&buf, &size);
        CHK( mstream != NULL );

        Logger *lg = new_logger("TEST", mstream, NULL);
        CHK(lg);

        Logger *nlg = new_logger("NULL_TEST", 0, NULL);
        CHK(nlg);

        CHK( LOG_F(nlg, "Hello Logs!") == 0 );
        CHK( LOG_F(lg, "Hello Logs!") == 18);
        CHK( size == 18 );
        CHK( !memcmp(buf, expected_text, size) );

        LOG_F(nlg, "Hello Logs #%d!", 2);
        LOG_F(lg, "Hello Logs #%d!", 2);
        CHK( size == 18 + 21 );
        CHK( !memcmp(buf, expected_text, size) );

        LOG_UNLESS(lg, 4+4 == 8);
        CHK( size == 18 + 21 );
        LOG_UNLESS(lg, -1+4 == 8);
        CHK( size == 18 + 21 + 16 );

        Error *e = ERROR_WITH(error, "goodbye world!");
        CHK( log_error(nlg, e) == 0 );
        CHK( log_error(lg, e) == 21 );
        CHK( size == 18 + 21 + 16 + 21 );
        CHK( !memcmp(buf, expected_text, size) );

        destroy_logger(lg);
        destroy_logger(nlg);
        fclose(mstream);
        free(buf);
        destroy_error(e);

        PASS();
}

static int test_debug_logger()
{
        size_t size;
        char *buf, *expect;

        FILE *mstream = open_memstream(&buf, &size);
        CHK( mstream != NULL );

        Logger *lg = new_logger("DTEST", mstream, "d");
        CHK(lg);

        char *text = "Eeek, a (pretend) software bug!";
        int line_p = __LINE__;
        LOG_F(lg, "Eeek, a (pretend) software bug!" );
        int n = asprintf(&expect, "DTEST (%s:%d in %s): %s\n",
                __FILE__, line_p + 1, __func__, text );
        CHK( n > 0 );

        CHK( strlen(expect) == size );
        CHK( !memcmp(expect, buf, size) );

        free(expect);
        destroy_logger(lg);
        fclose(mstream);
        free(buf);

        PASS();
}

static int test_log_hiding() {
        // this test always passes, can do interesting stuff.
        Logger *old_log = dbg_log;
        dbg_log = null_log;
        LOG_F(dbg_log, "Look at mee! I'm invisible!");
        dbg_log = old_log;
        if(FAKE_FAIL)
                LOG_F(dbg_log, "Visible debug.");

        PASS();
}

// ----------------------------------------------------------------------------

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

// ----------------------------------------------------------------------------

static int chk_recursive_panic(int depth)
{
        PanicReturn ret;
        Error *err;
        static int catch_count = 0;

        assert( depth >= 0 && depth <= 10 );
        if(depth == 10)
                PANIC("You've gone too far this time!");

        if(err = TRY(ret)) {
                catch_count++;
                CHK(chk_error(err, error_type,
                        "You've gone too far this time!"));
                CHK(depth);
                if(depth > 1)
                        panic(err);
                else {
                        destroy_error(err);
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
                CHK( !panic_is_caught() );
                CHK( err->type == error_type );
                CHK( chk_error( err, error_type,
                        "not in 07 years!" ) );
                destroy_error(err);
                failed = 1;
        } else {
                CHK( panic_is_caught() );
                PANIC("not in %02d %s!", 7, "years");
                assert(!"never");
                NO_WORRIES(ret);
        }
        CHK( !panic_is_caught() );

        // don't throw an error and don't catch it.
        if ( err = TRY(ret) ) {
                panic(err);
        } else {
                succeeded = 1;
                NO_WORRIES(ret);
        }
        CHK( !panic_is_caught() );

        CHK( failed && succeeded );
        PASS();
}


// -- Main -----------------------------

int main(int argc, const char **argv)
{

        test_errors();
        test_error_format();
        test_system_error();

        test_logging();
        test_debug_logger();
        LOG_F(null_log, "EEEK!  I'm invisible!  Don't look!");

        test_log_hiding();

        test_try_panic();
        test_recursive_panic();
        if( argc > 1 && !strcmp(argv[1], "--panic") )
                PANIC("The slithy toves!"); //FIX
        if(FAKE_FAIL)
                runtests_malloc_fail();
        else
                test_malloc(128 * 1024);
}

