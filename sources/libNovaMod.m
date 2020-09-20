#import <Carbon/Carbon.h>
#import <Foundation/Foundation.h>
#include <dlfcn.h>

NSEnumerator<NSURL *> *enumerator = nil;

OSErr FSOpenIterator(const FSRef *container, FSIteratorFlags iteratorFlags, FSIterator *iterator) {
    CFURLRef urlRef = CFURLCreateFromFSRef(NULL, container);
    if (!urlRef) return nsvErr;
    NSArray *urls = [NSFileManager.defaultManager contentsOfDirectoryAtURL:(__bridge NSURL *)urlRef
                                                includingPropertiesForKeys:nil
                                                                   options:NSDirectoryEnumerationSkipsHiddenFiles
                                                                     error:nil];
    NSArray *descriptors = @[[NSSortDescriptor sortDescriptorWithKey:@"lastPathComponent" ascending:YES]];
    enumerator = [urls sortedArrayUsingDescriptors:descriptors].objectEnumerator;
    CFRelease(urlRef);
    return noErr;
}

OSErr FSCloseIterator(FSIterator iterator) {
    enumerator = nil;
    return noErr;
}

OSErr FSGetCatalogInfoBulk(FSIterator iterator, ItemCount maximumObjects, ItemCount *actualObjects, Boolean *containerChanged, FSCatalogInfoBitmap whichInfo, FSCatalogInfo *catalogInfos, FSRef *refs, FSSpecPtr specs, HFSUniStr255 *names) {
    NSURL *url = nil;
    int i = 0;
    while (maximumObjects-- > 0 && (url = enumerator.nextObject) != nil) {
        CFURLGetFSRef((__bridge CFURLRef)url, &refs[i]);
        FSGetCatalogInfo(&refs[i], whichInfo, &catalogInfos[i], &names[i], &specs[i], NULL);
        i++;
    }
    *actualObjects = i;
    return noErr;
}


void (*original_TextSize) (short) = NULL;

void TextSize(short size) {
    if (!original_TextSize) {
        original_TextSize = dlsym(RTLD_NEXT, "TextSize");
    }
    if (size > 0) {
        NSInteger minSize = [NSUserDefaults.standardUserDefaults integerForKey:@"FontSize"];
        if (minSize > size) size = minSize;
    }
    original_TextSize(size);
}
