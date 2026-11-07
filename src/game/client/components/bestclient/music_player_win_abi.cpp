/* ChainsawClient: what is playing on Windows without C++/WinRT (see music_player_win_abi.h). The Windows COM
   headers live only in this file: their IStorage would clash with the one of the engine. */
#if defined(_WIN32)

#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#include <hstring.h>
#include <inspectable.h>
#include <objidl.h>

#include <cstdint>
#include <string>
#include <vector>

#include "music_player_win_abi.h"

namespace BcWinMedia
{
// ---- the raw interfaces (vtable order after IInspectable, as in the metadata) ----
struct IAsyncInfoAbi : public IInspectable
{
	virtual HRESULT STDMETHODCALLTYPE get_Id(UINT32 *pId) = 0;
	virtual HRESULT STDMETHODCALLTYPE get_Status(INT32 *pStatus) = 0; // 0 started, 1 completed, 2 canceled, 3 error
	virtual HRESULT STDMETHODCALLTYPE get_ErrorCode(HRESULT *pError) = 0;
	virtual HRESULT STDMETHODCALLTYPE Cancel() = 0;
	virtual HRESULT STDMETHODCALLTYPE Close() = 0;
};
// IAsyncOperation<T> (its own id depends on T, it is never asked for)
struct IAsyncOpAbi : public IInspectable
{
	virtual HRESULT STDMETHODCALLTYPE put_Completed(void *pHandler) = 0;
	virtual HRESULT STDMETHODCALLTYPE get_Completed(void **ppHandler) = 0;
	virtual HRESULT STDMETHODCALLTYPE GetResults(void *pResult) = 0;
};
// IVectorView<T> / IReadOnlyList<T>
struct IVectorViewAbi : public IInspectable
{
	virtual HRESULT STDMETHODCALLTYPE GetAt(UINT32 Index, void **ppItem) = 0;
	virtual HRESULT STDMETHODCALLTYPE get_Size(UINT32 *pSize) = 0;
};
struct IStreamRefAbi : public IInspectable
{
	virtual HRESULT STDMETHODCALLTYPE OpenReadAsync(IAsyncOpAbi **ppOp) = 0;
};
struct IControlsAbi : public IInspectable
{
	virtual HRESULT STDMETHODCALLTYPE get_IsPlayEnabled(unsigned char *p) = 0;
	virtual HRESULT STDMETHODCALLTYPE get_IsPauseEnabled(unsigned char *p) = 0;
	virtual HRESULT STDMETHODCALLTYPE get_IsStopEnabled(unsigned char *p) = 0;
	virtual HRESULT STDMETHODCALLTYPE get_IsRecordEnabled(unsigned char *p) = 0;
	virtual HRESULT STDMETHODCALLTYPE get_IsFastForwardEnabled(unsigned char *p) = 0;
	virtual HRESULT STDMETHODCALLTYPE get_IsRewindEnabled(unsigned char *p) = 0;
	virtual HRESULT STDMETHODCALLTYPE get_IsNextEnabled(unsigned char *p) = 0;
	virtual HRESULT STDMETHODCALLTYPE get_IsPreviousEnabled(unsigned char *p) = 0;
	virtual HRESULT STDMETHODCALLTYPE get_IsChannelUpEnabled(unsigned char *p) = 0;
	virtual HRESULT STDMETHODCALLTYPE get_IsChannelDownEnabled(unsigned char *p) = 0;
	virtual HRESULT STDMETHODCALLTYPE get_IsPlayPauseToggleEnabled(unsigned char *p) = 0;
};
struct IPlaybackInfoAbi : public IInspectable
{
	virtual HRESULT STDMETHODCALLTYPE get_Controls(IControlsAbi **pp) = 0;
	virtual HRESULT STDMETHODCALLTYPE get_PlaybackStatus(INT32 *p) = 0; // 0 closed, 1 opened, 2 changing, 3 stopped, 4 playing, 5 paused
};
struct ITimelineAbi : public IInspectable
{
	virtual HRESULT STDMETHODCALLTYPE get_StartTime(INT64 *p) = 0; // TimeSpan, 100 ns
	virtual HRESULT STDMETHODCALLTYPE get_EndTime(INT64 *p) = 0;
	virtual HRESULT STDMETHODCALLTYPE get_MinSeekTime(INT64 *p) = 0;
	virtual HRESULT STDMETHODCALLTYPE get_MaxSeekTime(INT64 *p) = 0;
	virtual HRESULT STDMETHODCALLTYPE get_Position(INT64 *p) = 0;
	virtual HRESULT STDMETHODCALLTYPE get_LastUpdatedTime(INT64 *p) = 0; // DateTime, 100 ns since 1601
};
struct IMediaPropsAbi : public IInspectable
{
	virtual HRESULT STDMETHODCALLTYPE get_Title(HSTRING *p) = 0;
	virtual HRESULT STDMETHODCALLTYPE get_Subtitle(HSTRING *p) = 0;
	virtual HRESULT STDMETHODCALLTYPE get_AlbumArtist(HSTRING *p) = 0;
	virtual HRESULT STDMETHODCALLTYPE get_Artist(HSTRING *p) = 0;
	virtual HRESULT STDMETHODCALLTYPE get_AlbumTitle(HSTRING *p) = 0;
	virtual HRESULT STDMETHODCALLTYPE get_TrackNumber(INT32 *p) = 0;
	virtual HRESULT STDMETHODCALLTYPE get_Genres(void **pp) = 0;
	virtual HRESULT STDMETHODCALLTYPE get_AlbumTrackCount(INT32 *p) = 0;
	virtual HRESULT STDMETHODCALLTYPE get_PlaybackType(void **pp) = 0;
	virtual HRESULT STDMETHODCALLTYPE get_Thumbnail(IStreamRefAbi **pp) = 0;
};
struct ISessionAbi : public IInspectable
{
	virtual HRESULT STDMETHODCALLTYPE get_SourceAppUserModelId(HSTRING *p) = 0;
	virtual HRESULT STDMETHODCALLTYPE TryGetMediaPropertiesAsync(IAsyncOpAbi **pp) = 0;
	virtual HRESULT STDMETHODCALLTYPE GetTimelineProperties(ITimelineAbi **pp) = 0;
	virtual HRESULT STDMETHODCALLTYPE GetPlaybackInfo(IPlaybackInfoAbi **pp) = 0;
	virtual HRESULT STDMETHODCALLTYPE TryPlayAsync(IAsyncOpAbi **pp) = 0;
	virtual HRESULT STDMETHODCALLTYPE TryPauseAsync(IAsyncOpAbi **pp) = 0;
	virtual HRESULT STDMETHODCALLTYPE TryStopAsync(IAsyncOpAbi **pp) = 0;
	virtual HRESULT STDMETHODCALLTYPE TryRecordAsync(IAsyncOpAbi **pp) = 0;
	virtual HRESULT STDMETHODCALLTYPE TryFastForwardAsync(IAsyncOpAbi **pp) = 0;
	virtual HRESULT STDMETHODCALLTYPE TryRewindAsync(IAsyncOpAbi **pp) = 0;
	virtual HRESULT STDMETHODCALLTYPE TrySkipNextAsync(IAsyncOpAbi **pp) = 0;
	virtual HRESULT STDMETHODCALLTYPE TrySkipPreviousAsync(IAsyncOpAbi **pp) = 0;
	virtual HRESULT STDMETHODCALLTYPE TryChangeChannelUpAsync(IAsyncOpAbi **pp) = 0;
	virtual HRESULT STDMETHODCALLTYPE TryChangeChannelDownAsync(IAsyncOpAbi **pp) = 0;
	virtual HRESULT STDMETHODCALLTYPE TryTogglePlayPauseAsync(IAsyncOpAbi **pp) = 0;
};
struct IManagerAbi : public IInspectable
{
	virtual HRESULT STDMETHODCALLTYPE GetCurrentSession(ISessionAbi **pp) = 0;
	virtual HRESULT STDMETHODCALLTYPE GetSessions(IVectorViewAbi **pp) = 0;
};
struct IManagerStaticsAbi : public IInspectable
{
	virtual HRESULT STDMETHODCALLTYPE RequestAsync(IAsyncOpAbi **pp) = 0;
};

// {00000036-0000-0000-C000-000000000046}
static const GUID IID_AsyncInfo = {0x00000036, 0x0000, 0x0000, {0xC0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x46}};
// {2050C4EE-11A0-57DE-AED7-C97C70338245}
static const GUID IID_ManagerStatics = {0x2050C4EE, 0x11A0, 0x57DE, {0xAE, 0xD7, 0xC9, 0x7C, 0x70, 0x33, 0x82, 0x45}};
// IStream {0000000C-0000-0000-C000-000000000046}
static const GUID IID_StreamCom = {0x0000000C, 0x0000, 0x0000, {0xC0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x46}};

class CClientImpl
{
	typedef HRESULT(WINAPI *FRoInitialize)(int);
	typedef void(WINAPI *FRoUninitialize)();
	typedef HRESULT(WINAPI *FRoGetActivationFactory)(HSTRING, REFIID, void **);
	typedef HRESULT(WINAPI *FWindowsCreateString)(LPCWSTR, UINT32, HSTRING *);
	typedef HRESULT(WINAPI *FWindowsDeleteString)(HSTRING);
	typedef PCWSTR(WINAPI *FWindowsGetStringRawBuffer)(HSTRING, UINT32 *);
	typedef HRESULT(WINAPI *FCreateStreamOverRandomAccessStream)(IUnknown *, REFIID, void **);

