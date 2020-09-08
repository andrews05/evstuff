#import <Carbon/Carbon.h>
#import <Foundation/Foundation.h>

NSEnumerator<NSURL *> *enumerator = nil;

OSErr FSOpenIterator(const FSRef *container, FSIteratorFlags iteratorFlags, FSIterator *iterator) {
    NSURL *url = (__bridge_transfer NSURL*)CFURLCreateFromFSRef(NULL, container);
    if (!url) return nsvErr;
    NSArray *urls = [NSFileManager.defaultManager contentsOfDirectoryAtURL:url
                                                includingPropertiesForKeys:nil
                                                                   options:NSDirectoryEnumerationSkipsHiddenFiles
                                                                     error:nil];
    NSArray *descriptors = @[[NSSortDescriptor sortDescriptorWithKey:@"lastPathComponent" ascending:YES]];
    enumerator = [urls sortedArrayUsingDescriptors:descriptors].objectEnumerator;
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
