#!/bin/bash
echo "Testing installer..."
bash build.sh
if [ $? -ne 0 ]; then
    echo "Build failed, cannot test installer."
    exit 1
fi
./bin/installer
