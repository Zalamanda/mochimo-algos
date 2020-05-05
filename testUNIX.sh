#!/bin/sh
echo
echo "  testUNIX.sh - Hash Functions compilation and execution script"
echo "                MD2, MD5, SHA1, SHA256, SHA3, Keccak, Blake2b"
echo

################
# Remove old error log

LOG="hashtest_error.log"
rm -f $LOG

################
# Build test software

printf "Building Hashtest... "
cc -o hashtest test/hashtest.c 2>>$LOG

if test -s $LOG
then
   echo "Error"
   echo
   cat $LOG
else
   echo "OK"
   echo
   echo "Done."

################
# Run software on success

   ./hashtest

fi

echo
exit
