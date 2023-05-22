import sys
import os

args = sys.argv
binFile = open(args[1], 'rb')
binary_test = []
size = os.path.getsize(args[1])

for i in range(size):
    data = binFile.read(1)
    binary_test.append("0x" + data.hex())
binFile.close()

toCFileName = args[2]
objName = args[1][ : args[1].find(".")]
toCFile = open(toCFileName, mode='w', encoding='utf8')
toCFile.write(f"char binary_{objName}[] = " + "{\n")
for i in range(size-1):
    toCFile.write(binary_test[i] + ", ")
toCFile.write(binary_test[size-1])
toCFile.write("\n")
toCFile.write("};\n")
toCFile.write(f"int binary_{objName}_size = " + str(size) + ";\n")
toCFile.close()
