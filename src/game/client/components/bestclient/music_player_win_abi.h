/* ChainsawClient: what is playing on Windows (Spotify, browsers, Yandex Music ...) for the music player, without
   C++/WinRT. BestClient reads the Windows media sessions (GlobalSystemMediaTransportControls) through C++/WinRT,
   which only the Microsoft compiler has; our MinGW build fell back to a provider that never sees any music, so the
   player always showed "BestClient". music_player_win_abi.cpp talks to the very same Windows API through its raw COM
   interfaces (the interface ids and the method order come from Windows.Media.winmd); combase.dll / shcore.dll are
   loaded at run time, so nothing changes in the linking. Everything runs on the caller's thread (the player's
   worker). This header has no Windows headers in it (their IStorage would clash with the engine's). Windows only. */
#ifndef GAME_CLIENT_COMPONENTS_BESTCLIENT_MUSIC_PLAYER_WIN_ABI_H
#define GAME_CLIENT_COMPONENTS_BESTCLIENT_MUSIC_PLAYER_WIN_ABI_H

#include <cstdint>
#include <string>
#include <vector>

namespace BcWinMedia
{
enum
{
	ACTION_PREVIOUS = 0,
	ACTION_PLAY_PAUSE,
	ACTION_NEXT,
};

struct STrack
{
	std::string m_SourceId; // the app, e.g. Spotify.exe
	std::string m_Title;
	std::string m_Artist;
	std::string m_Album;
	std::string m_Subtitle;
	std::string m_AlbumArtist;
	int64_t m_StartMs = 0;
	int64_t m_EndMs = 0;
	int64_t m_PositionMs = 0;
	int64_t m_LastUpdated = 0; // 100 ns since 1601, when the app last told the position
	int m_Status = 0; // 0 closed, 1 opened, 2 changing, 3 stopped, 4 playing, 5 paused
	bool m_CanPrev = false;
	bool m_CanNext = false;
	bool m_CanPlayPause = false;
	std::vector<unsigned char> m_vArt; // the cover picture as the app gives it (png / jpeg)
};

class CClientImpl;

class CClient
{
	CClientImpl *m_pImpl;

public:
	CClient();
	~CClient();
	CClient(const CClient &) = delete;
	CClient &operator=(const CClient &) = delete;

	// on the thread that will use it; false = the media service is not there (try again later)
	bool Init();
	void Shutdown();
	// every media session; SystemCurrent = the one Windows itself shows (its cover is read when the track changes)
	bool Sessions(std::vector<STrack> &vOut, std::string &SystemCurrent, bool WithArt);
	// ACTION_* on the session SourceId (or the one Windows shows)
	void Action(const std::string &SourceId, int Kind);
};
}

#endif
