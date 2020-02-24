#!/usr/bin/env python3

from subprocess import call, Popen, PIPE
import argparse
import fnmatch
import re
import os
import sys
import difflib

class TestFailException(Exception): pass
class TestIgnoredException(Exception): pass

class Utils:

    def fetchProperties(fileName):
        """
        Fetch properties from a file.
        """

        properties = {}
        with open(fileName, "r") as f:
            while True:
                argLine = f.readline()
                if len(argLine) >= 4 and argLine[:3] == "//!":
                    split = argLine[3:].split("=", 1)
                    properties[split[0].lower().strip()] = split[1][:-1].strip()
                else: break

        return properties


    def execBuffered(command):
        proc = Popen(["/bin/sh", "-c", command], stdout=PIPE, stderr=PIPE)
        streams = proc.communicate();

        stdout_output = []
        for out in streams[0].splitlines():
            out = out.decode("utf-8")
            if len(out) >= 4 and out[:4] == "!DBG": continue
            stdout_output.append(out + "\n")

        stderr_output = []
        for out in streams[1].splitlines():
            out = out.decode("utf-8")
            if len(out) >= 4 and out[:4] == "!DBG": continue
            stderr_output.append(out + "\n")

        return proc.returncode, stdout_output, stderr_output


class TestCase:
    WAITING = 1
    COMPILED = 2
    EXECUTED = 3
    SUCCESS = 4
    FAILED = 5
    IGNORED = 6

    def __init__(self, testCase, verbose=0, debug=False):
        self.expectFile = testCase + ".expect"
        self.expectErrFile = testCase + ".expect_stderr"
        self.sourceFile = testCase
        self.objFile = testCase + ".o"
        self.outFile = testCase + ".out"
        self.testResult = None
        self.debug = debug
        self.verbose = verbose
        self.status = TestCase.WAITING

        self.properties = Utils.fetchProperties(self.sourceFile)

        self.driver = self.properties["driver"] if "driver" in self.properties else "test-driver.c"
        self.driverProperties = Utils.fetchProperties(self.driver)


    def getProperty(self, name, default):
        if name in self.properties: return self.properties[name]
        elif name in self.driverProperties: return self.driverProperties[name]
        else: return default


    def compile(self):
        if self.status != TestCase.WAITING: return

        # compiler taken from environment CC unless overwritten by explicit property

        substs = {
            "cc": self.getProperty("cc", os.environ["CC"] if "CC" in os.environ else "cc"),
            "ldflags": os.environ["LDFLAGS"] if "LDFLAGS" in os.environ else "-g -L/usr/lib64/llvm",
            "ldlibs": os.environ["LDLIBS"] if "LDLIBS" in os.environ else "-lLLVM-3.8",
            "ccflags": self.getProperty("ccflags", "-std=c99 -g"),
            "dbrew": "-I../include ../libdbrew.a",
            "outfile": self.outFile,
            "infile": self.sourceFile,
            "ofile": self.objFile,
            "driver": self.driver
        }

        # switch off PIE
        match = re.search('^(cc|gcc|clang)', substs["cc"])
        if match:
            substs["ccflags"] += " -fno-pie"
            if (match.group(1) == 'gcc') or (match.group(1) == 'cc'):
                s = Popen([substs["cc"], "-dumpversion"],stdout=PIPE).communicate();
                v = s[0].decode("utf-8")[0];
                if int(v) > 4:
                    substs["ccflags"] += " -no-pie"
        else:
            print("FAIL (Compiler " + substs["cc"] + " not supported)")
            self.status = TestCase.FAILED
            raise TestFailException()

        compileDef = "{cc} {ccflags} -o {outfile} {infile} {driver} {dbrew}"
        compileArgs = self.getProperty("compile", compileDef).format(**substs)
        if self.verbose > 0:
            print("\nCompiling:\n " + compileArgs)

        # ignore stderr
        returnCode, output, errout = Utils.execBuffered(compileArgs)
        if returnCode != 0:
            print("FAIL (Compile)")
            print("".join(output) + "".join(errout))
            self.status = TestCase.FAILED
            raise TestFailException()
        else:
            self.status = TestCase.COMPILED


    def run(self):
        self.compile()

        if self.status != TestCase.COMPILED: return

        substs = {
            "outfile": self.outFile,
            "args" : self.getProperty("args", "")
        }
        runDef = "./{outfile} {args}"
        runArgs = self.getProperty("run", runDef).format(**substs)

        if self.debug: runArgs += " --debug"

        if self.verbose > 0:
            print("\nRunning:\n " + runArgs)

        returnCode, outResult, errResult = Utils.execBuffered(runArgs)

        for expectFile, result in ((self.expectFile, outResult), (self.expectErrFile, errResult)):
            if os.path.isfile(expectFile + "_filter"):
                proc = Popen(["/bin/sh", "-c", expectFile + "_filter"], stdin=PIPE, stdout=PIPE)
                streams = proc.communicate(input="".join(result).encode())
                outResult = []
                for out in streams[0].splitlines():
                    out = out.decode("utf-8")
                    outResult.append(out + "\n")

        if returnCode != 0:
            print("FAIL (Exit Code %d)" % returnCode)
            print("".join(outResult) + "".join(errResult))
            self.status = TestCase.FAILED
            raise TestFailException()
        elif int(self.getProperty("nooutput", "0")) == 1:
            print("OK")
            self.status = TestCase.SUCCESS
        else:
            self.status = TestCase.EXECUTED
            self.outResult = outResult
            self.errResult = errResult


    def test(self):
        self.run()

        if self.status != TestCase.EXECUTED: return

        try:
            with open(self.expectFile) as f:
                comparison = f.readlines()
        except Exception:
            print("IGNORED")
            self.status = TestCase.IGNORED
            raise TestIgnoredException()

        if self.outResult != comparison:
            print("FAIL (stdout)")
            for line in difflib.unified_diff(comparison, self.outResult):
                sys.stdout.write(line)
            self.status = TestCase.FAILED
            raise TestFailException()

        try:
            with open(self.expectErrFile) as f:
                comparison = f.readlines()

            if self.errResult != comparison:
                print("FAIL (stderr)")
                for line in difflib.unified_diff(comparison, self.errResult):
                    sys.stdout.write(line)
                self.status = TestCase.FAILED
                raise TestFailException()
            else:
                print("OK (stdout, stderr)")
        except Exception:
            print("OK (stdout)")

        self.status = TestCase.SUCCESS



    def store(self):
        self.run()

        if self.status != TestCase.EXECUTED: return

        with open(self.expectFile, "w") as f:
            f.writelines(self.outResult)

        print("OK")
        self.status = TestCase.SUCCESS

    def printResult(self):
        self.run()

        if self.status != TestCase.EXECUTED: return

        print("OK")
        print("".join(self.outResult))

        self.status = TestCase.SUCCESS

