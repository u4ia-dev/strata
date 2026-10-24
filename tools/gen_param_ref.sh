#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# The manual's parameter reference from the firmware's table: sh tools/gen_param_ref.sh (WSL or Linux, from the repo)
set -e
mkdir -p build/gen build/tools
cc -O1 -w -Ibuild/gen -Ifirmware/src -o build/tools/param_ref tools/param_ref.c -lm
build/tools/param_ref > book/manual/parameters.md
echo "param_ref: book/manual/parameters.md ($(grep -c '^| [A-Z0-9]' book/manual/parameters.md) rows)"
