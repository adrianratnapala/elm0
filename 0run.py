#!/usr/bin/python

import sys

# errors -----------------------------------------------------

def warn(msg, x = None , errno=1, txt='warning') :
        ERROR="\033[31m\033[1m0run {}: \033[0m".format(txt)
        if x : sys.stderr.write(ERROR+"{}: {}\n".format(msg, x))
        else : sys.stderr.write(ERROR+"{}\n".format(msg))
        return errno

def die(msg, x = None , errno=1) :
        warn(msg, x, errno, 'error')
        sys.exit(errno)

class Error(Exception) : pass
class Fail( Error ) : 
        def __init__( s, msg, errno, command ) :
                Error.__init__( s, msg )
                s.errno = errno
                s.msg = msg
                s.command = command
                s.args = (errno, msg, command)

class NoMatch(Error) : pass 
class DuplicateTest(Error) : pass 

# util -------------------------------------------------------
def lines_without_ansi(po) :
        import re

        ansi = re.compile(b"\033\[?.*?[@-~]")
        endl = re.compile(b"\r?\n")
        for line in po :
               yield endl.sub( b'', ansi.sub(b'', line ) )

# data gathering ---------------------------------------------

def Maker(base=object) : # turns a function into a class with only a constructor
        def dec(init) :
                class cls() :
                        __doc__=init.__doc__
                        __init__ = init
                cls.__name__=init.__name__
                return cls
        return dec

@Maker()
def RunData(s, command, source) :
        from subprocess import Popen, PIPE
        s.command = command
        s.source = source
        popen = Popen(s.command, stdout=PIPE, stderr=PIPE)

        s.out, s.err = popen.communicate()
        s.errno = popen.wait()

# source -----------------------------------------------------
def scan_source(filename, def_re = None, cb = (lambda l,m : None) ) :
        """
        Scans a named source file for lines matching the regex /def_re/.  This
        regex sould include a match object with name "n".  The set of all
        matches a returned, identified by the "n" match.  For each match, an
        optional callback function /cb(line,match)/, where line is the entire
        line of text (FIX?: a python string) and "m" the match object resulting
        from the regex.

        If the regex "def_re" is omitted, a default is used that matches lines
        of roughly the form

                [static] int test_SOME_NAME([signature])

        where items in are optional.
        """
        import re

        if not def_re :
                storage_class = br"(static\s+)?"
                type_and_name = br"int\s+(?P<n>test_[_a-zA-Z0-9]*)";
                args=br"\(.*\)";
                def_re = re.compile(b"\s*" + storage_class + 
                                             type_and_name + b"\s*" + 
                                             args );

        tests = set()
        with open(filename, "rb") as  f:
                for line in f:
                        m = def_re.match(line)
                        if not m : continue
                        cb(line, m)
                        tests.add( m.group('n').strip().decode('utf-8')  )
        return tests

# output scanning --------------------------------------------

def compile_matchers(sources) :
        def showok(name, line) :
                from sys import stdout
                stdout.buffer.write(b'\033[32m\033[1mOK:\033[0m '+line+b'\n')
         
        def compile_one(s) :
                from re import compile
                if type(s) != tuple :
                        raise Exception('match spec must be a tuple of' + 
                                        'the form  (name, byte-regex, [act])')

                n, b, *f = s
                f = f[0] if f else showok

                if not isinstance(b, bytes) :
                        raise Exception('regex must be bytes.')

                return n, compile(b), f

        return list(map(compile_one, sources))

match_passed = compile_matchers([ ('passed', b'^passed: (?P<n>test\S*)') ])
match_failed = compile_matchers([ ('FAILED', b'^FAILED: (?P<n>test\S*)') ])

def scan_output(po, matchers = match_passed ) :
        out = {}
        err = []
        # m=match name, s=set of names per mname, r=regex,  a = callback ("act")
        msra = [ (m, out.setdefault(m, set()), r, a) for m, r, a in matchers]

        for line in lines_without_ansi(po) :
                if not line.strip() : continue
                for (mname, s,re,act) in msra :
                        m = re.match(line)
                        if m : break
                else :  
                        err.append(NoMatch("unmatched output line", line))
                        continue

                name = m.group('n').decode('utf-8');
                if name in out :
                        err.append( DuplicateTest(
                                "Test '{}' found twice!".format( name)),
                                (name, mname) )

                s.add(name)
                act(name, line)
      
        return out, err

# runner -----------------------------------------------------

class Runner :
        matchers = match_passed
        def __init__(s, command, source) :
                s.data = RunData(command, source)
                s.check_output()
                s.lines = s.data.out.split(b'\n')

        # any one of these might be overriden
        def scan_source(s) : return scan_source(s.data.source)
        def scan_output(s) : return scan_output(s.lines, s.matchers)
        def check_output(s):
                if s.data.err != b'' :
                        raise Fail("test program wrote to stderr", 
                                        -1, s.data.command)
                if s.data.errno :
                        raise Fail("test program failed", 
                                s.data.errno, s.data.command) 


                
# CLI --------------------------------------------------------

def parse_argv():
        if len(sys.argv) < 2 : 
                die("{} requires at least a test name as an argument".format(
                        sys.argv[0]))
        if len(sys.argv) > 3 :
                die("{} takes at most two arguments (test, source)".format(
                        sys.argv[0]))

        test_command = sys.argv[1]
        if len(sys.argv) == 3 :
                source_file = sys.argv[2]
        else :
                if test_command[-5:] != '-test' :
                        die("No source file was given and the test " + 
                            "command '{}' is not of the standard form.".format(
                                test_command));
                source_file = test_command[:-5] + '.c'
        return test_command, source_file

def cli_scan_source(r) :
        try : return r.scan_source()
        except IOError as x :
                die("Error reading source file", x, -x.args[0])

def cli_scan_output(r) :
        try:  # FIX: test this!
                out, err = r.scan_output()
                if not err : return out
                for x in err : warn(x)
                die("Errors found scanning output")
        except OSError as x :
                die("Error running test", x, x.args[0])
        except IOError as x :
                die("Error reading test output", x, x.args[0])


def run_main(RunnerClass = Runner) :
        r = RunnerClass(*parse_argv())
        source          = cli_scan_source(r)
        run_results     = cli_scan_output(r)
        return Results(source, run_results)


class Results() :
        def __init__(s, source_tests, run_results) :
                from functools import reduce
                import operator 
                s.src = source_tests
                s.res = run_results
                s.run = reduce(operator.or_, run_results.values())
                s.errno = 0
       
        def matched(s, m) :
                "Returns the set of tests run with output matched by /m/"
                return s.res[m]

        def check_run(s, tset) :
                rem = tset - s.run
                if rem: s.errno = warn("Tests {} did not run.".format(rem))
                
        def check_found(s, tset) :
                rem = tset - s.src
                if rem: s.errno = warn("Tests {} were not found in the source.".
                                        format(rem))
               
        def check_matched(s, m, tset) :
                rem = tset - s.matched(m)
                if rem: s.errno = warn("Tests {} did not match '{}'.".format(rem, m))

                
if __name__ == "__vain__":
        results = run_main()

        results.check_found( results.run )
        results.check_run( results.src )
        results.check_matched( 'passed', results.run )
        
        sys.exit(results.errno)

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

                # some keys will be in both err and out.
                # we don't check for this, because it turns out to be normal.

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

        no_pass = {'test_logging', 'test_malloc', 'test_debug_logger'}
        results.check_found( results.run )
        results.check_run( results.src )
        results.check_matched( 'passed', results.run - no_pass )
        
        sys.exit(results.errno)


