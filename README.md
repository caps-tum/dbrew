# DBrew - a Library for Dynamic Binary Rewriting

[![Build Status](https://travis-ci.org/caps-tum/dbrew.svg?branch=master)](https://travis-ci.org/caps-tum/dbrew)

---

**REMARK:** This repository no longer contains an LLVM-based binary optimizer, which has been moved [here](https://github.com/aengelke/binopt). The name DBrew only refers to the tracing binary rewriter.

---

This library allows application-controlled, explicit rewriting of functions
at runtime on the binary level. The rewritten functions can be used instead
of the original functions as drop-in replacements as they use the exact same
function signature.

Warning: DBrew is in a very early state with lots of features missing.


## Why is this useful?

Performance improvement
* specialization: if function parameters are known at runtime
* optimization of common case by reordering and inline, e.g. when
  profiling/usage data is available

Change functionality
* redirect function calls, memory accesses
* replace instructions
* insert instrumentation for profiling


# API

DBrew provides best-effort and robustness. The API is designed in a way
that rewriting may fail; however, it always can return the original
function as fall-back. Thus, there is no need to strive for complete
coverage of binary code.

Rewriting configurations heavily rely on the C calling convention / ABI
(Application Binary Interface) of the target architecture. This way,
DBrew supports rewriting of compiled code from most languages (C, C++, ...)
and makes the DBrew API itself architecture independent.



## Supported Architectures

For now just one:
* amd64 (that is, 64bit x86)


## Example

To generate a spezialised version of strcmp which only can compare a given
string with a fixed string, which should be faster than the generic strcmp:

    strcmpHW = dbrew_rewrite(strcmp, str, "Hello World!");

Use the returned function pointer to run the generated special comparison.
The second parameter actually is not used in the rewritten code. However,
if rewriting failed for whatever reason, the original strcmp may be returned
(depending on configuration). So, it is better to use valid parameters.

FIXME: This short example currently does not work because DBrew does not yet
(1) catch/ignore the dynamic-linker part of 1st-time invocations of shared
library functions and (2) specialize on (mixed) knowledge (known/unknown) about
SSE/AVX registers contents, which the strcmp version in your glibc may use.


## Publications

* Josef Weidendorfer and Jens Breitbart. The Case for Binary Rewriting at Runtime for Efficient Implementation of High-Level Programming Models in HPC. In *Proceedings of the 21st int. Workshop on High-Level Parallel Programming Models and Supportive Environments (HIPS 2016)*. Chicago, US, 2016. ([PDF of pre-print version](https://github.com/lrr-tum/dbrew/raw/master/docs/pubs/preprint-hips16.pdf))

* Alexis Engelke and Josef Weidendorfer. Using LLVM for Optimized Light-Weight Binary Re-Writing at Runtime. In Proceedings of the 22st int. Workshop on High-Level Parallel Programming Models and Supportive Environments (HIPS 2017). Orlando, US, 2017 ([PDF of pre-print version](http://wwwi10.lrr.in.tum.de/~weidendo/pubs/hips17.pdf)) *Note: the LLVM-based binary rewriter has been moved [here](https://github.com/aengelke/binopt).*

## License

LGPLv2.1+


## Remarks for Development

* All features should have a test case, see tests/ subdirectory. Running the tests is done with "make test". Using travis on github, pushed commits automatically trigger compile and test.

* Make heavy use of assertions. Any unsupported cases of partly implemented features should fail hard using "assert(0);", without trying to be smart on parsing input.

* C is tricky. For better quality, (1) compilations fail on warnings, with a lot of warnings switched on, (2) travis compiles and runs the tests with a variety of compilers and compiler versions, and (3) use the linter "clang-tidy": "make tidy"
