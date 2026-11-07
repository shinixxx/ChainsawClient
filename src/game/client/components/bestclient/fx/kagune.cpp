/* ChainsawClient: kagune instead of the hammer (see kagune.h, the look and the motion are in kagune_core.h) */
#include "kagune.h"

#include "cc_fx_common.h"
#include "own_tee.h"

#include <base/math.h>

#include <engine/image.h>
#include <engine/shared/config.h>

#include <game/client/components/bestclient/bestclient.h>
#include <game/client/gameclient.h>

#include <algorithm>
#include <cmath>

void CBcKaguneFx::OnInit()
{
	UpdateTexture();
	OnReset();
}

void CBcKaguneFx::OnReset()
{
	for(int i = 0; i < MAX_CLIENTS; i++)
	{
		m_aTees[i].m_Init = false;
		m_aTees[i].m_Presence = 0.0f;
		m_aTees[i].m_LastAttackId = -1;
		m_aTees[i].m_Seed = 0x9E3779B9u * (uint32_t)(i + 1);
		m_aRecords[i] = CRecord();
		m_aDrawn[i] = false;
		m_aBleed[i] = 0.0f;
		m_aBleedAcc[i] = 0.0f;
		m_aBleedDir[i] = vec2(1.0f, 0.0f);
	}
	m_Particles.m_Num = 0;
	m_LastTime = -1.0f;
	m_Any = false;
}

bool CBcKaguneFx::ShowFor(int ClientId) const
{
	return g_Config.m_BcKagune && in_range(ClientId, MAX_CLIENTS - 1) && (g_Config.m_BcKaguneOthers || BcIsOwnTee(GameClient(), ClientId));
}

void CBcKaguneFx::UpdateTexture()
{
	const int Style = std::clamp(g_Config.m_BcKaguneStyle, 0, BcKagune::NUM_STYLES - 1);
	if(m_Texture.IsValid() && m_TextureColor == (int)g_Config.m_BcKaguneColor && m_TextureStyle == Style)
		return;
	m_TextureColor = (int)g_Config.m_BcKaguneColor;
	m_TextureStyle = Style;
	m_Palette = BcKagune::MakePalette(ColorHSLA(g_Config.m_BcKaguneColor), Style);
	CImageInfo Image;
	Image.m_Width = BcKagune::TEX_W;
	Image.m_Height = BcKagune::TEX_H;
	Image.m_Format = CImageInfo::FORMAT_RGBA;
	Image.Allocate();
	BcKagune::BuildTexture(Image.m_pData, m_Palette, Style);
	if(m_Texture.IsValid())
		Graphics()->UnloadTexture(&m_Texture);
	m_Texture = Graphics()->LoadTextureRawMove(Image, 0, "cc_kagune");
}

BcKagune::CParams CBcKaguneFx::Params(int ClientId) const
{
	BcKagune::CParams Params;
	Params.m_Count = std::clamp(g_Config.m_BcKaguneCount, 1, BcKagune::MAX_TENTACLES);
	Params.m_Length = g_Config.m_BcKaguneLength / 100.0f;
	Params.m_Width = g_Config.m_BcKaguneWidth / 100.0f;
	Params.m_Shimmer = g_Config.m_BcKaguneShimmer / 100.0f;
	Params.m_Particles = BcFxSuppressed(GameClient()) ? 0.0f : g_Config.m_BcKaguneParticles / 100.0f;
	// our own tees get the finer mesh, the others a lighter one
	Params.m_Samples = BcIsOwnTee(GameClient(), ClientId) ? 32 : 22;
	Params.m_AttackMode = std::clamp(g_Config.m_BcKaguneAttack, 0, BcKagune::NUM_ATTACK_MODES - 1);
	Params.m_Style = std::clamp(g_Config.m_BcKaguneStyle, 0, BcKagune::NUM_STYLES - 1);
	return Params;
}

bool CBcKaguneFx::OnHammer(int ClientId, bool Ghost, vec2 Pos, vec2 Direction, vec2 Vel, bool InAir, bool Inactive, int AttackTick, float AttackTime, float Alpha, float Size)
{
	if(!ShowFor(ClientId))
		return false;
	// the unpredicted ghost of a tee with a kagune shows no hammer either; the kagune follows the real tee
	if(Ghost)
		return true;
	(void)Pos; // the kagune is drawn at the tee's render position of the next frame
	CRecord &Rec = m_aRecords[ClientId];
	Rec.m_Time = LocalTime();
	Rec.m_Aim = length(Direction) > 0.001f ? normalize(Direction) : vec2(1.0f, 0.0f);
	Rec.m_Vel = Vel * (float)Client()->GameTickSpeed();
	Rec.m_InAir = InAir;
	Rec.m_Resting = Inactive;
	Rec.m_AttackTick = AttackTick;
	Rec.m_AttackTime = AttackTime;
	Rec.m_Alpha = Alpha;
	Rec.m_Size = Size;
	return true;
}

