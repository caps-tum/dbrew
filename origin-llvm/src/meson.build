
dbreworiginllvm_lib = static_library('dbreworiginllvm', 'dbrew-backend.c', 'support.cc',
                                     include_directories: include_directories('../include'),
                                     dependencies: [libllvm, librellume, dbrew_priv],
                                     install: true)
