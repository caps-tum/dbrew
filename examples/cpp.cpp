// Compile with:
// clang++ -std=c++14 cpp.cpp -I../include/ ../libdbrew.a -o /tmp/cpp

#include "dbrew.hpp"

#include <iostream>

int foo(int i, int j) {
	if (i == 5) return 0;
	return i + j;
}

typedef int (*foo_t)(int, int);

int main(int argc, char *argv[]) {
	dbrew::rewriter<foo_t> a(foo);
	std::cout << a(1, 2) << std::endl; // 3
	dbrew::rewriter<foo_t> b = a.bind(0, 1);
	std::cout << b(2, 2) << std::endl; // 3 as well
	return 0;
}