// bc_kagune_gun: a tee holding the pistol keeps its kagune; CPlayers draws the pistol as usual, the record is
// filled from the snapshot the same way CPlayers would (aim, velocity, in the air, afk)
bool CBcKaguneFx::GunRecord(int ClientId, CRecord &Rec)
{
	if(!g_Config.m_BcKaguneGun)
		return false;
	const CGameClient::CClientData &CD = GameClient()->m_aClients[ClientId];
	if(CD.m_RenderCur.m_Weapon != WEAPON_GUN)
		return false;
	// a frozen tee shows no weapon: the kagune pulls in like with the hammer
	const bool Local = ClientId == GameClient()->m_aLocalIds[0] || ClientId == GameClient()->m_aLocalIds[1];
	if(Local ? CD.m_Predicted.m_FreezeEnd != 0 : CD.m_FreezeEnd != 0)
		return false;
	const float Intra = CD.m_IsPredicted ? Client()->PredIntraGameTick(g_Config.m_ClDummy) : Client()->IntraGameTick(g_Config.m_ClDummy);
	const float Angle = GameClient()->m_Players.GetPlayerTargetAngle(&CD.m_RenderPrev, &CD.m_RenderCur, ClientId, Intra);
	const vec2 PrevVel = vec2(CD.m_RenderPrev.m_VelX / 256.0f, CD.m_RenderPrev.m_VelY / 256.0f);
	const vec2 Vel = mix(PrevVel, vec2(CD.m_RenderCur.m_VelX / 256.0f, CD.m_RenderCur.m_VelY / 256.0f), Intra);
	const vec2 Pos = CD.m_RenderPos;
	const bool Known = Rec.m_Time >= 0.0f;
	Rec.m_Time = LocalTime();
	Rec.m_Aim = direction(Angle);
	Rec.m_Vel = Vel * (float)Client()->GameTickSpeed();
	Rec.m_InAir = !Collision()->CheckPoint(Pos.x, Pos.y + 16.0f);
	Rec.m_Resting = CD.m_Afk || CD.m_Paused;
	Rec.m_AttackTick = CD.m_RenderCur.m_AttackTick; // pistol shots are no strikes
	Rec.m_AttackTime = 100.0f;
	Rec.m_Alpha = Known ? Rec.m_Alpha : 1.0f;
	Rec.m_Size = BestClientTeeRenderSize(GameClient(), ClientId);
	return true;
}

// chainsaw: the kagune is the hook too: a hooking tee shows its kagune with any weapon; the record is filled from
// the snapshot like GunRecord does (aim, velocity, in the air, afk), no strikes
bool CBcKaguneFx::HookRecord(int ClientId, CRecord &Rec)
{
	const CGameClient::CClientData &CD = GameClient()->m_aClients[ClientId];
	const float Intra = CD.m_IsPredicted ? Client()->PredIntraGameTick(g_Config.m_ClDummy) : Client()->IntraGameTick(g_Config.m_ClDummy);
	const float Angle = GameClient()->m_Players.GetPlayerTargetAngle(&CD.m_RenderPrev, &CD.m_RenderCur, ClientId, Intra);
	const vec2 PrevVel = vec2(CD.m_RenderPrev.m_VelX / 256.0f, CD.m_RenderPrev.m_VelY / 256.0f);
	const vec2 Vel = mix(PrevVel, vec2(CD.m_RenderCur.m_VelX / 256.0f, CD.m_RenderCur.m_VelY / 256.0f), Intra);
	const vec2 Pos = CD.m_RenderPos;
	const bool Known = Rec.m_Time >= 0.0f;
	Rec.m_Time = LocalTime();
	Rec.m_Aim = direction(Angle);
	Rec.m_Vel = Vel * (float)Client()->GameTickSpeed();
	Rec.m_InAir = !Collision()->CheckPoint(Pos.x, Pos.y + 16.0f);
	Rec.m_Resting = false;
	Rec.m_AttackTick = CD.m_RenderCur.m_AttackTick;
	Rec.m_AttackTime = 100.0f;
	Rec.m_Alpha = Known ? Rec.m_Alpha : 1.0f;
	Rec.m_Size = BestClientTeeRenderSize(GameClient(), ClientId);
	return true;
}

