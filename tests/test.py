#!/usr/bin/env python3

from subprocess import call, Popen, PIPE
import argparse
import fnmatch
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
        output = []

        # This iterates through stdout and stderr in this order.
        for stream in proc.communicate():
            for out in stream.splitlines():
                out = out.decode("utf-8")
                if len(out) >= 4 and out[:4] == "!DBG": continue
                output.append(out + "\n")

        return proc.returncode, output


class TestCase:
    WAITING = 1
    COMPILED = 2
    EXECUTED = 3
    SUCCESS = 4
    FAILED = 5
    IGNORED = 6

    def __init__(self, testCase):
        self.expectFile = testCase + ".expect"
        self.sourceFile = testCase
        self.outFile = testCase + ".out"
        self.testResult = None
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

        compileArgs = self.getProperty("compile", "{cc} -o {outfile} {infile} {driver}").format(**{
            # At the time of writing (May 2016), Clang refuses to emit near
            # jumps as GNU AS does. Therefore, the CC env variable is not
            # respected currently. Once these issues have been resolved, we can
            # also use Clang.
            "cc": "cc",
            # "cc": os.environ["CC"] if "CC" in os.environ else "cc",
            "outfile": self.outFile,
            "infile": self.sourceFile,
            "driver": self.driver
        })

        returnCode, output = Utils.execBuffered(compileArgs)

        if returnCode != 0:
            print("FAIL (Compile)")
            print("".join(output))
            self.status = TestCase.FAILED
            raise TestFailException()
        else:
            self.status = TestCase.COMPILED

    def run(self):
        self.compile()

        if self.status != TestCase.COMPILED: return

        runArgs = self.getProperty("run", "{outfile}").format(**{
            "outfile": self.outFile,
        })

        returnCode, testResult = Utils.execBuffered(runArgs)

        if returnCode != 0:
            print("FAIL (Exit Code %d)" % returnCode)
            print("".join(testResult))
            self.status = TestCase.FAILED
            raise TestFailException()
        elif int(self.getProperty("nooutput", "0")) == 1:
            print("OK")
            self.status = TestCase.SUCCESS
        else:
            self.status = TestCase.EXECUTED
            self.testResult = testResult

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

        if self.testResult != comparison:
            print("FAIL (Output)")
            for line in difflib.context_diff(comparison, self.testResult): sys.stdout.write(line)
            self.status = TestCase.FAILED
            raise TestFailException()
        else:
            print("OK")
            self.status = TestCase.SUCCESS

    def store(self):
        self.run()

        if self.status != TestCase.EXECUTED: return

        with open(self.expectFile, "w") as f:
            f.writelines(self.testResult)

        print("OK")
        self.status = TestCase.SUCCESS

    def printResult(self):
        self.run()

        if self.status != TestCase.EXECUTED: return

        print("OK")
        print("".join(self.testResult))

        self.status = TestCase.SUCCESS

if __name__ == "__main__":
    argparser = argparse.ArgumentParser(description='Test Script.')
    argparser.add_argument("--test", help="Run tests and compare output (default)", dest="action", action="store_const", const=TestCase.test, default=TestCase.test)
    argparser.add_argument("--run", help="Run tests and print ouput", dest="action", action="store_const", const=TestCase.printResult)
    argparser.add_argument("--store", help="Run tests and store ouput", dest="action", action="store_const", const=TestCase.store)
    argparser.add_argument("cases", nargs="*")
    args = argparser.parse_args()

    # In case there are no flags given or a flag is given but no file:
    # Use all *.c and *.s files in the cases directory.
    expectFiles = args.cases
    if len(expectFiles) == 0:
        for root, dirnames, filenames in os.walk("cases"):
            for filename in fnmatch.filter(filenames, "*.[cs]"):
                expectFiles.append(os.path.join(root, filename))

    # Remove .expect extension for the real filename
    testCases = [TestCase(file) for file in expectFiles]

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
        print(len(failed), "of", len(testCases) - len(ignored), "tests", "failed:", ", ".join(failed))
        returnCode = 1
    else:
        print(len(testCases) - len(ignored), "tests passed.")

    if len(ignored) > 0:
        print("Ignored", len(ignored), "tests:", ", ".join(ignored))

    sys.exit(returnCode)
