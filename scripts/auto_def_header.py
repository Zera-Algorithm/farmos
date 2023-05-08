"""
auto_def_header.py
------
作者：张仁鹏
用途：用于给include目录里的头文件批量加防止重复引入的 "#ifndef" 宏
注意：应放置在tools/目录下
"""

import os
print(__file__)
os.chdir(os.path.dirname(__file__))
headers = os.listdir("../include")

for header in headers:
    filename = "../include/" + header
    with open(filename, "r") as f:
        is_have = f.readline().startswith("#ifndef")

    symbol = "_" + header[0:-2].upper() + "_H"

    print("%-50s" % filename, end=" ")
    if not is_have:
        with open(filename, "r+") as f:
            old = f.read()
            f.seek(0)
            f.write(f"#ifndef {symbol}\n")
            f.write(f"#define {symbol}\n")
            f.write(old)
            f.write("#endif")
        print("ADDED")
    else:
        print("OK")
