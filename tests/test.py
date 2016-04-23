#!/usr/bin/env python3

from subprocess import call, Popen, PIPE
import fnmatch
import os
import sys
import difflib

def fetchCompilerArgs(file, outFile, testCase):
    with open(file, "r") as f:
        argLine = f.readline()
        if argLine[:12] == "//  COMPILE:":
            return (argLine[12:] % (outFile, testCase)).split()
    return None

def compile(testCase):
    print("==TEST Compiling", testCase)
    outFile = testCase + ".out"

    args = fetchCompilerArgs(testCase, outFile, testCase)
    if not args: args = fetchCompilerArgs("test-parser.c", outFile, testCase)
    if not args: args = ["cc", "-o", outFile, testCase, "test-parser.c"]

    exitCode = call(args)
    if exitCode != 0:
        print("==TEST ERROR while compiling, skipping")
        raise Exception("Cannot compile")
    return outFile

def runTestCase(testCase):
    outFile = compile(testCase)
    print("==TEST Running", testCase)
    proc = Popen("./" + outFile, stdout=PIPE, stderr=PIPE)
    testResult = []

    # This iterates through stdout and stderr in this order.
    for stream in proc.communicate():
        for out in stream.splitlines():
            # Filter out debug output and remove trailing whitespaces for git
            out = out.decode("utf-8")
            if len(out) >= 4 and out[:4] == "!DBG": continue
            testResult.append(out + "\n")

    if proc.returncode != 0:
        print("==TEST CRASH", testCase, "return code", proc.returncode)
        raise Exception("Test exited with non-zero code")
    return testResult

def store(testCase):
    with open(testCase + ".expect", "w") as f:
        f.writelines(runTestCase(testCase))
        print("==TEST Stored", testCase)

def runAndPrint(testCase):
    print("".join(runTestCase(testCase)))

def test(testCase):
    with open(testCase + ".expect") as f:
        comparison = f.readlines()
    output = runTestCase(testCase)

    if output != comparison:
        print("==TEST FAILED  --  DIFF BELOW")
        for line in difflib.context_diff(comparison, output): sys.stdout.write(line)
        raise Exception("Output incorrect")

actions = [
    ("--test", test),
    ("--run", runAndPrint),
    ("--store", store),
    ("--compile-only", compile)
]

if __name__ == "__main__":
    testFunction = test
    files = None

    if len(sys.argv) > 1:
        if sys.argv[1] == "--help":
            print(sys.argv[0], "[--store,--compile-only]", "[.expect files]")
            print("If no files are specified, all files in the cases folder are used.")
            sys.exit(0)

        for flag, function in actions:
            if sys.argv[1] == flag:
                testFunction = function
                files = sys.argv[2:]
                break

        if files is None:
            files = sys.argv[1:]

    # In case there are no flags given or a flag is given but no file:
    # Use all *.expect files in the cases directory.
    if not files or len(files) == 0:
        files = []
        for root, dirnames, filenames in os.walk("cases"):
            for filename in fnmatch.filter(filenames, "*.expect"):
                files.append(os.path.join(root, filename))

    # Remove .expect extension for the real filename
    files = [file[:-7] for file in files]

    failed = []
    for testCase in files:
        try:
            testFunction(testCase)
            print("==TEST PASSED")
        except Exception as e:
            print("==TEST FAILED", e)
            failed.append(testCase)
    if len(failed) > 0:
        print("==TEST RESULT:", len(failed), "FAILED:", ", ".join(failed))
        sys.exit(1)
    else:
        print("==TESTS PASSED")
