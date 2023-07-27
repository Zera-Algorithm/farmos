import json
import sys
import os

print(sys.argv[1])
jsonDict = json.loads(open(sys.argv[1], "r").read())
for item in jsonDict:
    if (item["all"] != item["passed"]):
    	print(item["name"])
