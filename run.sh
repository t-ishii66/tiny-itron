#!/bin/bash
# Run tiny-itron kernel in QEMU
# Usage: ./run.sh [options]
#   (default) curses mode - VGA text displayed in terminal (ESC to quit)
#   -g    GTK window mode
#   -n    No graphics (serial on stdio, VGA output not visible)
#   -d    Debug mode (log interrupts to qemu.log)
#   -G    GDB mode (wait for GDB connection on port 1234)
set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
IMAGE="$SCRIPT_DIR/i386/i386"
FLOPPY="$SCRIPT_DIR/i386/floppy.img"

# Build if needed
if [ ! -f "$IMAGE" ]; then
    echo "Building kernel..."
    make -C "$SCRIPT_DIR/kernel"
    make -C "$SCRIPT_DIR/lib"
    make -C "$SCRIPT_DIR/i386"
fi

# Create a 1.44MB floppy image
dd if=/dev/zero of="$FLOPPY" bs=512 count=2880 2>/dev/null
dd if="$IMAGE" of="$FLOPPY" conv=notrunc 2>/dev/null
echo "Floppy image: $(wc -c < "$IMAGE") bytes kernel -> 1.44MB floppy"

# Parse options
EXTRA_OPTS=""
DISPLAY_OPT="-display curses"
while getopts "dgnG" opt; do
    case $opt in
        d) EXTRA_OPTS="$EXTRA_OPTS -d int,cpu_reset -D qemu.log"
           echo "Debug mode: interrupt log -> qemu.log" ;;
        g) DISPLAY_OPT="-display gtk,grab-on-hover=off"
           echo "  GTK mode: Ctrl+Alt+G to release mouse/keyboard grab" ;;
        n) DISPLAY_OPT="-nographic" ;;
        G) EXTRA_OPTS="$EXTRA_OPTS -s -S"
           echo "GDB mode: waiting for connection on port 1234..." ;;
    esac
done

echo "Starting QEMU..."
if [ "$DISPLAY_OPT" = "-display curses" ]; then
    echo "  Ctrl-C to quit (handled by guest kernel → CPU reset)"
    # curses mode changes terminal settings; restore on any exit
    qemu-system-i386 \
        -drive file="$FLOPPY",format=raw,if=floppy \
        -boot a \
        -m 16 \
        -cpu pentium3 \
        -accel tcg,thread=multi \
        -smp 2 \
        $DISPLAY_OPT \
        -no-reboot \
        $EXTRA_OPTS || true
    reset
else
    echo "  Close window or press Ctrl-C to quit"
    exec qemu-system-i386 \
        -drive file="$FLOPPY",format=raw,if=floppy \
        -boot a \
        -m 16 \
        -cpu pentium3 \
        -accel tcg,thread=multi \
        -smp 2 \
        $DISPLAY_OPT \
        -no-reboot \
        -no-shutdown \
        $EXTRA_OPTS
fi
