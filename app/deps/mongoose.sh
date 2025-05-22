#!/usr/bin/env bash
set -ex
DEPS_DIR=$(dirname ${BASH_SOURCE[0]})
cd "$DEPS_DIR"
. common
process_args "$@"

VERSION=7.11
FILENAME=mongoose-$VERSION.tar.gz
PROJECT_DIR=mongoose-$VERSION
SHA256SUM=728c94b764a54dd5fc0358bdfd2c9fee26b8e8fe65d4bb89c2a6ce70bcc91ce4

cd "$SOURCES_DIR"

if [[ -d "$PROJECT_DIR" ]]
then
    echo "$PWD/$PROJECT_DIR" found
else
    get_file "https://github.com/cesanta/mongoose/archive/refs/tags/$VERSION.tar.gz" "$FILENAME" "$SHA256SUM"
    tar xf "$FILENAME"
fi

# Mongoose is header-only, so we just need to copy the files
mkdir -p "$INSTALL_DIR/$DIRNAME/include"
cp "$PROJECT_DIR/mongoose.h" "$INSTALL_DIR/$DIRNAME/include/"
cp "$PROJECT_DIR/mongoose.c" "$INSTALL_DIR/$DIRNAME/include/"

echo "Mongoose files installed to $INSTALL_DIR/$DIRNAME/include"
