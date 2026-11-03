#!/bin/bash
# 用法: 在内核源码根目录执行  bash aresin-series/apply-all.sh
DIR=${1:-$(dirname "$0")}
cd "$(dirname "$0")/.." || exit 1
shopt -s nullglob
patches=("$DIR"/0*.patch)
if [ ${#patches[@]} -eq 0 ]; then
  echo "错误: 在 $DIR 下没有找到 patch 文件"; exit 1
fi
echo "共 ${#patches[@]} 个补丁，工作目录: $(pwd)"
FAIL=0
for p in "${patches[@]}"; do
  echo "================ $(basename "$p")"
  patch -p1 --forward < "$p" || FAIL=1
done
echo
echo "===== 汇总 ====="
[ $FAIL -ne 0 ] && echo "有补丁未完全应用，看上面报错"
REJ=$(find . -name "*.rej" | sort)
if [ -n "$REJ" ]; then
  echo "冲突文件(.rej):"; echo "$REJ" | tee /tmp/aresin-rej.txt
else
  echo "无 .rej"
fi
