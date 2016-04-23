#/bin/bash

cd /home/a/ogonek/vtrpk-build-tools
docker run --rm -v "$PWD":/home/a/ogonek/vtrpk-build-tools -w /home/a/ogonek/vtrpk-build-tools a/gcc48:latest ./build.sh
