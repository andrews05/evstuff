/**

 Convert EV Nova plugins between Mac and Windows (.rez) formats
 © 2019 Andrew Simmonds

 */

#import <Carbon/Carbon.h>

typedef struct RezHeader1 {
    OSType signature;
    UInt32 version;
    UInt32 header2Length;
} RezHeader1;

typedef struct RezHeader2 {
    UInt32 unknown;
    UInt32 firstIndex;
    UInt32 numEntries;
} RezHeader2;

typedef struct RezOffset {
    UInt32 offset;
    UInt32 size;
    UInt32 unknown;
} RezOffset;

typedef struct RezMapHeader {
    UInt32 unknown;
    UInt32 numTypes;
} RezMapHeader;

typedef struct RezTypeInfo {
    OSType type;
    UInt32 mapOffset;
    UInt32 numResources;
} RezTypeInfo;

typedef struct RezResourceInfo {
    UInt32 index;
    OSType type;
    SInt16 id;
    char name[256];
} RezResourceInfo;

const OSType    RezSignature = 'BRGR'; // burgerbill
const UInt32    RezVersion = 1;
const char      RezMapName[] = "resource.map";
const short     RezInfoSize = 4+4+2+256; // RezResourceInfo is not 4-byte aligned so can't use sizeof

OSStatus FlipperNoFlipping(OSType dataDomain, OSType dataType, SInt16 id, void *dataPtr, ByteCount dataSize, Boolean currentlyNative, void *refCon) {
    return 0;
}

OSErr npif2rez(FSRef npif, char *rez) {
    OSErr           errorCode;
    HFSUniStr255    forkName;
    ResFileRefNum   inputRefNum;
    UInt32          i, j, offset, numTypes;
    UInt32          numResources = 0;
    UInt32          index = 1;
    OSType          resType;
    SInt16          rID;
    Str255          resourceName;

    FSGetResourceForkName(&forkName);
    errorCode = FSOpenResourceFile(&npif, forkName.length, forkName.unicode, fsRdPerm, &inputRefNum);
    if (!errorCode) {
        UseResFile(inputRefNum);

        numTypes = Count1Types();
        RezTypeInfo typesInfo[numTypes];
        offset = sizeof(RezMapHeader) + sizeof(typesInfo);
        for (i = 0; i < numTypes; i++) {
            Get1IndType(&resType, i+1);
            typesInfo[i].type = resType;
            typesInfo[i].mapOffset = CFSwapInt32HostToBig(offset);
            typesInfo[i].numResources = Count1Resources(resType);
            offset += RezInfoSize * typesInfo[i].numResources;
            numResources += typesInfo[i].numResources;
            // Bypass any default flippers. This is needed for some standard resource types, including cicn, DITL, DLOG, snd , STR#.
            CoreEndianInstallFlipper(kCoreEndianResourceManagerDomain, resType, FlipperNoFlipping, NULL);
        }

        RezHeader2 header2 = {1, index, numResources+1};
        RezOffset offsets[header2.numEntries];
        RezHeader1 header1 = {CFSwapInt32HostToBig(RezSignature), RezVersion, sizeof(header2) + sizeof(offsets) + sizeof(RezMapName)};
        // The resource map, including types and resource info, is all big-endian
        RezMapHeader mapHeader = {CFSwapInt32HostToBig(8), CFSwapInt32HostToBig(numTypes)};
        RezResourceInfo resourcesInfo[numResources];
        Handle resources[numResources];

        offset = sizeof(header1) + header1.header2Length;
        for (i = 0; i < numTypes; i++) {
            resType = typesInfo[i].type;
            for (j = 1; j <= typesInfo[i].numResources; j++) {
                resources[index-1] = Get1IndResource(resType, j);
                offsets[index-1].offset = offset;
                offset += offsets[index-1].size = GetHandleSize(resources[index-1]);
                offsets[index-1].unknown = 0;

                resourcesInfo[index-1].index = CFSwapInt32HostToBig(index);
                GetResInfo(resources[index-1], &rID, &resType, resourceName);
                resourcesInfo[index-1].type = CFSwapInt32HostToBig(resType);
                resourcesInfo[index-1].id = CFSwapInt16HostToBig(rID);
                memset(&resourcesInfo[index-1].name, 0, sizeof(resourcesInfo[index-1].name));
                memcpy(&resourcesInfo[index-1].name, resourceName+1, resourceName[0]);
                
                index++;
            }
            typesInfo[i].type = CFSwapInt32HostToBig(typesInfo[i].type);
            typesInfo[i].numResources = CFSwapInt32HostToBig(typesInfo[i].numResources);
        }
        offsets[index-1].offset = offset;
        offsets[index-1].size = sizeof(mapHeader) + sizeof(typesInfo) + RezInfoSize*numResources;
        offsets[index-1].unknown = sizeof(offsets) + 12;

        FILE *fp = fopen(rez, "w+b");
        if (fp != NULL) {
            fwrite(&header1, sizeof(header1), 1, fp);
            fwrite(&header2, sizeof(header2), 1, fp);
            fwrite(&offsets, sizeof(offsets), 1, fp);
            fwrite(&RezMapName, sizeof(RezMapName), 1, fp);
            for (i = 0; i < numResources; i++) {
                fwrite(*resources[i], GetHandleSize(resources[i]), 1, fp);
                ReleaseResource(resources[i]);
            }
            fwrite(&mapHeader, sizeof(mapHeader), 1, fp);
            fwrite(&typesInfo, sizeof(typesInfo), 1, fp);
            for (i = 0; i < numResources; i++) {
                fwrite(&resourcesInfo[i], RezInfoSize, 1, fp);
            }
            fclose(fp);
        } else {
            perror("Failed to open output file");
            errorCode = errno;
        }

        CloseResFile(inputRefNum);
    } else {
        fprintf(stderr, "Failed to open resource map in input data fork\n");
    }

    return errorCode;
}

