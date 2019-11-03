#!/usr/bin/python
#
# rsrc.py
# Read or write a resource in a resource map
# (c) 2017 Andrew Simmonds
#
# References
# Resource Manager: http://mirror.informatimago.com/next/developer.apple.com/documentation/mac/MoreToolbox/MoreToolbox-9.html

import sys
from os import path
sys.tracebacklimit = 0
if len(sys.argv) < 3:
    print """Usage: %s <action> [-df] <file> [action parameters]
Actions:
    read:   <type> <id>
    write:  <type> <id> [name]
    list:   [type]
    delete: <type> <id>""" % path.basename(sys.argv[0])
    sys.exit(1)

from Carbon.Files import * # File constants
from Carbon.Res import * # Resource Manager functions

# FSCreateResourceFile unavailable in 64-bit, create it manually
def CreateResourceFile(rFile, dataFork=false):
    import struct
    resMap = struct.pack('>8xHHh', 28, 30, -1) # offset to type list, offset to name list, num types - 1
    resHead = struct.pack('>LLLL', 256, 256, 0, len(resMap)+16) # offset to data, offset to map, length of data, length of map
    if not dataFork:
        open(rFile, 'a').close()
        rFile += '/..namedfork/rsrc'
    with open(rFile, 'wb') as f:
        f.write(resHead + '\0'*240 + resHead + resMap)

args = sys.argv
action = args[1]
dataFork = args[2] == '-df'
if dataFork:
    rFork = u''
    args = args[1:]
else:
    rFork = u'RESOURCE_FORK'
rFile = args[2]
if len(args) > 3: rType = args[3]
if len(args) > 4: rID = int(args[4])

if action == 'read':
    rf = FSOpenResourceFile(rFile, rFork, fsRdPerm)
    rez = Get1Resource(rType, rID)
    sys.stdout.write(rez.data) # Print without newline
    CloseResFile(rf)
elif action == 'write':
    if not path.exists(rFile): CreateResourceFile(rFile, dataFork)
    rf = FSOpenResourceFile(rFile, rFork, fsWrPerm)
    try: # Remove resource if it exists
        Get1Resource(rType, rID).RemoveResource()
        UpdateResFile(rf)
    except: # Resource didn't exist, must close and reopen
        CloseResFile(rf)
        rf = FSOpenResourceFile(rFile, rFork, fsWrPerm)

    if len(args) > 5: rName = args[5]
    else: rName = ''
    rez = Handle(sys.stdin.read())
    rez.AddResource(rType, rID, rName)
    CloseResFile(rf)
elif action == 'list':
    rf = FSOpenResourceFile(rFile, rFork, fsRdPerm)
    if 'rType' in locals(): # List resources of type
        for i in range(Count1Resources(rType)):
            info = Get1IndResource(rType, i+1).GetResInfo() # id, type, name
            print str(info[0])+' '+info[2]
    else: # List types
        for i in range(Count1Types()):
            rType = Get1IndType(i+1)
            print rType+' '+str(Count1Resources(rType))
    CloseResFile(rf)
elif action == 'delete':
    rf = FSOpenResourceFile(rFile, rFork, fsWrPerm)
    Get1Resource(rType, rID).RemoveResource()
    UpdateResFile(rf)
    CloseResFile(rf)
