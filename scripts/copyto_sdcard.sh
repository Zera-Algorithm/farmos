#!/bin/bash

# 将文件拷贝到sdcard.img中
# 用法：./scripts/copyto_sdcard.sh <file>
# 注意：需要在farmos根目录下调用此脚本

cd /home/zrp/farmos
sudo mount sdcard.img sdcard
sudo cp $1 sdcard
sync
sleep 1
sudo umount sdcard
cp sdcard.img fs.img

echo "finish copy $1 to sdcard.img"
