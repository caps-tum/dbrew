project('libdbrew', 'c', default_options: ['warning_level=3'])

if get_option('warning_level').to_int() >= 3
    add_project_arguments(['-Wmissing-field-initializers', '-Wunused-parameter', '-Wold-style-definition', '-Wmissing-declarations', '-Wmissing-prototypes', '-Wredundant-decls', '-Wmissing-noreturn', '-Wshadow', '-Wpointer-arith', '-Wwrite-strings', '-Winline', '-Wformat-nonliteral', '-Wformat-security', '-Wswitch-default', '-Winit-self', '-Wnested-externs', '-Wstrict-prototypes', '-Wmissing-include-dirs', '-Wundef', '-Wmissing-format-attribute'], language: 'c')
endif

cflags = ['-std=gnu99']

sources = [
    'src/buffers.c',
    'src/config.c',
    'src/dbrew.c',
    'src/decode.c',
    'src/emulate.c',
    'src/engine.c',
    'src/error.c',
    'src/expr.c',
    'src/generate.c',
    'src/instr.c',
    'src/printer.c',
    'src/snippets.c',
    'src/vector.c',
]

# Also used for tests
dbrew_includes = include_directories('include', 'include/priv')
libdbrew = static_library('dbrew', sources, c_args: cflags, include_directories: dbrew_includes)

subdir('tests')
