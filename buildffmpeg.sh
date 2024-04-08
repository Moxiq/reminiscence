#!/bin/bash

SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )"

cd FFmpeg

./configure --prefix=$SCRIPT_DIR/bin --enable-libpulse --enable-nvenc --enable-ffnvcodec

make -j$(nproc)
make install

cd ..
