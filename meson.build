project('libdbrew', ['c', 'cpp'], default_options: ['buildtype=debugoptimized', 'warning_level=3', 'c_std=gnu99', 'cpp_std=c++17'])

if get_option('warning_level').to_int() >= 3
  add_project_arguments(['-Wmissing-field-initializers',
                         '-Wunused-parameter',
                         '-Wold-style-definition',
                         '-Wmissing-declarations',
                         '-Wmissing-prototypes',
                         '-Wredundant-decls',
                         '-Wmissing-noreturn',
                         '-Wshadow',
                         '-Wpointer-arith',
                         '-Wwrite-strings',
                         '-Winline',
                         '-Wformat-nonliteral',
                         '-Wformat-security',
                         '-Wswitch-default',
                         '-Winit-self',
                         '-Wnested-externs',
                         '-Wstrict-prototypes',
                         '-Wmissing-include-dirs',
                         '-Wundef',
                         '-Wmissing-format-attribute'
                        ], language: 'c')
endif

add_project_arguments(['-Wno-unused-parameter'], language: 'cpp')

subdir('src')
dbrew = declare_dependency(include_directories: include_directories('include'),
                           link_with: libdbrew)
dbrew_priv = declare_dependency(include_directories: dbrew_includes,
                                link_with: libdbrew)

install_headers('include/dbrew.h')
