#!/bin/bash
# average vblanks per rendered frame along a scripted walk through E1M1 (1350 vblanks)
cd "$(dirname "$0")/.."
export PATH="$HOME/.devkitsms/bin:$PATH"
smstest squaredoom.sms tests/bench_walk.txt | grep "print: game_frames" | awk '{print $4}' | xargs | awk '{printf "walk: %.2f vblanks/frame (%.2f fps NTSC)\n", 1350/($2-$1), 60*($2-$1)/1350}'
