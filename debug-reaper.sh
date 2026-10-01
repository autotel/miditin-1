#!/usr/bin/env bash
# Run REAPER under gdb; on a crash, write backtraces to ~/.local/state/miditin-crash/.
# Only non-fatal signals are ignored: in -batch mode any stop runs the bt commands and then kills REAPER.
set -u
dir="$HOME/.local/state/miditin-crash"
mkdir -p "$dir"
log="$dir/reaper-$(date +%F-%H%M%S).log"
ln -sfn "$log" "$dir/latest.log"
# A real-time thread that runs past RLIMIT_RTTIME without sleeping is SIGKILLed by the kernel.
# Anything launched from GNOME inherits gnome-shell's 200 ms, which REAPER's anticipative-FX
# bursts outrun on a heavy patch -- so start from the systemd user manager instead, with room for
# those bursts. The soft limit sits under the hard one so a real hang arrives as SIGXCPU, which
# gdb stops on, rather than as an untraceable SIGKILL.
systemd-run --user --pty --wait --collect --quiet --same-dir -p LimitRTTIME=1500ms:2s \
gdb -q -batch \
    -ex "set pagination off" \
    -ex "handle all nostop noprint pass" \
    -ex "handle SIGSEGV SIGABRT SIGBUS SIGFPE SIGILL SIGXCPU stop print" \
    -ex run \
    -ex "echo \n=== crashing thread ===\n" -ex "bt full" \
    -ex "echo \n=== all threads ===\n" -ex "thread apply all bt" \
    -ex "info sharedlibrary" \
    --args /opt/REAPER/reaper "$@" 2>&1 | tee "$log"
echo "log: $log"