// bc_kagune_blood: a strike touching another tee wounds it
void CBcKaguneFx::CheckWounds(int Owner, float Scale)
{
	if(!g_Config.m_BcKaguneBlood || BcFxSuppressed(GameClient()))
		return;
	const float Amount = g_Config.m_BcKaguneBloodAmount / 100.0f;
	BcKagune::CTee &Tee = m_aTees[Owner];
	for(int t = 0; t < Tee.m_Count; t++)
	{
		BcKagune::CTentacle &T = Tee.m_aTentacles[t];
		if(T.m_StrikeT < 0.0f || T.m_BloodDone || T.m_StrikeWeight < 0.45f)
			continue;
		const bool United = T.m_StrikeKind == BcKagune::KIND_UNITED;
		if(United && T.m_StrikeSlot != 0)
			continue;
		for(int j = 0; j < MAX_CLIENTS; j++)
		{
			if(j == Owner || !GameClient()->m_Snap.m_aCharacters[j].m_Active)
				continue;
			const vec2 Center = GameClient()->m_aClients[j].m_RenderPos;
			const float Radius = 26.0f * BestClientTeeRenderSize(GameClient(), j) / 64.0f;
			vec2 Contact;
			if(!BcKagune::StrikeContact(T, Center, Radius, Contact))
				continue;
			BcKagune::BloodBurst(m_Particles, Amount, Contact, T.m_StrikeAim, Scale, United);
			m_aBleed[j] = std::max(m_aBleed[j], United ? 1.4f : 0.9f);
			m_aBleedDir[j] = T.m_StrikeAim;
			T.m_BloodDone = true;
			break;
		}
	}
}

// the wounds keep dripping for a moment, following their tee
void CBcKaguneFx::UpdateBleeding(float Dt)
{
	const float Amount = g_Config.m_BcKaguneBlood && !BcFxSuppressed(GameClient()) ? g_Config.m_BcKaguneBloodAmount / 100.0f : 0.0f;
	for(int j = 0; j < MAX_CLIENTS; j++)
	{
		if(m_aBleed[j] <= 0.0f)
			continue;
		m_aBleed[j] -= Dt;
		if(Amount <= 0.0f || !GameClient()->m_Snap.m_aCharacters[j].m_Active)
		{
			m_aBleed[j] = 0.0f;
			continue;
		}
		m_aBleedAcc[j] += Dt * 16.0f * Amount;
		const float Scale = BestClientTeeRenderSize(GameClient(), j) / 64.0f;
		while(m_aBleedAcc[j] >= 1.0f)
		{
			m_aBleedAcc[j] -= 1.0f;
			BcKagune::BloodDrip(m_Particles, GameClient()->m_aClients[j].m_RenderPos, m_aBleedDir[j], Scale);
		}
	}
}

