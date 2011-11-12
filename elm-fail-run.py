#!/usr/bin/python3

from n0run import *

class Fail_Runner(Runner) :
        # FIX: this is cargo-cult programming.
        # I originally thought that making "matchers" a class attributes was a
        # clever way of having an extensible matchers list, but still having a
        # default.  But it is of no use to anyone who overrides scan_output,
        # since they must name their matchers explicitly anyway.
        matchers = match_passed + match_failed

        # FIX: should be "in elm"
        err_matchers = compile_matchers ([
                ('NOMEM', br'^NOMEM \(in elm.c:(?P<n>test_malloc+)'),
                ('LOGFAILED', br'^LOGFAILED \(in elm.c:(?P<n>test_logging)\)'),
                ('LOGFAILED', br'^LOGFAILED \(in elm.c:(?P<n>test_debug_logger)\)'),
        ])

        panic_matchers = compile_matchers ([
                ('LOGFAILED', br'^LOGFAILED \(in elm.c:(?P<n>main)\):'+
                              br' Error logging error.'),
        ])



        def __init__(s, panic, command, source) :
                s.panic = panic
                command = [command, '--panic'] if panic  else [command]
                Runner.__init__(s, command, source ) ;

        def scan_output(s) :
                errm = s.err_matchers
                if s.panic : errm += s.panic_matchers

                out, oe = scan_output(s.lines, s.matchers)
                err, ee = scan_output(s.data.err.split(b'\n'), errm)

                # FIX: check for duplicates.

                out.update(err)
                return out, oe + ee;

        def check_output(s) :
                import os
                if s.data.errno == 0 :
                        s.errno = -1
                        yield Fail("test program should have failed "
                                   "but did not", -1, s.data.command)

                if s.panic :
                        if s.data.errno == 255 : return
                        error = "test program failed but not with 255"
                else :
                        if s.data.errno == os.errno.ENOMEM : return
                        error = "test program failed but not with ENOMEM"

                yield Fail(error, s.data.errno, s.data.command)

if __name__ == "__main__":
        from sys import stdout, stderr
        print('elm-fail with panic ...')
        prunner  = Fail_Runner(True, './elm-fail', 'elm.c')
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
        prunner  = Fail_Runner(True, './elm-fail', 'elm.c')
        runner  = Fail_Runner(False, './elm-fail', 'elm.c')
        results = run_main(runner)

        results.check_found( results.run )
        results.check_run( results.src )
        results.check_matched('passed', results.run - {'test_logging'} )
        results.check_matched('NOMEM', {'test_malloc'} )
        results.check_matched('LOGFAILED', {'test_logging','test_debug_logger'})

        stderr.flush()
        stdout.flush()

        sys.exit(results.errno or presults.errno)
