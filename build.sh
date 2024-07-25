#!/bin/bash

make

TID="0100000000000123"
BIN_NAME="nx_sysmodule.nsp"

mkdir -p "./out/atmosphere/contents/$TID"
cp "./$BIN_NAME" "./out/atmosphere/contents/$TID/exefs.nsp"

mkdir -p "./out/atmosphere/contents/$TID/flags"
touch "./out/atmosphere/contents/$TID/flags/boot2.flag"
