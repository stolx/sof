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
  echo "INPUT_SIZE is $INPUT_SIZE"
  echo "OUTPUT_SIZE is $OUTPUT_SIZE"
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

function do_test() {
  WIDTH=$1
  RATE=$2
  INPUT=$3
  OUTPUT=$4
  LOG=$5

  echo "=========================================================="
  echo "./maxxeffect_run.sh " $WIDTH " " $WIDTH " " $RATE " " $INPUT " " $OUTPUT
  if ./maxxeffect_run.sh $WIDTH $WIDTH $RATE  $INPUT $OUTPUT &>$LOG; then
    echo "codec_adapter test passed!"
  else
    echo "codec_adapter test failed!"
    cat $LOG
    exit 1
  fi
  if comparesize "$(filesize $INPUT)" "$(filesize $OUTPUT)"; then
    echo "codec_adapter_out size check passed!"
  else
    echo "codec_adapter_out size check failed!"
    cat $LOG
    exit 1
  fi

  return 0
}

SCRIPTS_DIR=$(dirname "${BASH_SOURCE[0]}")
SOF_DIR=$SCRIPTS_DIR/../
TESTBENCH_DIR=${SOF_DIR}/tools/test/audio


cd "$TESTBENCH_DIR"

# remove .raw and .log files from test folder
rm -rf ./*.raw
rm -rf ./*.log
rm -rf ./outputs/
mkdir ./outputs

do_test 32 48000 ./inputs/sweep_2s_2ch_48000_32.raw     ./ca_sweep_2s_2ch_48000_32.raw     ./sweep_2s_2ch_48000_32.log
do_test 24 48000 ./inputs/sweep_2s_2ch_48000_24_LE4.raw ./ca_sweep_2s_2ch_48000_24_LE4.raw ./sweep_2s_2ch_48000_24_LE4.log
do_test 16 48000 ./inputs/sweep_2s_2ch_48000_16.raw     ./ca_sweep_2s_2ch_48000_16.raw     ./sweep_2s_2ch_48000_16.log
do_test 32 44100 ./inputs/sweep_2s_2ch_44100_32.raw     ./ca_sweep_2s_2ch_44100_32.raw     ./sweep_2s_2ch_44100_32.log
do_test 24 44100 ./inputs/sweep_2s_2ch_44100_24_LE4.raw ./ca_sweep_2s_2ch_44100_24_LE4.raw ./sweep_2s_2ch_44100_24_LE4.log
do_test 16 44100 ./inputs/sweep_2s_2ch_44100_16.raw     ./ca_sweep_2s_2ch_44100_16.raw     ./sweep_2s_2ch_44100_16.log
