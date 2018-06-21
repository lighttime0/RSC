#! /bin/bash

clang -emit-llvm -S test.c -o test.bc

opt -load ../../build/tools/demo-tool/libRSC-demo.so -RSC-demo < test.bc > /dev/null

rm test.bc
