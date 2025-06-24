#!/bin/sh
docker build -t scrcpy-web -f ./Dockerfile .
if [ $? -ne 0 ]; then
    echo "Docker build failed. Fix it and try again."
    exit 1
fi

mkdir -p dist
docker run --rm -i -v "${PWD}/dist:/dist" scrcpy-web /bin/sh -c "cp -r /src/scrcpy/release/work/build-linux-x86_64/dist /dist/linux-x86_64 && cp -r /src/scrcpy/release/work/build-win32/dist /dist/win32 && cp -r /src/scrcpy/release/work/build-win64/dist /dist/win64 && cp -r /src/scrcpy/release/work/build-macos-x86_64/dist /dist/macos-x86_64 && cp -r /src/scrcpy/release/work/build-macos-aarch64/dist /dist/macos-aarch64"