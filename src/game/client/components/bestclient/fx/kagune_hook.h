/* ChainsawClient: the kagune is the hook too (part of bc_kagune, no own settings), see kagune_hook_core.h.
   It draws nothing and has no texture: the kagune's own tentacles hold the hook and the kagune's batches draw them.
   - CPlayers::RenderHook calls OnHook() where it would draw the hook chain: the chain and the hand are skipped and
     the hook is remembered;
   - CBcKaguneFx::RenderBack asks Hooking() (a hooking tee shows its kagune with any weapon) and calls Pose() right
     after it posed the kagune: the tentacles that hold the hook are bent there, bites go into the kagune particles. */
#ifndef GAME_CLIENT_COMPONENTS_BESTCLIENT_FX_KAGUNE_HOOK_H
#define GAME_CLIENT_COMPONENTS_BESTCLIENT_FX_KAGUNE_HOOK_H

#include <base/vmath.h>

#include <engine/shared/protocol.h>

#include <game/client/component.h>

#include "kagune_hook_core.h"

class CBcKaguneHookFx : public CComponent
{
public:
	int Sizeof() const override { return sizeof(*this); }
	void OnReset() override;

	// CPlayers::RenderHook, instead of the chain: true = the kagune is the hook (nothing else is drawn)
	bool OnHook(int ClientId, vec2 Pos, vec2 HookPos, int HookState, int HookedPlayer, float Alpha, float Size);
	// CBcKaguneFx: is this tee hooking right now (its kagune comes out for it)
	bool Hooking(int ClientId) const;
	// CBcKaguneFx, right after BcKagune::Update for this tee
	void Pose(int ClientId, BcKagune::CTee &Kagune, const BcKagune::CInput &KaguneIn, BcKagune::CParticles &Particles);

private:
	struct CRecord
	{
		float m_Time = -1.0f; // LocalTime of the last OnHook
		vec2 m_HookPos = vec2(0.0f, 0.0f);
		int m_HookState = 0;
		int m_Hooked = -1;
	};

	BcKaguneHook::CState m_aStates[MAX_CLIENTS];
	CRecord m_aRecords[MAX_CLIENTS];

	bool ShowFor(int ClientId) const;
};

#endif
