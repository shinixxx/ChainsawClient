/* ChainsawClient: decode any picture with the Windows Imaging Component. See cc_image_wic.h. */
#include "cc_image_wic.h"

#if defined(_WIN32)
#include <windows.h>

#include <propidl.h>
#include <wincodec.h>

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace
{
// the GUIDs are written here, so nothing more has to be linked (values from wincodec.h)
const GUID s_ClsidFactory = {0xcacaf262, 0x9370, 0x4615, {0xa1, 0x3b, 0x9f, 0x55, 0x39, 0xda, 0x4c, 0x0a}};
const GUID s_IidFactory = {0xec5ec8a9, 0xc395, 0x4314, {0x9c, 0x77, 0x54, 0xd7, 0xa9, 0x35, 0xff, 0x70}};
const GUID s_PixelBgra = {0x6fddc324, 0x4e03, 0x4bfe, {0xb1, 0x85, 0x3d, 0x77, 0x76, 0x8d, 0xc9, 0x0f}};

template<class T>
struct CCom
{
	T *m_p = nullptr;
	CCom() = default;
	CCom(const CCom &) = delete;
	CCom &operator=(const CCom &) = delete;
	~CCom()
	{
		if(m_p)
			m_p->Release();
	}
	T **Out() { return &m_p; }
	T *operator->() const { return m_p; }
	T *Get() const { return m_p; }
};

void SetError(char *pError, int ErrorSize, const char *pText, HRESULT Hr)
{
	if(pError && ErrorSize > 0)
		std::snprintf(pError, ErrorSize, "%s (0x%08lx)", pText, (unsigned long)Hr);
}

// EXIF orientation 1..8 (phones save photos sideways and write how to turn them)
UINT Orientation(IWICBitmapFrameDecode *pFrame)
{
	CCom<IWICMetadataQueryReader> Reader;
	if(FAILED(pFrame->GetMetadataQueryReader(Reader.Out())) || !Reader.Get())
		return 1;
	const wchar_t *apPaths[] = {L"/app1/ifd/{ushort=274}", L"/ifd/{ushort=274}"};
	for(const wchar_t *pPath : apPaths)
	{
		PROPVARIANT Value;
		PropVariantInit(&Value);
		if(SUCCEEDED(Reader->GetMetadataByName(pPath, &Value)))
		{
			UINT Result = 1;
			if(Value.vt == VT_UI2)
				Result = Value.uiVal;
			else if(Value.vt == VT_UI4)
				Result = Value.ulVal;
			else if(Value.vt == VT_I2)
				Result = (UINT)Value.iVal;
			PropVariantClear(&Value);
			if(Result >= 1 && Result <= 8)
				return Result;
		}
	}
	return 1;
}

WICBitmapTransformOptions Transform(UINT Orient)
{
	switch(Orient)
	{
	case 2: return WICBitmapTransformFlipHorizontal;
	case 3: return WICBitmapTransformRotate180;
	case 4: return WICBitmapTransformFlipVertical;
	case 5: return (WICBitmapTransformOptions)(WICBitmapTransformRotate90 | WICBitmapTransformFlipHorizontal);
	case 6: return WICBitmapTransformRotate90;
	case 7: return (WICBitmapTransformOptions)(WICBitmapTransformRotate270 | WICBitmapTransformFlipHorizontal);
	case 8: return WICBitmapTransformRotate270;
	}
	return WICBitmapTransformRotate0;
}

bool Decode(IWICImagingFactory *pFactory, const void *pData, size_t DataSize, int CoverW, int CoverH, std::vector<uint8_t> &vRgba, int &Width, int &Height, char *pError, int ErrorSize)
{
	CCom<IWICStream> Stream;
	HRESULT Hr = pFactory->CreateStream(Stream.Out());
	if(SUCCEEDED(Hr))
		Hr = Stream->InitializeFromMemory((BYTE *)pData, (DWORD)DataSize);
	if(FAILED(Hr))
	{
		SetError(pError, ErrorSize, "stream", Hr);
		return false;
	}
	CCom<IWICBitmapDecoder> Decoder;
	Hr = pFactory->CreateDecoderFromStream(Stream.Get(), nullptr, WICDecodeMetadataCacheOnDemand, Decoder.Out());
	if(FAILED(Hr))
	{
		SetError(pError, ErrorSize, "unknown format", Hr);
		return false;
	}
	CCom<IWICBitmapFrameDecode> Frame;
	Hr = Decoder->GetFrame(0, Frame.Out());
	UINT W0 = 0, H0 = 0;
	if(SUCCEEDED(Hr))
		Hr = Frame->GetSize(&W0, &H0);
	if(FAILED(Hr) || W0 == 0 || H0 == 0)
	{
		SetError(pError, ErrorSize, "no picture inside", Hr);
		return false;
	}

	IWICBitmapSource *pSource = Frame.Get();
	const UINT Orient = Orientation(Frame.Get());

	// a big photo is scaled down already while decoding: faster and much less memory
	CCom<IWICBitmapScaler> Scaler;
	if(CoverW > 0 && CoverH > 0)
	{
		const bool Swap = Orient >= 5;
		const double Ow = Swap ? H0 : W0, Oh = Swap ? W0 : H0;
		const double Scale = std::max(CoverW / Ow, CoverH / Oh);
		if(Scale < 1.0 && SUCCEEDED(pFactory->CreateBitmapScaler(Scaler.Out())))
		{
			const UINT Sw = std::max<UINT>(1, (UINT)std::lround(W0 * Scale)), Sh = std::max<UINT>(1, (UINT)std::lround(H0 * Scale));
			if(SUCCEEDED(Scaler->Initialize(pSource, Sw, Sh, WICBitmapInterpolationModeFant)))
				pSource = Scaler.Get();
		}
	}
	CCom<IWICBitmapFlipRotator> Rotator;
	if(Orient != 1 && SUCCEEDED(pFactory->CreateBitmapFlipRotator(Rotator.Out())) && SUCCEEDED(Rotator->Initialize(pSource, Transform(Orient))))
		pSource = Rotator.Get();

	CCom<IWICFormatConverter> Converter;
	Hr = pFactory->CreateFormatConverter(Converter.Out());
	if(SUCCEEDED(Hr))
		Hr = Converter->Initialize(pSource, s_PixelBgra, WICBitmapDitherTypeNone, nullptr, 0.0, WICBitmapPaletteTypeCustom);
	UINT W = 0, H = 0;
	if(SUCCEEDED(Hr))
		Hr = Converter->GetSize(&W, &H);
	if(FAILED(Hr) || W == 0 || H == 0 || (uint64_t)W * H > 8192ull * 8192ull)
	{
		SetError(pError, ErrorSize, "can not convert", Hr);
		return false;
	}
	vRgba.resize((size_t)W * H * 4);
	Hr = Converter->CopyPixels(nullptr, W * 4, (UINT)vRgba.size(), vRgba.data());
	if(FAILED(Hr))
	{
		vRgba.clear();
		SetError(pError, ErrorSize, "can not read the pixels", Hr);
		return false;
	}
	for(size_t i = 0; i < vRgba.size(); i += 4)
		std::swap(vRgba[i], vRgba[i + 2]); // BGRA -> RGBA
	Width = (int)W;
	Height = (int)H;
	return true;
}
} // namespace

