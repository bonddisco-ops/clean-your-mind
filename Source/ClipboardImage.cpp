// Reads a picture from the system clipboard (a screenshot, a copied image or a copied image file).
// JUCE only does text clipboard, so this talks to the OS directly. The macOS version is ClipboardImage.mm.

#if defined (_WIN32)
 #ifndef NOMINMAX
  #define NOMINMAX
 #endif
 #include <windows.h>
 #include <shellapi.h>
 #pragma comment (lib, "shell32.lib")
 #pragma comment (lib, "user32.lib")
#endif

#include <juce_gui_basics/juce_gui_basics.h>

using namespace juce;

#if defined (_WIN32)

static Image imageFromDib (const uint8* data, size_t size)
{
    if (size < sizeof (BITMAPINFOHEADER))
        return {};
    const auto* bi = reinterpret_cast<const BITMAPINFOHEADER*> (data);
    const int w = bi->biWidth;
    const int hRaw = bi->biHeight;
    const int h = std::abs (hRaw);
    const int bpp = bi->biBitCount;
    if (w <= 0 || h <= 0 || (bpp != 24 && bpp != 32) || (bi->biCompression != BI_RGB && bi->biCompression != BI_BITFIELDS))
        return {};
    size_t offset = bi->biSize;
    if (bi->biCompression == BI_BITFIELDS && bi->biSize == sizeof (BITMAPINFOHEADER))
        offset += 12;
    offset += (size_t) bi->biClrUsed * 4;
    const size_t stride = (size_t) (((w * bpp) + 31) / 32) * 4;
    if (offset + stride * (size_t) h > size)
        return {};

    Image img (Image::RGB, w, h, false);
    Image::BitmapData bd (img, Image::BitmapData::writeOnly);
    for (int y = 0; y < h; ++y)
    {
        const int srcRow = hRaw > 0 ? (h - 1 - y) : y;
        const auto* src = data + offset + stride * (size_t) srcRow;
        for (int x = 0; x < w; ++x)
        {
            const auto* px = src + x * (bpp / 8);
            bd.setPixelColour (x, y, Colour (px[2], px[1], px[0]));
        }
    }
    return img;
}

Image readClipboardImage()
{
    if (! OpenClipboard (nullptr))
        return {};
    Image result;

    const UINT png = RegisterClipboardFormatW (L"PNG");
    if (HANDLE hData = GetClipboardData (png))
        if (auto* p = GlobalLock (hData))
        {
            result = ImageFileFormat::loadFrom (p, GlobalSize (hData));
            GlobalUnlock (hData);
        }

    if (! result.isValid())
        if (HANDLE hDrop = GetClipboardData (CF_HDROP))
            if (auto drop = (HDROP) GlobalLock (hDrop))
            {
                if (DragQueryFileW (drop, 0xFFFFFFFF, nullptr, 0) > 0)
                {
                    wchar_t buf[4096] = {};
                    DragQueryFileW (drop, 0, buf, 4095);
                    result = ImageFileFormat::loadFrom (File (String (buf)));
                }
                GlobalUnlock (hDrop);
            }

    if (! result.isValid())
        for (UINT fmt : { (UINT) CF_DIBV5, (UINT) CF_DIB })
        {
            if (HANDLE hDib = GetClipboardData (fmt))
                if (auto* p = GlobalLock (hDib))
                {
                    result = imageFromDib (static_cast<const uint8*> (p), GlobalSize (hDib));
                    GlobalUnlock (hDib);
                }
            if (result.isValid())
                break;
        }

    CloseClipboard();
    return result;
}

#elif ! defined (__APPLE__)

Image readClipboardImage()
{
    // Linux test builds: accept a copied file path as text
    const auto text = SystemClipboard::getTextFromClipboard().trim().fromFirstOccurrenceOf ("file://", false, false);
    if (File::isAbsolutePath (text))
        return ImageFileFormat::loadFrom (File (text));
    return {};
}

#endif
