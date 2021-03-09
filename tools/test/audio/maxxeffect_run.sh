#!/bin/bash

# stop on most errors
set -e

usage ()
{
    echo "Usage:   $0 <bits in> <bits out> <rate> <input> <output>"
    echo "Example: $0 16 16 48000 input.raw output.raw"
}

main ()
{
    local COMP DIRECTION

    if [ $# -ne 5 ]; then
        usage "$0"
        exit 1
    fi

    COMP=maxxeffect
    DIRECTION=playback

    ./comp_run.sh $COMP $DIRECTION "$1" "$2" "$3" "$3" "$4" "$5"
}

main "$@"
