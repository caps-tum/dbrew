sources = [
  'buffers.c',
  'config.c',
  'dbrew.c',
  'decode.c',
  'emulate.c',
  'engine.c',
  'error.c',
  'expr.c',
  'generate.c',
  'instr.c',
  'printer.c',
  'snippets.c',
  'vector.c',
]

dbrew_includes = include_directories('../include', '../include/priv')

libdbrew = static_library('dbrew', sources, include_directories: dbrew_includes, install: true)