	HMODULE m_Combase = nullptr;
	HMODULE m_Shcore = nullptr;
	FRoInitialize m_pRoInitialize = nullptr;
	FRoUninitialize m_pRoUninitialize = nullptr;
	FRoGetActivationFactory m_pRoGetActivationFactory = nullptr;
	FWindowsCreateString m_pWindowsCreateString = nullptr;
	FWindowsDeleteString m_pWindowsDeleteString = nullptr;
	FWindowsGetStringRawBuffer m_pWindowsGetStringRawBuffer = nullptr;
	FCreateStreamOverRandomAccessStream m_pCreateStreamOverRandomAccessStream = nullptr;
	bool m_RoInit = false;
	IManagerAbi *m_pManager = nullptr;
	std::string m_LastArtKey;
	std::vector<unsigned char> m_vLastArt;

	template<class T>
	static void SafeRelease(T *&p)
	{
		if(p)
			p->Release();
		p = nullptr;
	}

	std::string ToUtf8(HSTRING Str) const
	{
		if(!Str)
			return {};
		UINT32 Len = 0;
		const wchar_t *pW = m_pWindowsGetStringRawBuffer(Str, &Len);
		std::string Out;
		if(pW && Len > 0)
		{
			const int Need = WideCharToMultiByte(CP_UTF8, 0, pW, (int)Len, nullptr, 0, nullptr, nullptr);
			if(Need > 0)
			{
				Out.resize(Need);
				WideCharToMultiByte(CP_UTF8, 0, pW, (int)Len, Out.data(), Need, nullptr, nullptr);
			}
		}
		m_pWindowsDeleteString(Str);
		return Out;
	}

