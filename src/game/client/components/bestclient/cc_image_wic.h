/* ChainsawClient: decode any picture with the Windows Imaging Component (WIC).
   The format is found from the bytes, not the file name (a WebP saved as .png works), and WIC knows JPEG, PNG, GIF,
   BMP, TIFF, ICO, JPEG XR and, with the codecs Windows 10/11 bring, WebP, HEIC and AVIF. Phone photos are turned the
   right way (EXIF orientation), CMYK / 16 bit / palette pictures become plain RGBA.
   Kept in its own file without engine headers: the COM headers clash with the engine (IStorage). */
#ifndef GAME_CLIENT_COMPONENTS_BESTCLIENT_CC_IMAGE_WIC_H
#define GAME_CLIENT_COMPONENTS_BESTCLIENT_CC_IMAGE_WIC_H

#include <cstddef>
#include <cstdint>
#include <vector>

namespace CcImage
{
// Decodes pData into RGBA (8 bit per channel, rows top to bottom). A big picture is scaled down while decoding so that it
// still covers CoverW x CoverH (0 = keep the full size). Returns false when Windows can not read it (pError gets why).
bool DecodeWic(const void *pData, size_t DataSize, int CoverW, int CoverH, std::vector<uint8_t> &vRgba, int &Width, int &Height, char *pError = nullptr, int ErrorSize = 0);
} // namespace CcImage

#endif
