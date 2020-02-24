TOOD

* make tests out of examples
* thread-local-storage: make op-tls.c into test

Missing decode tests
* IP-relative addressing

Known Bugs
* idiv: with "/ 1", rdx gets not set to constant 0
* cmov: not correct with constants as input and unknown condition
* RIP-relative addressing of data may need constant pool (FIX?)
* push/pop of constant values still needs to capture rsp changes

