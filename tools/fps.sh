#!/bin/bash
# average vblanks per rendered frame at the E1M1 start view over 1200 vblanks
cd "$(dirname "$0")/.."
export PATH="$HOME/.devkitsms/bin:$PATH"
smstest squaredoom.sms tests/bench.tmpl.txt | grep "print: game_frames" | awk '{print $4}' | xargs | awk '{printf "%.2f vblanks/frame (%.2f fps NTSC)\n", 1200/($2-$1), 60*($2-$1)/1200}'
