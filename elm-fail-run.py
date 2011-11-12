#!/usr/bin/python3

from n0run import *

class Fail_Runner(Runner) :
        matchers = match_passed + match_failed

        def check_output(s) :
                if s.data.errno == 0 :
                        s.errno = -1
                        yield Fail("test program should have failed "
                                   "but did not", -1, s.data.command)

        def scan_output(s) :
                out, oe = scan_output(s.lines, s.matchers)
                err, ee = scan_output(s.data.err.split(b'\n'), s.err_matchers)

                # FIX: check for duplicates.

                out.update(err)
                return out, oe + ee;


class Elm_Fail_Runner(Fail_Runner) :
        err_matchers = compile_matchers ([
                ('NOMEM', br'^NOMEM \(in elm.c:(?P<n>test_malloc+)'),
                ('LOGFAILED', br'^LOGFAILED \(in elm.c:(?P<n>test_logging)\)'),
                ('LOGFAILED', br'^LOGFAILED \(in elm.c:(?P<n>test_debug_logger)\)'),
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


class Elm_Panic_Runner(Fail_Runner) :
        def __init__(s, command, source) :
                Fail_Runner.__init__(s, [command, '--panic'], source ) ;

        def check_output(s) :
                for err in Fail_Runner.check_output(s) :
                        yield err

                if s.data.errno != 255 :
                        yield Fail("test program failed but not with 255",
                                s.data.errno, s.data.command)


class Elm_Fail_Panic_Runner(Elm_Panic_Runner) :
        err_matchers = Elm_Fail_Runner.err_matchers + compile_matchers ([
                ('LOGFAILED', br'^LOGFAILED \(in elm.c:(?P<n>main)\):'+
                              br' Error logging error.'),
                ])




if __name__ == "__main__":
        from sys import stdout, stderr

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
        results.check_matched('LOGFAILED', {'test_logging','test_debug_logger'})

        stderr.flush()
        stdout.flush()

        sys.exit(results.errno or presults.errno)
