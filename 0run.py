#!/usr/bin/python

import sys

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
                storage_class = r"(static\s+)?"
                type_and_name = r"int\s+(?P<n>test_[_a-zA-Z0-9]*)";
                args=r"\(.*\)";
                def_re = re.compile("\s*" + storage_class + 
                                            type_and_name + "\s*" + 
                                            args );

        tests = set()
        with open(filename) as  f:
                for line in f:
                        m = def_re.match(line)
                        if not m : continue
                        cb(line, m)
                        tests.add( m.group('n').strip()  )
        return tests

def lines_without_ansi(po) :
        import re

        ansi = re.compile(b"\033\[?.*?[@-~]")
        endl = re.compile(b"\r?\n")
        for line in po.stdout :
               yield endl.sub( b'', ansi.sub(b'', line ) )


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
        for line in lines_without_ansi(po) :
                for (mname,re,act) in matchers :
                        m = re.match(line)
                        if m : break
                else :
                        raise NoMatch("unmatched output line", line)

                name = m.group('n').decode('utf-8');
                if name in out :
                        raise DuplicateTest("Test '{}' found twice!".format(
                                                name))

                out[name] = mname
                act(name, line)

      
        return out

class Runner :
        matchers = match_passed
        def __init__(s, command, source) :
                from subprocess import Popen, PIPE
                s.command = command
                s.source = source
                s.popen = Popen(s.command, stdout=PIPE)

        # any one of these might be overriden
        def scan_source(s) : return scan_source(s.source)
        def scan_output(s) : 
                return scan_output(s.popen, s.matchers)
        def run(s) : 
                out = s.scan_output()
                errno = s.popen.wait()
                if errno :
                        raise Fail("test program failed", errno, command) 
                return out 

                

def run_main(RunnerClass = Runner) :
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
                
        r = RunnerClass(test_command, source_file)

        try : source = r.scan_source()
        except IOError as x :
                die("Error reading source file", x, -x.args[0])

        try: run_results = r.run()
        except OSError as x :
                die("Error running test", x, x.args[0])
        except IOError as x :
                die("Error reading test output", x, x.args[0])
        except Error as x :
                die(x)

        return Results(source, run_results)

class Results() :
        def __init__(s, source_tests, run_results) :
                s.src = source_tests
                s.res = run_results
                s.run = set(run_results.keys())
                s.errno = 0
       
        def matched(s, m) :
                "Returns the set of tests run with output matched by /m/"
                return set(k for k, v in s.res.items() if v == m)

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
        matchers = match_passed + match_failed

        def run(s) :
                out = s.scan_output()
                errno = s.popen.wait()
                        
                # FIX: this should be os.errno.ENOMEM
                if errno == 1 :
                        return out
                if errno == 0 :
                        raise Fail("test program should have failed "
                                   "but did not", -1, s.command)
                raise Fail("test program failed but not with ENOMEM", 
                                errno, s.command) 

if __name__ == "__main__":
        results = run_main(Fail_Runner)

        xfail = {'test_logging'}
        results.check_found( results.run )
        results.check_run( results.src )
        results.check_matched( 'passed', results.run - xfail )
        
        sys.exit(results.errno)