	// waits for an async operation and takes its result (an interface); false = it failed or took too long
	static bool Wait(IAsyncOpAbi *pOp, void **ppResult, int TimeoutMs)
	{
		*ppResult = nullptr;
		if(!pOp)
			return false;
		IAsyncInfoAbi *pInfo = nullptr;
		if(FAILED(pOp->QueryInterface(IID_AsyncInfo, (void **)&pInfo)) || !pInfo)
			return false;
		INT32 Status = 0;
		for(int Waited = 0; Waited <= TimeoutMs; Waited += 2)
		{
			if(FAILED(pInfo->get_Status(&Status)) || Status != 0)
				break;
			Sleep(2);
		}
		pInfo->Release();
		if(Status != 1)
			return false;
		return SUCCEEDED(pOp->GetResults(ppResult)) && *ppResult != nullptr;
	}

	// a running action (play, pause, skip): let it finish, the result does not matter
	static void Finish(IAsyncOpAbi *pOp)
	{
		if(!pOp)
			return;
		IAsyncInfoAbi *pInfo = nullptr;
		if(SUCCEEDED(pOp->QueryInterface(IID_AsyncInfo, (void **)&pInfo)) && pInfo)
		{
			INT32 Status = 0;
			for(int Waited = 0; Waited <= 1000; Waited += 2)
			{
				if(FAILED(pInfo->get_Status(&Status)) || Status != 0)
					break;
				Sleep(2);
			}
			pInfo->Release();
		}
		pOp->Release();
	}