void CBcKaguneFx::RenderBack()
{
	const float Now = LocalTime();
	float Dt = m_LastTime < 0.0f ? 0.0f : std::clamp(Now - m_LastTime, 0.0f, 0.1f);
	m_LastTime = Now;
	if(!g_Config.m_BcKagune)
	{
		if(m_Any || m_Particles.m_Num > 0)
			OnReset();
		return;
	}
	CcFx::CTimer Timer(CcFx::STAT_KAGUNE);
	if(GameClient()->IsWorldPaused() || GameClient()->IsDemoPlaybackPaused())
		Dt = 0.0f;
	else
		Dt *= GameClient()->GetAnimationPlaybackSpeed();
	if(m_TextureColor != (int)g_Config.m_BcKaguneColor || m_TextureStyle != std::clamp(g_Config.m_BcKaguneStyle, 0, BcKagune::NUM_STYLES - 1))
		UpdateTexture();

	// CPlayers skips tees outside the screen + 100; it calls OnHammer every frame for the tees it draws with a hammer
	const CcFx::CView View = CcFx::View(Graphics(), 0.0f);
	const CcFx::CView PlayersView = CcFx::View(Graphics(), 100.0f);
	// a single slow frame must not pull the kagune in
	const float Fresh = std::max(0.12f, 3.0f * Client()->RenderFrameTime());

	m_Any = false;
	for(int i = 0; i < MAX_CLIENTS; i++)
	{
		BcKagune::CTee &Tee = m_aTees[i];
		CRecord &Rec = m_aRecords[i];
		m_aDrawn[i] = false;
		if(!GameClient()->m_Snap.m_aCharacters[i].m_Active || !ShowFor(i))
		{
			if(Tee.m_Init)
			{
				Tee.m_Init = false;
				Tee.m_Presence = 0.0f;
				Tee.m_LastAttackId = -1;
			}
			continue;
		}
		const vec2 Pos = GameClient()->m_aClients[i].m_RenderPos;
		bool Seen = Rec.m_Time >= 0.0f && Now - Rec.m_Time < Fresh;
		bool Gun = false;
		// no hammer drawn: with bc_kagune_gun the pistol keeps the kagune out
		if(!Seen && PlayersView.Sees(Pos, 0.0f) && GunRecord(i, Rec))
			Seen = Gun = true;
		// chainsaw: the kagune is the hook too: while the tee hooks, its kagune is out with any weapon
		if(!Seen && GameClient()->m_BcKaguneHookFx.Hooking(i) && HookRecord(i, Rec))
			Seen = Gun = true;
		if(Rec.m_Time < 0.0f)
			continue;
		const float Since = Now - Rec.m_Time;
		// not drawn by CPlayers: off screen keeps the kagune as it is, on screen it means no hammer or frozen: it pulls in
		if(!Seen && !PlayersView.Sees(Pos, 0.0f))
			continue;
		if(!Seen && Tee.m_Init && Tee.m_Presence <= 0.0f)
			continue;
		const BcKagune::CParams Params = this->Params(i);
		BcKagune::CInput In;
		In.m_Pos = Pos;
		In.m_Aim = Rec.m_Aim;
		In.m_Vel = Rec.m_Vel;
		In.m_Scale = Rec.m_Size / 64.0f;
		In.m_Dt = Dt;
		In.m_AttackTime = Gun ? 100.0f : Rec.m_AttackTime + Since;
		In.m_AttackId = Rec.m_AttackTick;
		In.m_Active = Seen;
		In.m_InAir = Rec.m_InAir;
		In.m_Resting = Rec.m_Resting;
		BcKagune::Update(Tee, In, Params, m_Particles);
		GameClient()->m_BcKaguneGhoulFx.Emerge(i, Tee, In, m_Particles); // chainsaw: it comes out of gathered red, the ghoul eye
		GameClient()->m_BcKaguneHookFx.Pose(i, Tee, In, m_Particles); // chainsaw: its tentacles that hold the hook
		if(Tee.m_Presence <= 0.0f)
			continue;
		CheckWounds(i, In.m_Scale);
		BcKagune::BuildSamples(Tee, Params, In.m_Scale);
		if(!View.Sees(Tee.m_Root, BcKagune::Reach(Tee)))
			continue;
		m_aDrawn[i] = true;
		m_Any = true;
	}
	UpdateBleeding(Dt);
	m_Particles.Update(Dt, [&](vec2 P) { return Collision()->CheckPoint(P.x, P.y); });

	const bool AnyBack = BcKagune::AnyIn(m_Particles, BcKagune::LAYER_BACK);
	const bool AnySplats = BcKagune::AnyIn(m_Particles, BcKagune::LAYER_SPLATS);
	if(!m_Any && !AnyBack && !AnySplats)
		return;

	// freeform quads with a color per corner; the engine draws P0 P1 P3 + P0 P3 P2 and SetColor4 takes TL TR BL BR
	// where BR is corner 2 and BL corner 3
	const auto Quad = [&](const BcKagune::CVertex *pV) {
		Graphics()->QuadsSetSubsetFree(pV[0].m_U, pV[0].m_V, pV[1].m_U, pV[1].m_V, pV[2].m_U, pV[2].m_V, pV[3].m_U, pV[3].m_V);
		Graphics()->SetColor4(pV[0].m_Color, pV[1].m_Color, pV[3].m_Color, pV[2].m_Color);
		const IGraphics::CFreeformItem Item(pV[0].m_Pos, pV[1].m_Pos, pV[2].m_Pos, pV[3].m_Pos);
		Graphics()->QuadsDrawFreeform(&Item, 1);
		CcFx::CountQuads(1);
	};
	const auto Sees = [&](vec2 Pos, float Radius) { return View.Sees(Pos, Radius); };

	// 0 (normal): blood stuck on the map, behind everything
	if(AnySplats)
	{
		CcFx::Begin(Graphics(), m_Texture, false);
		BcKagune::EmitParticles(m_Particles, m_Palette, BcKagune::LAYER_SPLATS, Sees, Quad);
		CcFx::End(Graphics());
	}

	// 1 (additive): the glow around the tentacles and on the back
	if(m_Any)
	{
		CcFx::Begin(Graphics(), m_Texture, true);
		for(int i = 0; i < MAX_CLIENTS; i++)
			if(m_aDrawn[i])
				BcKagune::EmitGlow(m_aTees[i], m_Palette, m_aRecords[i].m_Alpha * g_Config.m_BcKaguneGlow / 100.0f, /* chainsaw: bc_kagune_glow */ m_aRecords[i].m_Size / 64.0f, Quad);
		CcFx::End(Graphics());

		// 2 (normal): the bodies with spikes, tendrils and the light flowing over them; the pattern repeats along
		Graphics()->WrapNormal();
		CcFx::Begin(Graphics(), m_Texture, false);
		for(int i = 0; i < MAX_CLIENTS; i++)
			if(m_aDrawn[i])
				BcKagune::EmitBody(m_aTees[i], m_Palette, Params(i), m_aRecords[i].m_Alpha, m_aRecords[i].m_Size / 64.0f, Quad);
		CcFx::End(Graphics());
	}

	// 3 (additive): glints on stabbing tips, drifting red cells
	if(m_Any || AnyBack)
	{
		CcFx::Begin(Graphics(), m_Texture, true);
		for(int i = 0; i < MAX_CLIENTS; i++)
			if(m_aDrawn[i])
				BcKagune::EmitLights(m_aTees[i], m_Palette, m_aRecords[i].m_Alpha, m_aRecords[i].m_Size / 64.0f, Quad);
		BcKagune::EmitParticles(m_Particles, m_Palette, BcKagune::LAYER_BACK, Sees, Quad);
		CcFx::End(Graphics());
	}
	Graphics()->SetColor(1.0f, 1.0f, 1.0f, 1.0f);
}

