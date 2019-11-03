#import <Carbon/Carbon.h>

Boolean mySaveResource(const Handle theResource) {
	UInt16	resAttributes;
	Boolean itWorked;
	
	resAttributes = GetResAttrs(theResource);
	if (resAttributes & resProtected) {
		SetResAttrs(theResource, resAttributes ^ resProtected);	//set as unprotected
		ChangedResource(theResource);
		itWorked = ResError()==noErr; 
		if (itWorked)
			WriteResource(theResource);
		SetResAttrs(theResource, resAttributes);					//set as protected
		//this setting will be saved even though we don't call changed/write resource
	}
	else {
		ChangedResource(theResource);
		itWorked = ResError()==noErr; 
		if (itWorked)
			WriteResource(theResource);
	}
	if (!itWorked) DebugStr("\pError returned by ChangedResource - cannot save");
	return itWorked;
}

void ExpandEVResource(Handle theResource, OSType resType) {
	UInt16		i;
	UInt32		magicNum;
	UInt32		*vPtr = (UInt32 *) *theResource;
	UInt8		*bPtr;
	UInt32		resSize;
	
	if (resType == 'Mp\225L' || resType == 'Op\225L')	
		magicNum = 0xABCD1234;
	else if (resType == 'Np\225L')
		magicNum = 0xB36A210F;
	else return;
	
	resSize = GetHandleSize(theResource);
	for (i = resSize/4; i > 0; i--) {
		*(vPtr++)	^= CFSwapInt32HostToBig(magicNum);
		magicNum	+= 0xDEADBEEF;
		magicNum	^= 0xDEADBEEF;
	}
	bPtr = (UInt8 *) vPtr;
	for (i = resSize%4; i > 0; i--) {
		*(bPtr++)	^= magicNum >> 24;
		magicNum   <<= 8;
	}
}

void ExpandResourceType(OSType resType) {
	Handle		theResource;
	SInt16		i, resID, numResources;
	Boolean		itWorked;
	Str255		resName;
	
	numResources = Count1Resources(resType);
	for (i = 1; i <= numResources; i++) {
		theResource = Get1IndResource(resType, i);
		itWorked = (theResource != nil);
		if (itWorked) {
			HLockHi(theResource);
			GetResInfo(theResource, &resID, &resType, resName);
			ExpandEVResource(theResource, resType);
			itWorked = mySaveResource(theResource);
			HUnlock(theResource);
			ReleaseResource(theResource);
		}	
	}
}

void ExpandResources(void) {
	UInt16		i, numTypes;
	OSType		resType;
	
	numTypes = Count1Types();
	for (i = 1; i <= numTypes; i++) {
		Get1IndType(&resType, i);
		if (resType == 'Mp\225L' || resType == 'Op\225L' || resType == 'Np\225L')
			ExpandResourceType(resType);
	}
}

int main (int argc, const char * argv[]) {
	UInt8				*path;
	OSErr				errorCode;
	FSRef				fileRef;
	Boolean				isDirectory;
	HFSUniStr255		resourceForkName;
	ResFileRefNum		inputRefNum;
	
	if (argc < 2) {
		printf("usage: pilotcrypt <path>\n");
		return noErr;
	}
		
	path = (UInt8 *)argv[1];
	errorCode = FSPathMakeRef(path, &fileRef, &isDirectory);
	if (!errorCode) {
		FSGetResourceForkName(&resourceForkName);
		errorCode = FSOpenResourceFile(&fileRef, resourceForkName.length, resourceForkName.unicode, fsRdWrPerm, &inputRefNum);
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