#!/bin/bash
# run every scripted test against squaredoom.sms; non-zero exit if any fails
cd "$(dirname "$0")/.."
export PATH="$HOME/.devkitsms/bin:$PATH"
fail=0
for t in tests/*.tmpl; do
  n=$(basename $t .tmpl)
  python3 tools/symaddr.py $t > tests/$n.txt
  if smstest squaredoom.sms tests/$n.txt > tests/$n.log 2>&1; then echo "PASS $n"; else echo "FAIL $n"; fail=1; grep FAIL tests/$n.log; fi
done
exit $fail