OSErr rez2npif(char *rez, FSRef npifDir, HFSUniStr255 npifName) {
    OSErr           errorCode;
    HFSUniStr255    forkName;
    FSCatalogInfo   catInfo = {0};
    FSRef           npif;
    ResFileRefNum   outputRefNum;
    UInt32          i, index, numResources;
    RezHeader1      header1;
    RezHeader2      header2;
    RezTypeInfo     typeInfo;
    RezResourceInfo resourceInfo;
    Str255          resourceName;

    FILE *fp = fopen(rez, "rb");
    if (fp != NULL) {
        fread(&header1, sizeof(header1), 1, fp);
        if (CFSwapInt32BigToHost(header1.signature) == RezSignature && header1.version == RezVersion) {
            fread(&header2, sizeof(header2), 1, fp);
            RezOffset offsets[header2.numEntries];
            numResources = header2.numEntries - 1;
            Handle resources[numResources];
            fread(&offsets, sizeof(offsets), 1, fp);
            for (i = 0; i < numResources; i++) {
                resources[i] = NewHandle(offsets[i].size);
                fseek(fp, offsets[i].offset, SEEK_SET);
                fread(*resources[i], offsets[i].size, 1, fp);
            }

            FSGetResourceForkName(&forkName);
            ((FileInfo *)&catInfo.finderInfo)->fileType = 'Np\225f';
            ((FileInfo *)&catInfo.finderInfo)->fileCreator = 'N\232v\212';
            errorCode = FSCreateResourceFile(&npifDir, npifName.length, npifName.unicode, kFSCatInfoFinderInfo, &catInfo, forkName.length, forkName.unicode, &npif, NULL);
            if (!errorCode) {
                errorCode = FSOpenResourceFile(&npif, forkName.length, forkName.unicode, fsWrPerm, &outputRefNum);
            }
            if (!errorCode) {
                UseResFile(outputRefNum);

                RezMapHeader mapHeader;
                fseek(fp, offsets[i].offset, SEEK_SET);
                fread(&mapHeader, sizeof(mapHeader), 1, fp);
                for (i = 0; i < CFSwapInt32BigToHost(mapHeader.numTypes); i++) {
                    fread(&typeInfo, sizeof(typeInfo), 1, fp);
                    CoreEndianInstallFlipper(kCoreEndianResourceManagerDomain, CFSwapInt32BigToHost(typeInfo.type), FlipperNoFlipping, NULL);
                }
                for (i = 0; i < numResources; i++) {
                    fread(&resourceInfo, RezInfoSize, 1, fp);
                    index = CFSwapInt32BigToHost(resourceInfo.index) - header2.firstIndex;
                    resourceName[0] = strlen(resourceInfo.name);
                    memcpy(&resourceName[1], resourceInfo.name, resourceName[0]);
                    AddResource(resources[index], CFSwapInt32BigToHost(resourceInfo.type), CFSwapInt16BigToHost(resourceInfo.id), resourceName);
                }

                UpdateResFile(outputRefNum);
                CloseResFile(outputRefNum);
            } else {
                fprintf(stderr, "Failed to create resource map in output file\n");
            }
        } else {
            fprintf(stderr, "Input not a valid .rez file\n");
            errorCode = paramErr;
        }
        fclose(fp);
    } else {
        perror("Failed to open input file");
        errorCode = errno;
    }

    return errorCode;
}

int main (int argc, const char * argv[]) {
    OSErr   errorCode;
    UInt8   *npifPath;
    char    *rezPath, *format;
    FSRef   ref;
    Boolean check;
    
    if (argc < 3) {
        printf("Usage:\nplugconvert -mac <input plugin.rez> <output mac plugin>\nplugconvert -win <input mac plugin> <output plugin.rez>\n");
        return noErr;
    }

    format = (char *)argv[1];

    if (strcmp(format, "-win") == 0) {
        npifPath = (UInt8 *)argv[2];
        rezPath = (char *)argv[3];
        errorCode = FSPathMakeRef(npifPath, &ref, &check);
        if (errorCode) {
            fprintf(stderr, "Input file not found\n");
            errorCode = paramErr;
        } else if (check) {
            fprintf(stderr, "Input file is a directory\n");
            errorCode = paramErr;
        } else {
            errorCode = npif2rez(ref, rezPath);
        }
    } else if (strcmp(format, "-mac") == 0) {
        rezPath = (char *)argv[2];
        npifPath = (UInt8 *)argv[3];
        HFSUniStr255 npifName;
        CFURLRef npifUrl = CFURLCreateFromFileSystemRepresentation(kCFAllocatorDefault, npifPath, strlen((char *)npifPath), false);
        FSGetHFSUniStrFromString(CFURLCopyLastPathComponent(npifUrl), &npifName);
        check = CFURLGetFSRef(CFURLCreateCopyDeletingLastPathComponent(kCFAllocatorDefault, npifUrl), &ref);
        if (!check) {
            fprintf(stderr, "Directory of output file not found\n");
            errorCode = paramErr;
        } else {
            errorCode = rez2npif(rezPath, ref, npifName);
        }
    } else {
        fprintf(stderr, "Invalid format\n");
        errorCode = paramErr;
    }

    return errorCode;
}
