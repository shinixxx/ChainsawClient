/* ChainsawClient: the kagune is the hook too (see kagune_hook.h, the motion is in kagune_hook_core.h) */
#include "kagune_hook.h"

#include "own_tee.h"

#include <base/math.h>

#include <engine/shared/config.h>

#include <game/client/gameclient.h>

#include <algorithm>

void CBcKaguneHookFx::OnReset()
{
	for(int i = 0; i < MAX_CLIENTS; i++)
	{
		m_aStates[i] = BcKaguneHook::CState();
		m_aRecords[i] = CRecord();
	}
}

bool CBcKaguneHookFx::ShowFor(int ClientId) const
{
	// part of the kagune: on with it, for the same tees
	return g_Config.m_BcKagune && in_range(ClientId, MAX_CLIENTS - 1) && (g_Config.m_BcKaguneOthers || BcIsOwnTee(GameClient(), ClientId));
}

bool CBcKaguneHookFx::OnHook(int ClientId, vec2 Pos, vec2 HookPos, int HookState, int HookedPlayer, float Alpha, float Size)
{
	if(!ShowFor(ClientId))
		return false;
	(void)Pos;
	(void)Alpha; // the kagune draws with its own alpha and size
	(void)Size;
	CRecord &Rec = m_aRecords[ClientId];
	Rec.m_Time = LocalTime();
	Rec.m_HookPos = HookPos;
	Rec.m_HookState = HookState;
	Rec.m_Hooked = HookedPlayer;
	return true;
}

bool CBcKaguneHookFx::Hooking(int ClientId) const
{
	if(!ShowFor(ClientId))
		return false;
	// CPlayers draws the hook every frame while it is out (after the kagune: the record is one frame old);
	// a single slow frame must not let go of it
	const float Fresh = std::max(0.08f, 3.0f * Client()->RenderFrameTime());
	const CRecord &Rec = m_aRecords[ClientId];
	return Rec.m_Time >= 0.0f && LocalTime() - Rec.m_Time < Fresh;
}

void CBcKaguneHookFx::Pose(int ClientId, BcKagune::CTee &Kagune, const BcKagune::CInput &KaguneIn, BcKagune::CParticles &Particles)
{
	if(!in_range(ClientId, MAX_CLIENTS - 1))
		return;
	BcKaguneHook::CState &State = m_aStates[ClientId];
	const bool Hook = Hooking(ClientId);
	if(!Hook && !BcKaguneHook::Busy(State))
		return;
	const CRecord &Rec = m_aRecords[ClientId];
	BcKaguneHook::CInput In;
	In.m_Pos = KaguneIn.m_Pos;
	In.m_Vel = KaguneIn.m_Vel;
	In.m_Scale = KaguneIn.m_Scale;
	In.m_Dt = KaguneIn.m_Dt;
	In.m_HookState = Hook ? Rec.m_HookState : BcKaguneHook::HOOK_NONE;
	In.m_HookPos = Rec.m_HookPos;
	In.m_HookTee = Hook && in_range(Rec.m_Hooked, MAX_CLIENTS - 1) && GameClient()->m_Snap.m_aCharacters[Rec.m_Hooked].m_Active;
	if(In.m_HookTee)
	{
		// the hooked tee where it is drawn this frame
		In.m_HookTeePos = GameClient()->m_aClients[Rec.m_Hooked].m_RenderPos;
		In.m_HookPos = In.m_HookTeePos;
	}
	In.m_Blood = g_Config.m_BcKaguneBlood && !BcFxSuppressed(GameClient()) ? g_Config.m_BcKaguneBloodAmount / 100.0f : 0.0f;
	In.m_Particles = BcFxSuppressed(GameClient()) ? 0.0f : g_Config.m_BcKaguneParticles / 100.0f;
	In.m_Style = std::clamp(g_Config.m_BcKaguneStyle, 0, BcKagune::NUM_STYLES - 1);
	const auto Ray = [&](vec2 From, vec2 To, vec2 &Hit) {
		vec2 Before;
		return Collision()->IntersectLine(From, To, &Hit, &Before) != 0;
	};
	BcKaguneHook::Pose(State, Kagune, In, Particles, Ray);
}
