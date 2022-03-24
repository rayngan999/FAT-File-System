#!/bin/sh
cp disk.fs disk2.fs
# read script read from reference lib
./fs_ref.x script disk2.fs ./scripts/script_phase4_overwrite.example > ref.stdout 2>ref.stderr
# read script read from my lib
./test_fs.x script disk2.fs ./scripts/script_phase4_overwrite.example > lib.stdout 2>lib.stderr

# put output files into variables
REF_STDOUT=$(cat ref.stdout)
REF_STDERR=$(cat ref.stderr)
LIB_STDOUT=$(cat lib.stdout)
LIB_STDERR=$(cat lib.stderr)

# compare stdout
if [ "$REF_STDOUT" != "$LIB_STDOUT" ]; then
 echo "Stdout outputs don't match..."
 diff -u ref.stdout lib.stdout
else
 echo "Stdout outputs match!"
fi
# compare stderr
if [ "$REF_STDERR" != "$LIB_STDERR" ]; then
 echo "Stderr outputs don't match..."
 diff -u ref.stderr lib.stderr
else
 echo "Stderr outputs match!"
fi
# clean
rm ref.stdout ref.stderr
rm lib.stdout lib.stderr
