#!/bin/bash
SRC=mkdir-5.2.1.c
BIN=mkdir-5.2.1

function clean {
  rm -rf file $BIN log d1/d2 d1 d3
  rm -rf temp*
  rm -rf cu-*
  return 0
}

function compile {
  case $COV in
    1) CFLAGS="-w -fprofile-arcs -ftest-coverage --coverage" ;;
    *) CFLAGS="-w -fsanitize=$1" ;;
  esac
  $CC $CFLAGS -o $BIN $SRC >& /dev/null || exit 1
  if [[ $CREDUCE -eq 1 ]]
  then
    reducer.native -lint $SRC >& /dev/null || exit 1
  fi
  return 0

}

function run {
  { timeout 0.1 ./$BIN $1 $2 ; } >& /dev/null || exit 1
  ls -ald $2 | cut -d ' ' -f 1,2,3,4 > temp1
  rm -rf $2
  /bin/mkdir $1 $2
  ls -ald $2 | cut -d ' ' -f 1,2,3,4 > temp2
  diff -q temp1 temp2 >& /dev/null || exit 1
  rm -rf $2
  return 0
}

function run_error {
  { timeout 0.1 ./$BIN $1 $2 ; } >& temp1 && exit 1
  /bin/mkdir $1 $2 >& temp2
  head -n 1 temp1 | cut -d ' ' -f 2,3 > temp3
  head -n 1 temp2 | cut -d ' ' -f 2,3 | sed "s/missing operand/too few/g" > temp4
  diff -q temp3 temp4 >& /dev/null || exit 1
  return 0
}

function default {
  run "" d1 || exit 1
  run_error "" d1/d2 || exit 1
  run_error "-m 123124" d1/d2 || exit 1
  run_error "-m" d1/d2 || exit 1
  run "-m 400" d1 || exit 1
  run "-m 555" d1 || exit 1
  run "-m 644" d1 || exit 1
  run "-m 610" d1 || exit 1
  run "-m 777" d1 || exit 1
  run "-p" d1/d2 || exit 1
  rm -rf d1
  run "-p" d1/d2/d3/d4 || exit 1
  rm -rf d1
  /bin/mkdir d1
  run "-p" d1 || exit 1
  rm -rf d1
  /bin/mkdir d1
  run "-p" d1/d2 || exit 1
  rm -rf d1
  return 0
}

function crash {
  r=$1
  grep "Sanitizer" log >& /dev/null && return 0
  if [[ $r -gt 128 && $r -le 162 ]] # crash
  then
    return 0
  fi
  return 1
}

OPT=("" "-m" "-p" "-v" "-Z" "-v" "--context" "--help")
function side {
  export srcdir=`pwd`/../
  export PATH="`pwd`:$PATH"
  { timeout 0.1 ./$BIN ; } >& log
  crash $? && exit 1
  touch file
  for opt in ${OPT[@]}
  do
    { timeout 0.1 ./$BIN $opt file ; } >& log
    crash $? && exit 1
  done
  return 0
}

clean
compile address || exit 1
default || exit 1
side || exit 1
clean
compile memory || exit 1
default || exit 1
side || exit 1
clean
