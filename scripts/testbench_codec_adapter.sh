#!/bin/bash
# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2018 Intel Corporation. All rights reserved.

#
# Usage:
# please run following scrits for the test tplg and host build
# ./scripts/build-tools.sh -t
# ./scripts/host-build-all.sh
# Run test
# ./scripts/testbench_codec_adapter.sh
#

# stop on most errors
set -e

function filesize() {
  du -b "$1" | awk '{print $1}'
}

function comparesize() {
  INPUT_SIZE=$1
  OUTPUT_SIZE=$2
  INPUT_SIZE_MIN=$(echo "$INPUT_SIZE*0.9/1"|bc)
  # echo "MIN SIZE with 90% tolerance is $INPUT_SIZE_MIN"
  # echo "ACTUALL SIZE is $OUTPUT_SIZE"
  if [[ "$OUTPUT_SIZE" -gt "$INPUT_SIZE" ]]; then
    echo "OUTPUT_SIZE $OUTPUT_SIZE too big"
    return 1
  fi
  if [[ "$OUTPUT_SIZE" -lt "$INPUT_SIZE_MIN" ]]; then
    echo "OUTPUT_SIZE $OUTPUT_SIZE too small"
    return 1
  fi

  return 0
}

function srcsize() {
  INPUT_SIZE=$1
  INPUT_RATE=$2
  OUTPUT_RATE=$3
  OUTPUT_SIZE=$(echo "${INPUT_SIZE}*${OUTPUT_RATE}/${INPUT_RATE}"|bc)
  echo "$OUTPUT_SIZE"
}

SCRIPTS_DIR=$(dirname "${BASH_SOURCE[0]}")
SOF_DIR=$SCRIPTS_DIR/../
TESTBENCH_DIR=${SOF_DIR}/tools/test/audio


cd "$TESTBENCH_DIR"

# remove .raw and .log files from test folder
rm -rf ./*.raw
rm -rf ./*.log

# test codec adapter 16 bit
INPUT_FILE=./inputs/audio_16.raw
OUTPUT_FILE=./codec_adapter_out_16.raw
LOG_FILE=./ca16.log

echo "=========================================================="
echo "test codec adapter with ./codec_adapter_run.sh 16 16 48000 " $INPUT_FILE " " $OUTPUT_FILE
if ./codec_adapter_run.sh 16 16 48000 $INPUT_FILE $OUTPUT_FILE &>$LOG_FILE; then
  echo "codec_adapter test passed!"
else
  echo "codec_adapter test failed!"
  cat $LOG_FILE
  exit 1
fi
if comparesize "$(filesize $INPUT_FILE)" "$(filesize $OUTPUT_FILE)"; then
  echo "codec_adapter_out size check passed!"
else
  echo "codec_adapter_out size check failed!"
  cat $LOG_FILE
  exit 1
fi

# test codec adapter 32 bit
INPUT_FILE=./inputs/audio_32.raw
OUTPUT_FILE=./codec_adapter_out_32.raw
LOG_FILE=./ca32.log

echo "=========================================================="
echo "test codec adapter with ./codec_adapter_run.sh 32 32 48000 " $INPUT_FILE " " $OUTPUT_FILE
if ./codec_adapter_run.sh 32 32 48000 $INPUT_FILE $OUTPUT_FILE &>$LOG_FILE; then
  echo "codec_adapter test passed!"
else
  echo "codec_adapter test failed!"
  cat $LOG_FILE
  exit 1
fi
if comparesize "$(filesize $INPUT_FILE)" "$(filesize $OUTPUT_FILE)"; then
  echo "codec_adapter_out size check passed!"
else
  echo "codec_adapter_out size check failed!"
  cat $LOG_FILE
  exit 1
fi


# rm volume_out.raw vol.log
