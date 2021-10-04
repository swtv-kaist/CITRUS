#!/bin/bash
set -euo pipefail

TARGET_DIR=$1
SUBJECTS=("hjson-cpp" "json-voorhees" "JsonBox" "jsoncpp" "jvar" "re2" "tinyxml2" "yaml-cpp")

PWD=$(pwd)

mkdir -p ${TARGET_DIR}
cd ${TARGET_DIR}
for SUB in "${SUBJECTS[@]}"; do
  rm -rf ${SUB}
done

cp ../replication/CITRUS_target_project.zip .
unzip CITRUS_target_project.zip

for SUBDIR in "${SUBJECTS[@]}" ; do
    pushd ${SUBDIR}

    mkdir -p build && cd build
    cmake -DCMAKE_C_FLAGS="-g -O0 -fsanitize=fuzzer-no-link --coverage" \
     -DCMAKE_CXX_FLAGS="-g -O0 -fsanitize=fuzzer-no-link --coverage" \
     -DCMAKE_EXPORT_COMPILE_COMMANDS=1 -DCMAKE_C_COMPILER="clang" -DCMAKE_CXX_COMPILER="clang++" ..
    mv compile_commands.json ..
    make -j3
    cd ..

    mkdir -p build_libfuzzer && cd build_libfuzzer
    cmake -DCMAKE_C_FLAGS="-g -O0 -fsanitize=fuzzer-no-link --coverage" \
     -DCMAKE_CXX_FLAGS="-g -O0 -fsanitize=fuzzer-no-link --coverage" \
     -DCMAKE_EXPORT_COMPILE_COMMANDS=1 -DCMAKE_C_COMPILER="clang" -DCMAKE_CXX_COMPILER="clang++" ..
    make -j3
    cd ..

    popd
done
