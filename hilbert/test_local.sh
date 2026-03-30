#!/usr/bin/bash

INCLUDES="-I.. -I../include"
CFLAGS="-std=c++17 -Werror -Wall -Wextra -Wpedantic -O1"

# Compile
g++ -c geometry.cc -o geometry.o $INCLUDES $CFLAGS
g++ -c hilbert_curve.cc -o hilbert_curve.o $INCLUDES $CFLAGS

g++ -c geometry_number_test.cc -o geometry_number_test.o $INCLUDES $CFLAGS
g++ -c geometry_rectangle_test.cc -o geometry_rectangle_test.o $INCLUDES $CFLAGS
g++ -c hilbert_curve_test.cc -o hilbert_curve_test.o $INCLUDES $CFLAGS
g++ -c rtree_test.cc -o rtree_test.o $INCLUDES $CFLAGS
echo "=== Compiled ==="

# Link
g++ geometry.o geometry_number_test.o -o geometry_number_test
g++ geometry.o geometry_rectangle_test.o -o geometry_rectangle_test
g++ geometry.o hilbert_curve.o hilbert_curve_test.o -o hilbert_curve_test
g++ geometry.o hilbert_curve.o rtree_test.o -o rtree_test

echo "=== Linked ==="

# Test
chmod +x geometry_number_test
chmod +x geometry_rectangle_test
chmod +x hilbert_curve_test
chmod +x rtree_test
./geometry_number_test
./geometry_rectangle_test
./hilbert_curve_test
./rtree_test

# Remove
rm *.o geometry_number_test geometry_rectangle_test hilbert_curve_test rtree_test