void CBcKaguneFx::OnRender()
{
	if(Client()->State() != IClient::STATE_ONLINE && Client()->State() != IClient::STATE_DEMOPLAYBACK)
		return;
	if(!g_Config.m_BcKagune || m_Particles.m_Num == 0)
		return;
	const bool AnyFront = BcKagune::AnyIn(m_Particles, BcKagune::LAYER_FRONT);
	const bool AnyDrops = BcKagune::AnyIn(m_Particles, BcKagune::LAYER_DROPS);
	if(!AnyFront && !AnyDrops)
		return;
	CcFx::CTimer Timer(CcFx::STAT_KAGUNE);
	const CcFx::CView View = CcFx::View(Graphics(), 0.0f);
	const auto Sees = [&](vec2 Pos, float Radius) { return View.Sees(Pos, Radius); };
	const auto Quad = [&](const BcKagune::CVertex *pV) {
		Graphics()->QuadsSetSubsetFree(pV[0].m_U, pV[0].m_V, pV[1].m_U, pV[1].m_V, pV[2].m_U, pV[2].m_V, pV[3].m_U, pV[3].m_V);
		Graphics()->SetColor4(pV[0].m_Color, pV[1].m_Color, pV[3].m_Color, pV[2].m_Color);
		const IGraphics::CFreeformItem Item(pV[0].m_Pos, pV[1].m_Pos, pV[2].m_Pos, pV[3].m_Pos);
		Graphics()->QuadsDrawFreeform(&Item, 1);
		CcFx::CountQuads(1);
	};
	// flying blood in front of the tees
	if(AnyDrops)
	{
		CcFx::Begin(Graphics(), m_Texture, false);
		BcKagune::EmitParticles(m_Particles, m_Palette, BcKagune::LAYER_DROPS, Sees, Quad);
		CcFx::End(Graphics());
	}
	// the sparks, flashes and shards of the strikes fly in front of the tees
	if(AnyFront)
	{
		CcFx::Begin(Graphics(), m_Texture, true);
		BcKagune::EmitParticles(m_Particles, m_Palette, BcKagune::LAYER_FRONT, Sees, Quad);
		CcFx::End(Graphics());
	}
	Graphics()->SetColor(1.0f, 1.0f, 1.0f, 1.0f);
}

void CBcKaguneFxBack::OnRender()
{
	if(Client()->State() != IClient::STATE_ONLINE && Client()->State() != IClient::STATE_DEMOPLAYBACK)
		return;
	GameClient()->m_BcKaguneFx.RenderBack();
}
