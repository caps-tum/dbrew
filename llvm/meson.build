
libllvm = dependency('llvm', version: ['>=8', '<=9'])
dbrewllvm_flags = ['-DLL_LLVM_MAJOR='+libllvm.version().split('.')[0]]

rellume = subproject('rellume')
librellume = rellume.get_variable('librellume')

subdir('src')

dbrewllvm = declare_dependency(include_directories: include_directories('include'),
                               link_with: libdbrewllvm_lib,
                               dependencies: [libllvm, librellume])
dbrewllvm_priv = declare_dependency(include_directories: include_directories('src', 'include'),
                                    link_with: libdbrewllvm_lib,
                                    dependencies: [libllvm, librellume])

subdir('test')
