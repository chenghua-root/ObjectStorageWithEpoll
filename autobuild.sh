#!/bin/bash

die() {
    echo "$@" >&2
    exit 1
}

BASE_DIR=$(cd "$(dirname "$0")"; pwd)
cd $BASE_DIR

if [ ! -d "build" ]; then
  mkdir -p build
fi

if [ -z $1 ]
then
  # compile with debug info
  echo -e "\033[33m begin compile with debug \033[0m"
  rm bin -rf && mkdir -p bin

  (pushd build                                          \
      && cmake .. -DCMAKE_BUILD_TYPE="Debug" -DCMAKE_EXPORT_COMPILE_COMMANDS=1           \
      && make -j2                                       \
      && popd) || die make "build"
  echo -e "\033[33m end compile \033[0m"

elif [ "x$1" == "xrelease" ] || [ "x$1" == "xRelease" ]
then
  # compile release with no debug info
  echo -e "\033[33m begin compile release \033[0m"
  rm build bin -rf && mkdir -p build bin

  (pushd build                                          \
      && cmake .. -DCMAKE_BUILD_TYPE="Release" -DCMAKE_EXPORT_COMPILE_COMMANDS=1          \
      && make -j2                                       \
      && popd) || die make "build"
  echo -e "\033[33m end compile \033[0m"

else
    echo "invalid argument. you could run it as: ./autobuild.sh [release|unittest|coverage]"
fi