bool CcImage::DecodeWic(const void *pData, size_t DataSize, int CoverW, int CoverH, std::vector<uint8_t> &vRgba, int &Width, int &Height, char *pError, int ErrorSize)
{
	vRgba.clear();
	Width = Height = 0;
	if(pError && ErrorSize > 0)
		pError[0] = '\0';
	if(!pData || DataSize == 0 || DataSize > 0xffffffffu)
		return false;
	// S_OK / S_FALSE: we balance it below; RPC_E_CHANGED_MODE: COM already runs in another mode, WIC works there too
	const HRESULT InitHr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
	bool Ok = false;
	{
		CCom<IWICImagingFactory> Factory;
		const HRESULT Hr = CoCreateInstance(s_ClsidFactory, nullptr, CLSCTX_INPROC_SERVER, s_IidFactory, (void **)Factory.Out());
		if(SUCCEEDED(Hr))
			Ok = Decode(Factory.Get(), pData, DataSize, CoverW, CoverH, vRgba, Width, Height, pError, ErrorSize);
		else
			SetError(pError, ErrorSize, "no WIC", Hr);
	}
	if(SUCCEEDED(InitHr))
		CoUninitialize();
	return Ok;
}

#else

bool CcImage::DecodeWic(const void *, size_t, int, int, std::vector<uint8_t> &vRgba, int &Width, int &Height, char *pError, int ErrorSize)
{
	vRgba.clear();
	Width = Height = 0;
	if(pError && ErrorSize > 0)
		pError[0] = '\0';
	return false;
}

#endif
