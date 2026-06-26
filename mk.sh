#!/usr/bin/env bash
set -euo pipefail

BUILD_DIR="build"
TIMER_NO_PAUSE="ON"

PICO_BOARD="pico_w"
usage(){
    echo "Usage: $0 [clean] [pico|picow] [timer-on|timer-off]"
    echo "  (no args)  : incremental build, board = pico_w, timer no pause = ON"
    echo "  clean      : remove build dir then configure & build"
    echo "  pico       : build for Raspberry Pi Pico"
    echo "  picow      : build for Raspberry Pi Pico W"
    echo "  timer-on   : build with RP2040_DEBUG_TIMER_NO_PAUSE=ON"
    echo "  timer-off  : build with RP2040_DEBUG_TIMER_NO_PAUSE=OFF"
    echo ""
    echo "Examples:"
    echo "  $0"
    echo "  $0 clean"
    echo "  $0 clean picow"
    echo "  $0 clean pico"
    echo "  $0 clean picow timer-on"
    echo "  $0 clean picow timer-off"
}

for arg in "$@"; do
    case "$arg" in
        -h|--help)
            usage
            exit 0
            ;;
        clean)
            rm -rf "$BUILD_DIR"
            ;;
        pico)
            PICO_BOARD="pico"
            ;;
        picow|pico-w|pico_w)
            PICO_BOARD="pico_w"
            ;;
        timer-on)
            TIMER_NO_PAUSE="ON"
            ;;
        timer-off)
            TIMER_NO_PAUSE="OFF"
            ;;
        *)
            echo "Unknown arg: $arg"
            usage
            exit 1
            ;;
    esac
done

mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"

echo "PICO_BOARD=${PICO_BOARD}"
echo "RP2040_DEBUG_TIMER_NO_PAUSE=${TIMER_NO_PAUSE}"

cmake .. \
    -DPICO_BOARD="${PICO_BOARD}" \
    -DRP2040_DEBUG_TIMER_NO_PAUSE="${TIMER_NO_PAUSE}"

make -j"$(nproc)"