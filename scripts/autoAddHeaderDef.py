"""
auto_def_header.py
------
作者：张仁鹏
用途：用于给include目录里的头文件批量加防止重复引入的 "#ifndef" 宏
注意：应放置在 scripts/目录下，在farmOS目录下调用
"""

import os
print(__file__)
os.chdir(os.path.dirname(__file__))

def addHeader(path, item):
    with open(path, "r") as f:
        is_have = f.readline().startswith("#ifndef")

    symbol = "_" + item[0:-2].upper() + "_H"

    print("%-50s" % path, end=" ")
    if not is_have:
        with open(path, "r+") as f:
            old = f.read()
            f.seek(0)
            f.write(f"#ifndef {symbol}\n")
            f.write(f"#define {symbol}\n")
            f.write(old)
            f.write("#endif")
        print("ADDED")
    else:
        print("OK")


def scanDir(dir):
    items = os.listdir(dir)
    for item in items:
        path = dir + item
        if os.path.isdir(path):
            scanDir(path + "/")
        elif item.endswith(".h"):
            addHeader(path, item)

scanDir("../include/")

