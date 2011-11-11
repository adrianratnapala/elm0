#!/usr/bin/python3

from n0run import *

class Fail_Runner(Runner) :
        matchers = match_passed + match_failed  # FIX: cargo cult

        # FIX: should be "in elm"
        err_matchers = compile_matchers ([
                ('NOMEM', br'^NOMEM \(inelm.c:(?P<n>test_malloc+)'),
                ('LOGFAILED', br'^LOGFAILED \(inelm.c:(?P<n>test_logging)\)'),
                ('LOGFAILED', br'^LOGFAILED \(inelm.c:(?P<n>test_debug_logger)\)'),
        ])


        def scan_output(s) : 
                out, oe = scan_output(s.lines, s.matchers)
                err, ee = scan_output(s.data.err.split(b'\n'), s.err_matchers)

                # FIX: check for duplicates.

                out.update(err)
                return out, oe + ee;

        def check_output(s) :
                # FIX: this should be os.errno.ENOMEM
                if s.data.errno == 1 :
                        return 
                if s.data.errno == 0 :
                        raise Fail("test program should have failed "
                                   "but did not", -1, s.data.command)
                raise Fail("test program failed but not with ENOMEM", 
                                errno, s.data.command) 

if __name__ == "__main__":
        results = run_main(Fail_Runner)

        results.check_found( results.run )
        results.check_run( results.src )
        results.check_matched('passed', results.run - {'test_logging'} )
        results.check_matched('NOMEM', {'test_malloc'} )
        results.check_matched('LOGFAILED', {'test_logging','test_debug_logger'})
                                            

        sys.exit(results.errno)


