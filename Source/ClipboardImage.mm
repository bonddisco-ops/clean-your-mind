// macOS clipboard picture reader (see ClipboardImage.cpp)
#import <AppKit/AppKit.h>
#include <juce_gui_basics/juce_gui_basics.h>

using namespace juce;

Image readClipboardImage()
{
    @autoreleasepool
    {
        NSPasteboard* pb = [NSPasteboard generalPasteboard];

        NSArray* urls = [pb readObjectsForClasses: @[ [NSURL class] ]
                                          options: @{ NSPasteboardURLReadingFileURLsOnlyKey: @YES }];
        if (urls != nil && urls.count > 0)
        {
            NSURL* u = urls[0];
            auto img = ImageFileFormat::loadFrom (File (String::fromUTF8 (u.path.UTF8String)));
            if (img.isValid())
                return img;
        }

        if (NSData* d = [pb dataForType: NSPasteboardTypePNG])
        {
            auto img = ImageFileFormat::loadFrom (d.bytes, (size_t) d.length);
            if (img.isValid())
                return img;
        }

        NSImage* ns = [[NSImage alloc] initWithPasteboard: pb];
        if (ns != nil)
        {
            Image result;
            NSData* tiff = [ns TIFFRepresentation];
            NSBitmapImageRep* rep = [NSBitmapImageRep imageRepWithData: tiff];
            NSData* png = [rep representationUsingType: NSBitmapImageFileTypePNG properties: @{}];
            if (png != nil)
                result = ImageFileFormat::loadFrom (png.bytes, (size_t) png.length);
           #if ! __has_feature(objc_arc)
            [ns release];
           #endif
            return result;
        }
    }
    return {};
}
