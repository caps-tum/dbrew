
python3 = find_program('python3')

# Structure: [Test-Case, additional files (e.g. test driver), run arguments, additional compile flags]
#   Only the first two parts are required, run arguments and compile flags can be omitted.
tests = [
    ['cases/decode/avx.s', files('test-driver-decode.c')],
    ['cases/decode/group-f7.s', files('test-driver-decode.c')],
    ['cases/decode/it-adc.s', files('test-driver-decode.c')],
    ['cases/decode/it-add.s', files('test-driver-decode.c')],
    ['cases/decode/it-and-imm.s', files('test-driver-decode.c')],
    ['cases/decode/it-and.s', files('test-driver-decode.c')],
    ['cases/decode/it-bsf.s', files('test-driver-decode.c')],
    ['cases/decode/it-cltq.s', files('test-driver-decode.c')],
    ['cases/decode/it-cmovcc.s', files('test-driver-decode.c')],
    ['cases/decode/it-cmp.s', files('test-driver-decode.c')],
    ['cases/decode/it-inc-dec.s', files('test-driver-decode.c')],
    ['cases/decode/it-lea.s', files('test-driver-decode.c')],
    ['cases/decode/it-mov.s', files('test-driver-decode.c')],
    ['cases/decode/it-mul-div.s', files('test-driver-decode.c')],
    ['cases/decode/it-neg-not.s', files('test-driver-decode.c')],
    ['cases/decode/it-or.s', files('test-driver-decode.c')],
    ['cases/decode/it-pxor.s', files('test-driver-decode.c')],
    ['cases/decode/it-sbb.s', files('test-driver-decode.c')],
    ['cases/decode/it-shift.s', files('test-driver-decode.c')],
    ['cases/decode/it-sub.s', files('test-driver-decode.c')],
    ['cases/decode/it-ud2.s', files('test-driver-decode.c')],
    ['cases/decode/it-xor.s', files('test-driver-decode.c')],
    ['cases/decode/js-short.s', files('test-driver-decode.c')],
    ['cases/decode/modrm.s', files('test-driver-decode.c')],
    ['cases/decode/movq.s', files('test-driver-decode.c')],
    ['cases/decode/pop-word-ax.s', files('test-driver-decode.c')],
    ['cases/decode/pop-word-r10w.s', files('test-driver-decode.c')],
    ['cases/decode/push-word-ax.s', files('test-driver-decode.c')],
    ['cases/decode/push-word-r10w.s', files('test-driver-decode.c')],
    ['cases/decode/sse-arithmetic.s', files('test-driver-decode.c')],
    ['cases/decode/sse.s', files('test-driver-decode.c')],

    ['cases/generator/avx.s', files('test-driver-gen.c')],
    ['cases/generator/it-add.s', files('test-driver-gen.c')],
    ['cases/generator/it-and-imm.s', files('test-driver-gen.c')],
    ['cases/generator/it-pxor.s', files('test-driver-gen.c')],
    ['cases/generator/it-test.s', files('test-driver-gen.c')],
    ['cases/generator/it-undef.c', files('test-driver-generate.c')],
    ['cases/generator/modrm.s', files('test-driver-gen.c')],
    ['cases/generator/op-ret.c', files('test-driver-generate.c')],
    ['cases/generator/sse.s', files('test-driver-gen.c')],

    ['cases/integration/fp.s', files('test-driver.c')],
    ['cases/integration/it-and.s', files('test-driver.c'), ['--var']],
    ['cases/integration/it-cltq.s', files('test-driver.c'), ['--var']],
    ['cases/integration/op-call.s', files('test-driver.c')],
    ['cases/integration/op-inc.s', files('test-driver.c')],
    ['cases/integration/op-ja.s', files('test-driver.c')],
    ['cases/integration/op-jbe.s', files('test-driver.c')],
    ['cases/integration/op-je.s', files('test-driver.c'), ['--var', '0', '1']],
    ['cases/integration/op-jo.s', files('test-driver.c')],
    ['cases/integration/op-jp.s', files('test-driver.c')],
    ['cases/integration/op-js-1.s', files('test-driver.c')],
    ['cases/integration/op-js-2.s', files('test-driver.c')],
    ['cases/integration/op-jz-dynamic.s', files('test-driver.c')],
    ['cases/integration/op-jz.s', files('test-driver.c')],
    ['cases/integration/op-leave.s', files('test-driver.c')],
    ['cases/integration/op-mov-mem.s', files('test-driver.c'), ['--nobytes']],
    ['cases/integration/op-mov-r9d.s', files('test-driver.c')],
    ['cases/integration/op-push-pop.s', files('test-driver.c')],
    ['cases/integration/op-shl.s', files('test-driver.c')],
    ['cases/integration/segov.s', files('test-driver.c')],

    ['cases/examples/stencil-5.c', files('../examples/stencil.c'), ['5', '1002', '10'], ['-O2']],
    ['cases/examples/stencil-5-avx.c', files('../examples/stencil.c'), ['5', '1002', '10'], ['-O2', '-mavx']],
    ['cases/examples/stencil-7.c', files('../examples/stencil.c'), ['7', '1002', '10'], ['-O2']],
    ['cases/examples/stencil-7-avx.c', files('../examples/stencil.c'), ['7', '1002', '10'], ['-O2', '-mavx']],
    ['cases/examples/stencil-8.c', files('../examples/stencil.c'), ['8', '1002', '10'], ['-O2']],
    ['cases/examples/stencil-8-avx.c', files('../examples/stencil.c'), ['8', '1002', '10'], ['-O2', '-mavx']],
]

# Test cases which only work with GCC (but not Clang)
if meson.get_compiler('c').get_id() == 'gcc'
    tests += [
        ['cases/decode/it-add-buferr.s', files('test-driver-decode-buferr.c')],
        ['cases/decode/it-leave.s', files('test-driver-decode.c')],
        ['cases/decode/it-movsx.s', files('test-driver-decode.c')],
        ['cases/decode/it-movzx.s', files('test-driver-decode.c')],
        ['cases/decode/it-nop.s', files('test-driver-decode.c')],
        ['cases/decode/it-push-pop.s', files('test-driver-decode.c')],
        ['cases/decode/it-test.s', files('test-driver-decode.c')],
        ['cases/decode/js.s', files('test-driver-decode.c')],

        ['cases/examples/stencil-6.c', files('../examples/stencil.c'), ['6', '1002', '10'], ['-O2']],
        ['cases/examples/stencil-6-avx.c', files('../examples/stencil.c'), ['6', '1002', '10'], ['-O2', '-mavx']],
    ]
endif

foreach test : tests
    # Compile test
    c_args = []
    if test.length() > 3
        c_args += test.get(3)
    endif
    testExec = executable(test.get(0) + '.test.out', files(test.get(0)), test.get(1), c_args: c_args, link_with: libdbrew, include_directories: dbrew_includes)

    # Run arguments, passed to wrapper script
    args = files('test-meson.py') + files(test.get(0)) + [testExec.full_path()]
    if test.length() > 2
        args += test.get(2)
    endif
    test(test.get(0), python3, args: args, workdir: meson.current_source_dir())
endforeach
