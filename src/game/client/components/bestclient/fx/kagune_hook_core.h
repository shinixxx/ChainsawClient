/* ChainsawClient: the kagune is the hook too (part of bc_kagune) - engine free like kagune_core.h, so the offline
   preview runs exactly this code.

   No tentacles of its own: the kagune's own tentacles do it. After BcKagune::Update has posed the kagune, Pose()
   takes some of its tentacles and bends them into the hook (blended by a weight, so a tentacle leaves its place in
   the fan smoothly and comes back to it smoothly):
   - the tentacle that points most towards the hook IS the hook: its tip flies with it, bites into the wall where
     the hook grabs, or coils around the hooked tee, and goes back to the fan when the hook lets go;
   - while the hook holds, up to two more tentacles reach out one after another and dig into the wall next to it
     (found with rays like a hook would) or wrap the hooked tee from other sides;
   - while the tee is pulled along they let go and grab again further on, like a spider climbing;
   - every bite throws sparks and splinters into the kagune's own particles (blood from a tee with bc_kagune_blood).
   The hook itself works as always (it is the server's), only the picture changes. Cheap: a few rays when a tentacle
   reaches, one curve per held tentacle; it draws nothing itself (the kagune's batches draw the tentacles). */
#ifndef GAME_CLIENT_COMPONENTS_BESTCLIENT_FX_KAGUNE_HOOK_CORE_H
#define GAME_CLIENT_COMPONENTS_BESTCLIENT_FX_KAGUNE_HOOK_CORE_H

#include "kagune_core.h"