	std::vector<unsigned char> ReadArt(IStreamRefAbi *pRef)
	{
		std::vector<unsigned char> vBytes;
		if(!pRef || !m_pCreateStreamOverRandomAccessStream)
			return vBytes;
		IAsyncOpAbi *pOp = nullptr;
		IUnknown *pStream = nullptr;
		if(SUCCEEDED(pRef->OpenReadAsync(&pOp)) && Wait(pOp, (void **)&pStream, 1500))
		{
			IStream *pCom = nullptr;
			if(SUCCEEDED(m_pCreateStreamOverRandomAccessStream(pStream, IID_StreamCom, (void **)&pCom)) && pCom)
			{
				unsigned char aChunk[16384];
				ULONG Read = 0;
				while(SUCCEEDED(pCom->Read(aChunk, sizeof(aChunk), &Read)) && Read > 0)
				{
					vBytes.insert(vBytes.end(), aChunk, aChunk + Read);
					if(vBytes.size() > 8u * 1024u * 1024u)
					{
						vBytes.clear();
						break;
					}
				}
				pCom->Release();
			}
		}
		SafeRelease(pStream);
		SafeRelease(pOp);
		return vBytes;
	}

	bool Query(ISessionAbi *pSession, const std::string *pArtFor, STrack &Out)
	{
		Out = STrack();
		HSTRING Str = nullptr;
		if(SUCCEEDED(pSession->get_SourceAppUserModelId(&Str)))
			Out.m_SourceId = ToUtf8(Str);

		IPlaybackInfoAbi *pInfo = nullptr;
		if(SUCCEEDED(pSession->GetPlaybackInfo(&pInfo)) && pInfo)
		{
			INT32 Status = 0;
			pInfo->get_PlaybackStatus(&Status);
			Out.m_Status = Status;
			IControlsAbi *pControls = nullptr;
			if(SUCCEEDED(pInfo->get_Controls(&pControls)) && pControls)
			{
				unsigned char Play = 0, Pause = 0, Next = 0, Prev = 0, Toggle = 0;
				pControls->get_IsPlayEnabled(&Play);
				pControls->get_IsPauseEnabled(&Pause);
				pControls->get_IsNextEnabled(&Next);
				pControls->get_IsPreviousEnabled(&Prev);
				pControls->get_IsPlayPauseToggleEnabled(&Toggle);
				Out.m_CanNext = Next != 0;
				Out.m_CanPrev = Prev != 0;
				Out.m_CanPlayPause = Play != 0 || Pause != 0 || Toggle != 0;
				pControls->Release();
			}
			pInfo->Release();
		}
		else
			return false;

		ITimelineAbi *pTimeline = nullptr;
		if(SUCCEEDED(pSession->GetTimelineProperties(&pTimeline)) && pTimeline)
		{
			INT64 Start = 0, End = 0, Position = 0, Updated = 0;
			pTimeline->get_StartTime(&Start);
			pTimeline->get_EndTime(&End);
			pTimeline->get_Position(&Position);
			pTimeline->get_LastUpdatedTime(&Updated);
			Out.m_StartMs = Start / 10000;
			Out.m_EndMs = End / 10000;
			Out.m_PositionMs = Position / 10000;
			Out.m_LastUpdated = Updated;
			pTimeline->Release();
		}

		IAsyncOpAbi *pOp = nullptr;
		IMediaPropsAbi *pProps = nullptr;
		if(SUCCEEDED(pSession->TryGetMediaPropertiesAsync(&pOp)) && Wait(pOp, (void **)&pProps, 1500))
		{
			if(SUCCEEDED(pProps->get_Title(&Str)))
				Out.m_Title = ToUtf8(Str);
			if(SUCCEEDED(pProps->get_Artist(&Str)))
				Out.m_Artist = ToUtf8(Str);
			if(SUCCEEDED(pProps->get_AlbumTitle(&Str)))
				Out.m_Album = ToUtf8(Str);
			if(SUCCEEDED(pProps->get_Subtitle(&Str)))
				Out.m_Subtitle = ToUtf8(Str);
			if(SUCCEEDED(pProps->get_AlbumArtist(&Str)))
				Out.m_AlbumArtist = ToUtf8(Str);
			if(pArtFor != nullptr && *pArtFor == Out.m_SourceId)
			{
				// the picture only when the track changes (it is read from the app every time otherwise)
				const std::string Key = Out.m_SourceId + "|" + Out.m_Title + "|" + Out.m_Artist;
				if(Key != m_LastArtKey)
				{
					IStreamRefAbi *pThumb = nullptr;
					if(SUCCEEDED(pProps->get_Thumbnail(&pThumb)) && pThumb)
					{
						m_vLastArt = ReadArt(pThumb);
						pThumb->Release();
					}
					else
						m_vLastArt.clear();
					m_LastArtKey = Key;
				}
				Out.m_vArt = m_vLastArt;
			}
		}
		SafeRelease(pProps);
		SafeRelease(pOp);
		return true;
	}

public:
	~CClientImpl() { Shutdown(); }