if __name__ == "__main__":
    argparser = argparse.ArgumentParser(description="Test Script.")
    argparser.add_argument("--verbose", "-v", help="Be verbose about actions executing", action="count", default=0)
    argparser.add_argument("--test", help="Run tests and compare output (default)", dest="action", action="store_const", const=TestCase.test, default=TestCase.test)
    argparser.add_argument("--run", "-r", help="Run tests and print ouput", dest="action", action="store_const", const=TestCase.printResult)
    argparser.add_argument("--store", "-s", help="Run tests and store ouput", dest="action", action="store_const", const=TestCase.store)
    argparser.add_argument("--debug", "-d", help="Pass --debug to the test case", action="store_true")
    argparser.add_argument("cases", nargs="*")
    args = argparser.parse_args()

    # When no specific tests are given, use cases subdirectory
    paths = args.cases
    if len(paths) == 0:
        paths.append("cases")

    testFiles = []
    for path in paths:
        if os.path.isdir(path):
            # Search for all *.c, *.s and *.S files within given directory
            for root, dirnames, filenames in os.walk(path):
                for filename in fnmatch.filter(filenames, "*.[csS]"):
                    testFiles.append(os.path.join(root, filename))
        elif os.path.isfile(path):
            testFiles.append(path)
        else:
            print("Test '" + path + "' not existing; ignored")

    testCases = [TestCase(file, args.verbose, args.debug) for file in testFiles]

    failed = []
    ignored = []
    for testCase in testCases:
        sys.stdout.write((testCase.sourceFile + ":").ljust(60))
        try:
            args.action(testCase)
        except TestFailException:
            # We already printed "FAIL"
            failed.append(testCase.sourceFile)
        except TestIgnoredException:
            ignored.append(testCase.sourceFile)
        except Exception as e:
            print("FAIL (Exception)")
            print(e)
            failed.append(testCase.sourceFile)

    returnCode = 0
    if len(failed) > 0:
        print(len(failed), "of", len(testCases) - len(ignored), "tests", "failed:", " ".join(failed))
        returnCode = 1
    else:
        print(len(testCases) - len(ignored), "tests passed.")

    if len(ignored) > 0:
        print("Ignored", len(ignored), "tests:", " ".join(ignored))

    sys.exit(returnCode)
