#!/usr/bin/python

import sys

def die(msg, x, errno=1) :
        sys.stderr.write("{}: {}\n".format(msg,x))
        exit(errno)

class Fail( Exception ) : 
        def __init__( s, msg, errno, command ) :
                Exception.__init__( s, msg )
                s.args = (errno, msg, command)

class NoMatch(Exception) : pass 

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

                [static] int test_SOME_NAME([void])

        where items in are optional.
        """
        import re

        if not def_re :
                storage_class = r"(static\s+)?"
                type_and_name = r"int\s+(?P<n>test_[_a-zA-Z0-9]*)";
                args=r"\((void)?\)";
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

def lines_without_ansi(command) :
        from subprocess import Popen, PIPE
        import re

        ansi = re.compile(b"\033\[?.*?[@-~]")
        endl = re.compile(b"\r?\n")
        po = Popen(command, stdout=PIPE)
        with po:
                for line in po.stdout :
                       yield endl.sub( b'', ansi.sub(b'', line ) )

        errno = po.wait()
        if errno :
                raise Fail("test program failed", errno, command) 

def compile_matchers(sources) :
        def showok(name, line) :
                from sys import stdout
                stdout.buffer.write(b'\033[32m\033[1mOK:\033[0m '+line+b'\n')
         
        def compile_one(s) :
                from re import compile
                if isinstance(s, bytes) :
                        return compile(s), set(), showok

                if type(s) != 'tuple' or len(s) != 2 :
                        raise Exception('bad match spec.')
                
                b, f = s
                if not isinstance(b, bytes) :
                        raise Exception('regex must be bytes.')

                return re.compile(b), set(), f or showok

        return list(map(compile_one, sources))

match_passed = compile_matchers([b'^passed: (?P<n>test.*)'])

def run_test(command, matchers = match_passed ) :
        for line in lines_without_ansi(command) :
                for (re, cont, act) in matchers :
                        m = re.match(line)
                        if m : break
                else :
                        # FIX: test this
                        raise NoMatch("unmatched output line", line)
                
                name = m.group('n');
                cont.add(name)
                act(name, line)


for basename in sys.argv[1:] :
        try : all = scan_source(basename + '.c')
        except IOError as x :
                die("Error reading source file", x, -x.args[0])

        try: ok = run_test('./' + basename)
        except OSError as x :
                die("Error running test", x, x.args[0])
        except IOError as x :
                die("Error reading test output", x, x.args[0])
        except Fail as x :
                die("Unexpected test return code", x, +x.args[0])
        except NoMatch as x :
                die("Unexpected test output", x, +x.args[0])

