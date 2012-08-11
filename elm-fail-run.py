#!/usr/bin/python3

from n0run import *

class Elm_Fail_Runner(Fail_Runner) :
        err_matchers = compile_matchers ([
                ('NOMEM', br'^NOMEM \(in test_elm.c:(?P<n>test_malloc+)'),
                ('LOGFAILED', br'^LOGFAILED \(in test_elm.c:(?P<n>test_logging)\)'),
                ('LOGFAILED', br'^LOGFAILED \(in test_elm.c:(?P<n>test_debug_logger)\)'),
                ('LOGFAILED', br'^LOGFAILED \(in test_elm.c:(?P<n>test_log_hiding+)\):'+
                              br' Visible debug.'),
                ('DBG', br'^DBG \(test_elm.c:[0-9]+ in (?P<n>test_log_hiding+)\):'+
                        br' Visible debug.'),

        ])

        def __init__(s, command, source) :
                Fail_Runner.__init__(s, [command], source ) ;

        def check_output(s) :
                for err in Fail_Runner.check_output(s) :
                        yield err

                import os
                if s.data.errno != os.errno.ENOMEM :
                        yield Fail("test program failed but not with ENOMEM",
                                s.data.errno, s.data.command)

class Panic_Runner(Fail_Runner) :
        def __init__(s, command, source) :
                Fail_Runner.__init__(s, [command, '--panic'], source ) ;

        def check_output(s) :
                for err in Fail_Runner.check_output(s) :
                        yield err

                if s.data.errno != 255 :
                        yield Fail("test program failed but not with 255",
                                s.data.errno, s.data.command)


class Elm_Panic_Runner(Panic_Runner) :
        # FIX: the different errors do not have very consistent formats
        err_matchers = Elm_Fail_Runner.err_matchers + compile_matchers ([
                ('PANIC', br'^PANIC! \(test_elm.c:[0-9]+ in (?P<n>main)\):'+
                          br' The slithy toves!'),
                ])



class Elm_Fail_Panic_Runner(Panic_Runner) :
        err_matchers = Elm_Fail_Runner.err_matchers + compile_matchers ([
                ('LOGFAILED', br'^LOGFAILED \(in test_elm.c:(?P<n>main)\):'+
                              br' Error logging error.'),
               ])




if __name__ == "__main__":
        from sys import stdout, stderr

        print('elm-test with panic ...')
        trunner  = Elm_Panic_Runner('./elm-test', 'elm.c')
        tresults = run_main(trunner)
        results = tresults

        results.check_found( results.run - {'main'} )
        results.check_run( results.src - {'test_malloc'} )
        results.check_matched('passed', results.run - {'main'} )
        results.check_matched('NOMEM', set() )
        results.check_matched('PANIC', {'main',});
        stderr.flush()
        stdout.flush()

        print('elm-fail with panic ...')
        prunner  = Elm_Fail_Panic_Runner('./elm-fail', 'elm.c')
        presults = run_main(prunner)
        results = presults

        results.check_found( results.run - {'main'} )
        results.check_run( results.src - {'test_malloc'} )
        results.check_matched('passed', results.run - {'test_logging', 'main'} )
        results.check_matched('NOMEM', set() )
        results.check_matched('LOGFAILED', {'test_logging',
                                            'test_debug_logger',
                                            'test_log_hiding',
                                            'main'})
        stderr.flush()
        stdout.flush()

        print('elm-fail with out panic ...')
        runner  = Elm_Fail_Runner('./elm-fail', 'elm.c')
        results = run_main(runner)

        results.check_found( results.run )
        results.check_run( results.src )
        results.check_matched('passed', results.run - {'test_logging'} )
        results.check_matched('NOMEM', {'test_malloc'} )
        results.check_matched('LOGFAILED', {'test_logging',
                                            'test_log_hiding',
                                            'test_debug_logger'})

        stderr.flush()
        stdout.flush()

        sys.exit(results.errno or presults.errno or tresults.errno)