	// on the thread that will use it
	bool Init()
	{
		if(m_pManager)
			return true;
		if(!m_Combase)
		{
			m_Combase = LoadLibraryW(L"combase.dll");
			if(!m_Combase)
				return false;
			m_pRoInitialize = (FRoInitialize)(void *)GetProcAddress(m_Combase, "RoInitialize");
			m_pRoUninitialize = (FRoUninitialize)(void *)GetProcAddress(m_Combase, "RoUninitialize");
			m_pRoGetActivationFactory = (FRoGetActivationFactory)(void *)GetProcAddress(m_Combase, "RoGetActivationFactory");
			m_pWindowsCreateString = (FWindowsCreateString)(void *)GetProcAddress(m_Combase, "WindowsCreateString");
			m_pWindowsDeleteString = (FWindowsDeleteString)(void *)GetProcAddress(m_Combase, "WindowsDeleteString");
			m_pWindowsGetStringRawBuffer = (FWindowsGetStringRawBuffer)(void *)GetProcAddress(m_Combase, "WindowsGetStringRawBuffer");
			m_Shcore = LoadLibraryW(L"shcore.dll");
			if(m_Shcore)
				m_pCreateStreamOverRandomAccessStream = (FCreateStreamOverRandomAccessStream)(void *)GetProcAddress(m_Shcore, "CreateStreamOverRandomAccessStream");
		}
		if(!m_pRoInitialize || !m_pRoGetActivationFactory || !m_pWindowsCreateString || !m_pWindowsDeleteString || !m_pWindowsGetStringRawBuffer)
			return false;
		if(!m_RoInit)
		{
			const HRESULT Res = m_pRoInitialize(1); // RO_INIT_MULTITHREADED
			m_RoInit = SUCCEEDED(Res) || Res == RPC_E_CHANGED_MODE;
			if(!m_RoInit)
				return false;
		}
		static const wchar_t s_aClass[] = L"Windows.Media.Control.GlobalSystemMediaTransportControlsSessionManager";
		HSTRING ClassName = nullptr;
		if(FAILED(m_pWindowsCreateString(s_aClass, (UINT32)(sizeof(s_aClass) / sizeof(s_aClass[0]) - 1), &ClassName)))
			return false;
		IManagerStaticsAbi *pStatics = nullptr;
		const HRESULT Res = m_pRoGetActivationFactory(ClassName, IID_ManagerStatics, (void **)&pStatics);
		m_pWindowsDeleteString(ClassName);
		if(FAILED(Res) || !pStatics)
			return false;
		IAsyncOpAbi *pOp = nullptr;
		if(SUCCEEDED(pStatics->RequestAsync(&pOp)))
			Wait(pOp, (void **)&m_pManager, 3000);
		SafeRelease(pOp);
		pStatics->Release();
		return m_pManager != nullptr;
	}