namespace BcKaguneHook
{
constexpr int MAX_LIMBS = 3; // the hook + two that hold on next to it
constexpr float DIG_DEPTH = 11.0f; // units the tip goes into the block it holds on to (at scale 1)

// DDNet hook states (game/gamecore.h)
enum
{
	HOOK_NONE = 0, // not drawn (idle / retracted)
	HOOK_FLYING_STATE = 4,
	HOOK_GRABBED_STATE = 5,
};

enum
{
	LIMB_IDLE = 0,
	LIMB_REACH,
	LIMB_HOLD,
	LIMB_RELEASE,
};

struct CLimb
{
	int m_Mode = LIMB_IDLE;
	int m_Slot = -1; // which kagune tentacle
	float m_T = 0.0f; // seconds in the mode
	float m_Delay = 0.0f;
	float m_Hold = 0.0f;
	float m_HoldFor = 0.6f;
	float m_Weight = 0.0f; // 0 = the kagune's own pose, 1 = the hook pose
	vec2 m_Anchor = vec2(0.0f, 0.0f);
	bool m_OnTee = false;
	vec2 m_TeeOffset = vec2(0.0f, 0.0f);
	float m_WrapDir = 1.0f;
	vec2 m_Tip = vec2(0.0f, 0.0f);
	vec2 m_From = vec2(0.0f, 0.0f);
	bool m_Curve = false; // the smoothed control points are set
	vec2 m_C1 = vec2(0.0f, 0.0f), m_C1Vel = vec2(0.0f, 0.0f);
	vec2 m_C2 = vec2(0.0f, 0.0f), m_C2Vel = vec2(0.0f, 0.0f);
	float m_Wrap = 0.0f, m_WrapVel = 0.0f;
	vec2 m_Center = vec2(0.0f, 0.0f); // the tee it coils around, kept while it lets go (the hook knows no tee then)
	bool m_HasCenter = false;
	float m_Dig = 0.0f, m_DigVel = 0.0f; // 0 .. 1 how deep the tip is in the block
	float m_Side = 1.0f;
	bool m_Bit = false;
};

struct CState
{
	CLimb m_aLimbs[MAX_LIMBS];
	int m_LastHookState = HOOK_NONE;
	float m_Time = 0.0f;
	uint32_t m_Seed = 0x2545F491u;
};

struct CInput
{
	vec2 m_Pos = vec2(0.0f, 0.0f); // the tee
	vec2 m_Vel = vec2(0.0f, 0.0f); // units per second
	int m_HookState = HOOK_NONE; // HOOK_NONE = no hook this frame
	vec2 m_HookPos = vec2(0.0f, 0.0f);
	bool m_HookTee = false;
	vec2 m_HookTeePos = vec2(0.0f, 0.0f);
	float m_Scale = 1.0f;
	float m_Dt = 0.0f;
	float m_Blood = 0.0f;
	float m_Particles = 1.0f;
	int m_Style = BcKagune::STYLE_RINKAKU;
};

inline float EaseOut(float t)
{
	t = BcKagune::Clamp01(t);
	return 1.0f - (1.0f - t) * (1.0f - t) * (1.0f - t);
}

template<class FRay>
bool FindWallSpot(const CInput &In, vec2 HookPos, float Turn, vec2 &Out, FRay &&Ray)
{
	const vec2 To = HookPos - In.m_Pos;
	const float Len = length(To);
	if(Len < 1.0f)
		return false;
	const vec2 Dir = rotate(To / Len, Turn);
	const vec2 End = In.m_Pos + Dir * (Len * 1.2f + 40.0f * In.m_Scale);
	vec2 Hit;
	if(!Ray(In.m_Pos, End, Hit))
		return false;
	if(distance(Hit, HookPos) > Len * 0.8f + 60.0f * In.m_Scale)
		return false;
	Out = Hit;
	return true;
}

inline void Bite(BcKagune::CParticles &Particles, const CInput &In, vec2 At, vec2 Dir, bool Tee)
{
	if(In.m_Particles <= 0.0f)
		return;
	BcKagune::CParams Params;
	Params.m_Particles = 0.55f * In.m_Particles;
	Params.m_Style = In.m_Style;
	BcKagune::CInput KIn;
	KIn.m_Scale = In.m_Scale;
	BcKagune::StrikeSparks(Particles, Params, KIn, At, Dir, false);
	if(Tee && In.m_Blood > 0.0f)
		BcKagune::BloodBurst(Particles, 0.6f * In.m_Blood, At, Dir, In.m_Scale, false);
	if(!Tee)
	{
		// chips of the wall: a few dark grains that fall
		for(int n = 0; n < (int)(8 * In.m_Particles); n++)
		{
			BcKagune::CMote *pChip = Particles.New();
			if(!pChip)
				break;
			pChip->m_Pos = At;
			pChip->m_Vel = rotate(-Dir, BcKagune::RandRange(Particles.m_Seed, -70.0f, 70.0f)) * BcKagune::RandRange(Particles.m_Seed, 80.0f, 260.0f) * In.m_Scale;
			pChip->m_LifeSpan = BcKagune::RandRange(Particles.m_Seed, 0.35f, 0.6f);
			pChip->m_Size = BcKagune::RandRange(Particles.m_Seed, 1.5f, 2.6f) * In.m_Scale;
			pChip->m_Tone = 0.0f;
			pChip->m_Drag = 2.0f;
			pChip->m_Rise = -700.0f;
			pChip->m_Front = true;
		}
	}
}

inline vec2 TeeAnchor(const CInput &In, const CLimb &L)
{
	return In.m_HookTeePos + L.m_TeeOffset * In.m_Scale;
}

inline vec2 TipOf(const BcKagune::CTentacle &T) { return T.m_aPos[BcKagune::SIM_POINTS - 1]; }

// a free kagune tentacle for a limb: the one that points best at the spot (the hook) or is closest to it
inline int PickSlot(const CState &S, const BcKagune::CTee &K, vec2 Spot, bool ByAim)
{
	int Best = -1;
	float BestScore = -1e9f;
	for(int k = 0; k < K.m_Count; k++)
	{
		bool Used = false;
		for(const CLimb &L : S.m_aLimbs)
			Used |= L.m_Mode != LIMB_IDLE && L.m_Slot == k;
		if(Used)
			continue;
		const BcKagune::CTentacle &T = K.m_aTentacles[k];
		const vec2 Root = T.m_aPos[0];
		const vec2 Tip = TipOf(T);
		float Score;
		if(ByAim)
			Score = dot(normalize(Tip - Root + vec2(0.0f, 0.001f)), normalize(Spot - Root + vec2(0.0f, 0.001f)));
		else
			Score = -distance(Tip, Spot);
		if(Score > BestScore)
		{
			BestScore = Score;
			Best = k;
		}
	}
	return Best;
}

template<class FRay>
void StartReach(CState &S, CLimb &L, int Index, const CInput &In, vec2 From, FRay &&Ray)
{
	L.m_Mode = LIMB_REACH;
	L.m_T = 0.0f;
	L.m_Hold = 0.0f;
	L.m_HoldFor = BcKagune::RandRange(S.m_Seed, 0.45f, 0.8f);
	L.m_Bit = false;
	L.m_From = From;
	L.m_Tip = From;
	L.m_Side = (Index % 2) ? 1.0f : -1.0f;
	if(In.m_HookTee)
	{
		// around the tee from another side, coiling one way or the other
		L.m_OnTee = true;
		const vec2 Away = normalize(In.m_HookTeePos - In.m_Pos + vec2(0.001f, 0.0f));
		L.m_TeeOffset = rotate(-Away, (Index % 2 ? 1.0f : -1.0f) * 40.0f) * 12.0f;
		L.m_WrapDir = Index % 2 ? 1.0f : -1.0f;
		L.m_Anchor = TeeAnchor(In, L);
		return;
	}
	L.m_OnTee = false;
	const float Sign = Index % 2 ? 1.0f : -1.0f;
	const float aTry[3] = {Sign * (10.0f + BcKagune::RandRange(S.m_Seed, -3.0f, 3.0f)), Sign * 5.0f, -Sign * 10.0f};
	for(float Turn : aTry)
	{
		if(FindWallSpot(In, In.m_HookPos, Turn, L.m_Anchor, Ray))
			return;
	}
	L.m_Anchor = In.m_HookPos + rotate(normalize(In.m_HookPos - In.m_Pos + vec2(0.001f, 0.0f)), 90.0f) * (Sign * 10.0f * In.m_Scale);
}

inline void Let(CLimb &L)
{
	if(L.m_Mode == LIMB_IDLE || L.m_Mode == LIMB_RELEASE)
		return;
	L.m_Mode = LIMB_RELEASE;
	L.m_T = 0.0f;
}

// after BcKagune::Update: bend the tentacles of the kagune K (just posed) that hold the hook
template<class FRay>
void Pose(CState &S, BcKagune::CTee &K, const CInput &In, BcKagune::CParticles &Particles, FRay &&Ray)
{
	using namespace BcKagune;
	const float Dt = In.m_Dt;
	const bool Hook = In.m_HookState != HOOK_NONE;
	const bool Grabbed = In.m_HookState == HOOK_GRABBED_STATE;
	const vec2 HookAt = In.m_HookTee ? In.m_HookTeePos : In.m_HookPos;
	const float Speed = length(In.m_Vel);
	S.m_Time += Dt;
	// tentacles that went away with the kagune (fewer tentacles now, pulled in)
	for(CLimb &L : S.m_aLimbs)
	{
		if(L.m_Mode != LIMB_IDLE && (L.m_Slot < 0 || L.m_Slot >= K.m_Count || K.m_Presence <= 0.0f))
			L = CLimb();
	}
	const int Extra = std::min(MAX_LIMBS - 1, K.m_Count - 1);

	// 0: the hook itself
	{
		CLimb &L = S.m_aLimbs[0];
		if(Hook)
		{
			if(L.m_Mode == LIMB_IDLE)
			{
				L.m_Slot = PickSlot(S, K, HookAt, true);
				L.m_Curve = false;
			}
			if(L.m_Slot >= 0)
			{
				L.m_Mode = LIMB_HOLD;
				L.m_OnTee = In.m_HookTee;
				L.m_TeeOffset = vec2(0.0f, 0.0f);
				L.m_WrapDir = 1.0f;
				L.m_Side = -1.0f;
				L.m_Anchor = HookAt;
				L.m_Tip = HookAt;
				L.m_Weight = std::min(1.0f, L.m_Weight + Dt / 0.07f);
				if(Grabbed && S.m_LastHookState != HOOK_GRABBED_STATE)
				{
					const vec2 Dir = normalize(HookAt - K.m_aTentacles[L.m_Slot].m_aPos[0] + vec2(0.001f, 0.0f));
					Bite(Particles, In, In.m_HookTee ? HookAt - Dir * 14.0f * In.m_Scale : In.m_HookPos, Dir, In.m_HookTee);
				}
			}
		}
		else
			Let(L);
	}

	// 1..: hold on next to it while the hook holds
	for(int i = 1; i < MAX_LIMBS; i++)
	{
		CLimb &L = S.m_aLimbs[i];
		const bool Wanted = Grabbed && i <= Extra;
		if(!Wanted)
		{
			Let(L);
			L.m_Delay = 0.0f;
		}
		else if(L.m_Mode == LIMB_IDLE)
		{
			if(S.m_LastHookState != HOOK_GRABBED_STATE)
				L.m_Delay = 0.05f + 0.07f * (i - 1) + RandRange(S.m_Seed, 0.0f, 0.03f);
			L.m_Delay -= Dt;
			if(L.m_Delay <= 0.0f)
			{
				// the free tentacle closest to where it will grab
				const int Slot = PickSlot(S, K, HookAt, false);
				if(Slot >= 0)
				{
					L.m_Slot = Slot;
					L.m_Curve = false;
					StartReach(S, L, i, In, TipOf(K.m_aTentacles[Slot]), Ray);
				}
			}
		}
		if(L.m_OnTee && In.m_HookTee)
			L.m_Anchor = TeeAnchor(In, L);
		if(L.m_Mode == LIMB_REACH)
		{
			L.m_T += Dt;
			const float Time = 0.11f + 0.0004f * distance(L.m_From, L.m_Anchor) / In.m_Scale;
			const float t = L.m_T / Time;
			L.m_Tip = mix(L.m_From, L.m_Anchor, EaseOutBack(Clamp01(t)));
			L.m_Weight = std::max(L.m_Weight, Smooth(0.0f, 0.35f, t));
			if(t >= 1.0f)
			{
				L.m_Mode = LIMB_HOLD;
				L.m_T = 0.0f;
				L.m_Tip = L.m_Anchor;
				L.m_Weight = 1.0f;
				if(!L.m_Bit)
				{
					L.m_Bit = true;
					Bite(Particles, In, L.m_Anchor, normalize(L.m_Anchor - K.m_aTentacles[L.m_Slot].m_aPos[0] + vec2(0.001f, 0.0f)), L.m_OnTee);
				}
			}
		}
		else if(L.m_Mode == LIMB_HOLD)
		{
			L.m_Hold += Dt;
			L.m_Tip = L.m_Anchor;
			if(L.m_OnTee != In.m_HookTee)
				Let(L);
			else if(!L.m_OnTee)
			{
				// left behind or too far: let go and grab again further on, like climbing
				const vec2 Root = K.m_aTentacles[L.m_Slot].m_aPos[0];
				const vec2 ToHook = HookAt - Root;
				const vec2 ToTip = L.m_Anchor - Root;
				const float Dist = length(ToTip);
				const float Reach = length(ToHook) * 1.25f + 70.0f * In.m_Scale;
				const bool Behind = dot(ToTip, ToHook) < 0.35f * Dist * length(ToHook);
				const bool Tired = Speed > 250.0f * In.m_Scale && L.m_Hold > L.m_HoldFor;
				if(Dist > Reach || Dist < 22.0f * In.m_Scale || Behind || Tired)
					Let(L);
			}
		}
		else if(L.m_Mode == LIMB_RELEASE && Wanted && L.m_Weight < 0.45f)
		{
			// halfway back to the fan it grabs again, from where its tip is now
			const vec2 Now = mix(TipOf(K.m_aTentacles[L.m_Slot]), L.m_Tip, L.m_Weight);
			StartReach(S, L, i, In, Now, Ray);
		}
	}
	// letting go: back into the fan
	for(CLimb &L : S.m_aLimbs)
	{
		if(L.m_Mode != LIMB_RELEASE)
			continue;
		L.m_Weight = std::max(0.0f, L.m_Weight - Dt / 0.16f);
		if(L.m_Weight <= 0.0f)
			L = CLimb();
	}
	S.m_LastHookState = In.m_HookState;

	// bend the held tentacles: out of their root the way they grow, then over to the tip; the tee ones coil around
	for(CLimb &L : S.m_aLimbs)
	{
		if(L.m_Mode == LIMB_IDLE || L.m_Slot < 0 || L.m_Weight <= 0.0f)
			continue;
		CTentacle &T = K.m_aTentacles[L.m_Slot];
		const vec2 Root = T.m_aPos[0];
		if(L.m_OnTee && In.m_HookTee)
		{
			L.m_Center = In.m_HookTeePos;
			L.m_HasCenter = true;
		}
		const vec2 To = L.m_Tip - Root;
		const float Len = length(To);
		if(Len < 4.0f * In.m_Scale)
			continue;
		// never across the map: a hook is at most 380 units long, anything far beyond is a jump (teleport, respawn)
		if(Len > 700.0f * In.m_Scale || (L.m_HasCenter && distance(L.m_Center, Root) > 700.0f * In.m_Scale))
		{
			L = CLimb();
			continue;
		}
		const vec2 D = To / Len;
		const vec2 Nrm(-D.y, D.x);
		const vec2 Grow = normalize(T.m_aPos[1] - T.m_aPos[0] + vec2(0.0f, -0.001f));
		const float Taut = L.m_Mode == LIMB_HOLD ? 0.7f : 0.2f;
		const vec2 StartDir = normalize(Grow * (0.9f - 0.5f * Taut) + D);
		const vec2 C1Goal = Root + StartDir * std::min(Len * 0.42f, 70.0f * In.m_Scale);
		const vec2 C2Goal = L.m_Tip - D * (Len * 0.32f) + Nrm * (L.m_Side * Len * (0.12f - 0.08f * Taut));
		if(!L.m_Curve)
		{
			L.m_Curve = true;
			L.m_C1 = C1Goal;
			L.m_C2 = C2Goal;
			L.m_C1Vel = L.m_C2Vel = vec2(0.0f, 0.0f);
		}
		// the body lags a little behind the tip: it bends while it moves
		L.m_C1 = SmoothDamp(L.m_C1, C1Goal, L.m_C1Vel, 0.06f, Dt);
		L.m_C2 = SmoothDamp(L.m_C2, C2Goal, L.m_C2Vel, 0.09f, Dt);
		const bool Coil = L.m_OnTee && L.m_Mode == LIMB_HOLD;
		L.m_Wrap = SmoothDamp(L.m_Wrap, Coil ? 1.0f : 0.0f, L.m_WrapVel, 0.12f, Dt);
		if(!L.m_OnTee && L.m_Wrap <= 0.02f)
			L.m_HasCenter = false;

		const int CoilPoints = L.m_Wrap > 0.02f && L.m_HasCenter ? 4 : 0;
		const int Body = SIM_POINTS - CoilPoints;
		const vec2 Center = L.m_Center;
		const float Radius = 25.0f * In.m_Scale;
		// a tip that holds on to the map goes a bit into the block
		const bool Anchored = !L.m_OnTee && (&L == &S.m_aLimbs[0] ? In.m_HookState == HOOK_GRABBED_STATE : L.m_Mode == LIMB_HOLD);
		L.m_Dig = SmoothDamp(L.m_Dig, Anchored ? 1.0f : 0.0f, L.m_DigVel, 0.05f, Dt);
		vec2 End = L.m_Tip + D * (DIG_DEPTH * In.m_Scale * L.m_Dig);
		float EntryAngle = 0.0f;
		if(CoilPoints)
		{
			EntryAngle = angle(Root - Center) + L.m_WrapDir * 0.35f;
			End = Center + direction(EntryAngle) * mix(length(L.m_Tip - Center), Radius, L.m_Wrap);
		}
		const float Wave = (L.m_Mode == LIMB_REACH || L.m_Mode == LIMB_RELEASE ? 6.0f : (In.m_HookState == HOOK_FLYING_STATE ? 5.0f : 2.0f)) * In.m_Scale;
		const float w = L.m_Weight * L.m_Weight * (3.0f - 2.0f * L.m_Weight);
		vec2 aPath[SIM_POINTS];
		for(int j = 0; j < Body; j++)
		{
			const float s = j / (float)(Body - 1);
			const float u = 1.0f - s;
			vec2 P = Root * (u * u * u) + L.m_C1 * (3.0f * u * u * s) + L.m_C2 * (3.0f * u * s * s) + End * (s * s * s);
			P += Nrm * (Wave * std::sin(2.0f * PI * (1.4f * s - 1.6f * S.m_Time) + T.m_Phase) * std::sin(PI * s));
			aPath[j] = P;
		}
		for(int j = 0; j < CoilPoints; j++)
		{
			const float a = EntryAngle + L.m_WrapDir * (j + 1) * 0.62f * L.m_Wrap;
			const float r = mix(length(End - Center), Radius * (1.0f - 0.05f * j), L.m_Wrap);
			aPath[Body + j] = Center + direction(a) * r;
		}
		float PathLength = 0.0f;
		for(int j = 1; j < SIM_POINTS; j++)
			PathLength += distance(aPath[j], aPath[j - 1]);
		for(int j = 1; j < SIM_POINTS; j++)
			T.m_aPos[j] = mix(T.m_aPos[j], aPath[j], w);
		// the texture stretches with it instead of sliding
		T.m_RestLength = mix(T.m_RestLength, PathLength, w);
		// no strike trail across the hook
		if(w > 0.5f)
			T.m_TrailNum = 0;
	}
}

inline bool Busy(const CState &S)
{
	for(const CLimb &L : S.m_aLimbs)
		if(L.m_Mode != LIMB_IDLE)
			return true;
	return false;
}
}

#endif
