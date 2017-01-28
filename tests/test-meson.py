#!/usr/bin/env python3

from subprocess import STDOUT, check_output, CalledProcessError
from os import path
import sys

if len(sys.argv) <= 2:
    print("usage: %s [testcase] [args...]" % sys.argv[0])
    exit(1)

testCase = sys.argv[1]
expectFile = testCase + ".expect"
filterFile = testCase + ".expect_filter"

try:
    result = check_output(sys.argv[2:], stderr=STDOUT, universal_newlines=True)
    if path.isfile(filterFile):
        result = check_output(["/bin/sh", "-c", filterFile], input=result, universal_newlines=True)
except CalledProcessError as e:
    print("== EXIT CODE %d ==" % e.returncode)
    print(result)
    exit(e.returncode)

if not path.isfile(expectFile):
    print("== NO OUTPUT CHECK ==")
    print(result)
    exit(0)

with open(expectFile) as f:
    comparison = f.readlines()

result = [line + "\n" for line in result.splitlines()]

if result != comparison:
    print("== FAIL (output) ==")
    import difflib
    for line in difflib.unified_diff(comparison, result):
        sys.stdout.write(line)
    exit(1)
