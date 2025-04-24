#! /bin/bash

make clean
make xcheck


if ! [[ -x xcheck ]]; then
    echo "xcheck executable does not exist"
    exit 1
fi

make create-tests
./create-tests

TARGET_DIR="tester"

cp "xcheck" "$TARGET_DIR"

cd "$TARGET_DIR"

chmod +x -R .


pwd

./run-tests.sh $* 