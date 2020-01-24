
libllvm = dependency('llvm', version: ['>=8', '<=9'])

rellume = subproject('rellume')
librellume = rellume.get_variable('librellume')

subdir('src')

dbreworiginllvm = declare_dependency(include_directories: include_directories('include'),
                                     link_with: dbreworiginllvm_lib,
                                     dependencies: [dbrew, libllvm])

subdir('benchmark')
subdir('examples')
