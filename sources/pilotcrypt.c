#import <Carbon/Carbon.h>

void ExpandEVResource(Handle theResource, OSType resType) {
    UInt16  i;
    UInt32  magicNum;
    UInt32  *vPtr = (UInt32 *) *theResource;
    UInt8   *bPtr;
    UInt32  resSize;
    
    if (resType == 'Mp\225L' || resType == 'Op\225L')
        magicNum = 0xABCD1234;
    else if (resType == 'Np\225L')
        magicNum = 0xB36A210F;
    else return;
    
    resSize = GetHandleSize(theResource);
    for (i = resSize/4; i > 0; i--) {
        *(vPtr++)   ^= CFSwapInt32HostToBig(magicNum);
        magicNum    += 0xDEADBEEF;
        magicNum    ^= 0xDEADBEEF;
    }
    bPtr = (UInt8 *) vPtr;
    for (i = resSize%4; i > 0; i--) {
        *(bPtr++)   ^= magicNum >> 24;
        magicNum    <<= 8;
    }
    
    ChangedResource(theResource);
}

void ExpandResourceType(OSType resType) {
    Handle  theResource;
    SInt16  i, numResources;
    
    numResources = Count1Resources(resType);
    for (i = 1; i <= numResources; i++) {
        theResource = Get1IndResource(resType, i);
        if (theResource != nil) {
            ExpandEVResource(theResource, resType);
            ReleaseResource(theResource);
        }
    }
}

void ExpandResources() {
    UInt16  i, numTypes;
    OSType  resType;
    
    numTypes = Count1Types();
    for (i = 1; i <= numTypes; i++) {
        Get1IndType(&resType, i);
        if (resType == 'Mp\225L' || resType == 'Op\225L' || resType == 'Np\225L')
            ExpandResourceType(resType);
    }
}

int main(int argc, const char * argv[]) {
    UInt8           *path;
    OSErr           errorCode;
    FSRef           fileRef;
    Boolean         isDirectory;
    HFSUniStr255    forkName;
    ResFileRefNum   inputRefNum;
    
    if (argc < 2) {
        printf("usage: pilotcrypt <path>\n");
        return noErr;
    }
        
    path = (UInt8 *)argv[1];
    errorCode = FSPathMakeRef(path, &fileRef, &isDirectory);
    if (!errorCode) {
        FSGetDataForkName(&forkName);
        errorCode = FSOpenResourceFile(&fileRef, forkName.length, forkName.unicode, fsRdWrPerm, &inputRefNum);
        if (errorCode) {
            FSGetResourceForkName(&forkName);
            errorCode = FSOpenResourceFile(&fileRef, forkName.length, forkName.unicode, fsRdWrPerm, &inputRefNum);
        }
        if (!errorCode) {
            UseResFile(inputRefNum);
            ExpandResources();
            CloseResFile(inputRefNum);
        }
    }
    else if (isDirectory)
        errorCode = paramErr;
    
    if (errorCode)
        printf("Error: %d\n", errorCode);
    return errorCode;
}