	void Shutdown()
	{
		SafeRelease(m_pManager);
		if(m_RoInit && m_pRoUninitialize)
			m_pRoUninitialize();
		m_RoInit = false;
	}

	// every media session that plays or is paused; SystemCurrent = the one Windows itself shows
	bool Sessions(std::vector<STrack> &vOut, std::string &SystemCurrent, bool WithArt)
	{
		vOut.clear();
		SystemCurrent.clear();
		if(!Init())
			return false;
		ISessionAbi *pCurrent = nullptr;
		if(SUCCEEDED(m_pManager->GetCurrentSession(&pCurrent)) && pCurrent)
		{
			HSTRING Str = nullptr;
			if(SUCCEEDED(pCurrent->get_SourceAppUserModelId(&Str)))
				SystemCurrent = ToUtf8(Str);
			pCurrent->Release();
		}
		IVectorViewAbi *pList = nullptr;
		if(FAILED(m_pManager->GetSessions(&pList)) || !pList)
		{
			// the manager went bad (explorer restarted ...): a new one next time
			SafeRelease(m_pManager);
			return false;
		}
		UINT32 Count = 0;
		pList->get_Size(&Count);
		for(UINT32 i = 0; i < Count && i < 16; i++)
		{
			ISessionAbi *pSession = nullptr;
			if(FAILED(pList->GetAt(i, (void **)&pSession)) || !pSession)
				continue;
			STrack Track;
			// the picture only for the session Windows shows (that is almost always the one played)
			if(Query(pSession, WithArt ? &SystemCurrent : nullptr, Track))
				vOut.push_back(std::move(Track));
			pSession->Release();
		}
		pList->Release();
		return true;
	}

	void Action(const std::string &SourceId, int Kind)
	{
		if(!Init())
			return;
		IVectorViewAbi *pList = nullptr;
		ISessionAbi *pTarget = nullptr;
		if(!SourceId.empty() && SUCCEEDED(m_pManager->GetSessions(&pList)) && pList)
		{
			UINT32 Count = 0;
			pList->get_Size(&Count);
			for(UINT32 i = 0; i < Count && !pTarget; i++)
			{
				ISessionAbi *pSession = nullptr;
				if(FAILED(pList->GetAt(i, (void **)&pSession)) || !pSession)
					continue;
				HSTRING Str = nullptr;
				if(SUCCEEDED(pSession->get_SourceAppUserModelId(&Str)) && ToUtf8(Str) == SourceId)
					pTarget = pSession;
				else
					pSession->Release();
			}
			pList->Release();
		}
		if(!pTarget)
			m_pManager->GetCurrentSession(&pTarget);
		if(!pTarget)
			return;
		IAsyncOpAbi *pOp = nullptr;
		HRESULT Res = E_FAIL;
		if(Kind == ACTION_PREVIOUS)
			Res = pTarget->TrySkipPreviousAsync(&pOp);
		else if(Kind == ACTION_NEXT)
			Res = pTarget->TrySkipNextAsync(&pOp);
		else
			Res = pTarget->TryTogglePlayPauseAsync(&pOp);
		if(SUCCEEDED(Res))
			Finish(pOp);
		pTarget->Release();
	}
};
}


namespace BcWinMedia
{
CClient::CClient() :
	m_pImpl(new CClientImpl())
{
}
CClient::~CClient() { delete m_pImpl; }
bool CClient::Init() { return m_pImpl->Init(); }
void CClient::Shutdown() { m_pImpl->Shutdown(); }
bool CClient::Sessions(std::vector<STrack> &vOut, std::string &SystemCurrent, bool WithArt) { return m_pImpl->Sessions(vOut, SystemCurrent, WithArt); }
void CClient::Action(const std::string &SourceId, int Kind) { m_pImpl->Action(SourceId, Kind); }
}
#endif
