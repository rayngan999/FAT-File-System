#!/bin/sh

./fs_ref.x info  disk123.fs > ref.stdout 2>ref.stderr
./test_fs.x info disk123.fs > lib.stdout 2>lib.stderr
./fs_make.x disk_test.fs 4196
./fs_ref.x script disk_test.fs ./scripts/script_err.example > ref.stdout 2>ref.stderr
./test_fs.x script disk_test.fs ./scripts/script_err.example > lib.stdout 2>lib.stderr
./fs_ref.x ls disk.fs > ref.stdout 2>ref.stderr
./test_fs.x ls disk.fs > lib.stdout 2>lib.stderr


# put output files into variables
REF_STDOUT=$(cat ref.stdout)
REF_STDERR=$(cat ref.stderr)
LIB_STDOUT=$(cat lib.stdout)
LIB_STDERR=$(cat lib.stderr)

cat ref.stdout
cat ref.stderr

cat lib.stdout
cat lib.stderr

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
rm disk_test.fs
rm ref.stdout ref.stderr
rm lib.stdout lib.stderr
