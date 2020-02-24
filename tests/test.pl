#!/usr/bin/perl

foreach(<op-*.s>) {
    s/^(.*)\.s$/$1/;
    $cmd = "cc test-parser.c ../spec.c $_.s -o test-$_";
    print "$cmd\n";
    system $cmd;
}

