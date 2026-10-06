#import <Carbon/Carbon.h>
#import <CoreFoundation/CoreFoundation.h>
#include <stdio.h>

static const char *stringValue(TISInputSourceRef source, CFStringRef key,
                               char *buffer, size_t size) {
    CFStringRef value = (CFStringRef)TISGetInputSourceProperty(source, key);
    if (value == NULL || CFGetTypeID(value) != CFStringGetTypeID()) {
        return "?";
    }
    return CFStringGetCString(value, buffer, size, kCFStringEncodingUTF8)
               ? buffer
               : "?";
}

int main(void) {
    char id[256];
    char name[256];

    TISInputSourceRef current = TISCopyCurrentKeyboardInputSource();
    printf("current: %s | %s\n",
           stringValue(current, kTISPropertyInputSourceID, id, sizeof(id)),
           stringValue(current, kTISPropertyLocalizedName, name, sizeof(name)));
    CFRelease(current);

    const void *keys[] = {
        kTISPropertyInputSourceCategory,
        kTISPropertyInputSourceIsEnabled,
        kTISPropertyInputSourceIsSelectCapable,
    };
    const void *values[] = {
        kTISCategoryKeyboardInputSource,
        kCFBooleanTrue,
        kCFBooleanTrue,
    };
    CFDictionaryRef filter = CFDictionaryCreate(
        kCFAllocatorDefault, keys, values, 3,
        &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    CFArrayRef sources = TISCreateInputSourceList(filter, false);
    CFRelease(filter);

    for (CFIndex index = 0; index < CFArrayGetCount(sources); ++index) {
        TISInputSourceRef source =
            (TISInputSourceRef)CFArrayGetValueAtIndex(sources, index);
        printf("source: %s | %s\n",
               stringValue(source, kTISPropertyInputSourceID, id, sizeof(id)),
               stringValue(source, kTISPropertyLocalizedName, name, sizeof(name)));
    }
    CFRelease(sources);
    return 0;
}
