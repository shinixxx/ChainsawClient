/* ChainsawClient: "maximum optimization" mode. See chainsaw_perf.h. */
#include "chainsaw_perf.h"

#include <base/io.h>
#include <base/str.h>

#include <engine/console.h>
#include <engine/shared/config.h>
#include <engine/storage.h>

#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

namespace
{
enum EGroup
{
	G_OWN = 0, // our effects
	G_ATM, // atmosphere, weather, shaders
	G_OTHERS, // BestClient / TClient effects
	G_MAP, // simpler map
	G_PARTICLES, // no particles at all
	G_SYNC, // no vsync, no anti-aliasing
	G_HUD, // extras on the screen
	G_SYSTEM, // process priority, discord
	NUM_GROUPS,
};

int *GroupSwitch(int Group)
{
	switch(Group)
	{
	case G_OWN: return &g_Config.m_BcPerfOwn;
	case G_ATM: return &g_Config.m_BcPerfAtm;
	case G_OTHERS: return &g_Config.m_BcPerfOthers;
	case G_MAP: return &g_Config.m_BcPerfMap;
	case G_PARTICLES: return &g_Config.m_BcPerfParticles;
	case G_SYNC: return &g_Config.m_BcPerfSync;
	case G_HUD: return &g_Config.m_BcPerfHud;
	case G_SYSTEM: return &g_Config.m_BcPerfSystem;
	}
	return nullptr;
}

struct SEntry
{
	const char *m_pCommand;
	int *m_pValue;
	int m_Set; // the value the mode gives the setting
	int m_Group;
};

#define CC_PERF(Command, Var, Set, Group) {Command, &g_Config.m_##Var, Set, Group}

const SEntry s_aEntries[] = {
	// ChainsawClient effects
	CC_PERF("bc_fire_aura", BcFireAura, 0, G_OWN),
	CC_PERF("bc_fire_hammer", BcFireHammer, 0, G_OWN),
	CC_PERF("bc_land_shockwave", BcLandShockwave, 0, G_OWN),
	CC_PERF("bc_freeze_ice", BcFreezeIce, 0, G_OWN),
	CC_PERF("bc_fire_chain", BcFireChain, 0, G_OWN),
	CC_PERF("bc_fire_burn", BcFireBurn, 0, G_OWN),
	CC_PERF("bc_fire_strike", BcFireStrike, 0, G_OWN),
	CC_PERF("bc_kagune", BcKagune, 0, G_OWN),
	// atmosphere, weather and shaders
	CC_PERF("bc_atm_fireflies", BcAtmFireflies, 0, G_ATM),
	CC_PERF("bc_atm_stars", BcAtmStars, 0, G_ATM),
	CC_PERF("bc_atm_storm", BcAtmStorm, 0, G_ATM),
	CC_PERF("bc_atm_wind", BcAtmWind, 0, G_ATM),
	CC_PERF("bc_atm_snow", BcAtmSnow, 0, G_ATM),
	CC_PERF("bc_atm_fog", BcAtmFog, 0, G_ATM),
	CC_PERF("bc_atm_embers", BcAtmEmbers, 0, G_ATM),
	CC_PERF("bc_postfx_color", BcPostFxColor, 0, G_ATM),
	CC_PERF("bc_postfx_bloom", BcPostFxBloom, 0, G_ATM),
	CC_PERF("bc_postfx_vignette", BcPostFxVignette, 0, G_ATM),
	CC_PERF("bc_postfx_sharpen", BcPostFxSharpen, 0, G_ATM),
	CC_PERF("bc_postfx_grain", BcPostFxGrain, 0, G_ATM),
	CC_PERF("bc_postfx_chroma", BcPostFxChroma, 0, G_ATM),
	CC_PERF("bc_atmosphere", BcAtmosphere, 0, G_ATM),
	CC_PERF("bc_ambient_dust", BcAmbientDust, 0, G_ATM),
	CC_PERF("bc_game_media_background", BcGameMediaBackground, 0, G_ATM),
	// BestClient and TClient effects
	CC_PERF("bc_3d_particles", Bc3dParticles, 0, G_OTHERS),
	CC_PERF("bc_motion_blur", BcMotionBlur, 0, G_OTHERS),
	CC_PERF("bc_jelly_tee", BcJellyTee, 0, G_OTHERS),
	CC_PERF("bc_cursor_trail", BcCursorTrail, 0, G_OTHERS),
	CC_PERF("bc_laser_glow", BcLaserGlow, 0, G_OTHERS),
	CC_PERF("bc_shotgun_glow", BcShotgunGlow, 0, G_OTHERS),
	CC_PERF("bc_rocket_glow", BcRocketGlow, 0, G_OTHERS),
	CC_PERF("bc_meme_nuke_explosion", BcMemeNukeExplosion, 0, G_OTHERS),
	CC_PERF("bc_nameplate_gradient", BcNameplateGradient, 0, G_OTHERS),
	CC_PERF("bc_nameplate_gradient_clan", BcNameplateGradientClan, 0, G_OTHERS),
	CC_PERF("bc_nameplate_gradient_skin", BcNameplateGradientSkin, 0, G_OTHERS),
	CC_PERF("bc_nameplate_gradient_everything", BcNameplateGradientEverything, 0, G_OTHERS),
	CC_PERF("bc_flying_name_plates", BcFlyingNamePlates, 0, G_OTHERS),
	CC_PERF("bc_hook_style", BcHookStyle, 0, G_OTHERS),
	CC_PERF("bc_hook_tether", BcHookTether, 0, G_OTHERS),
	CC_PERF("bc_hook_lasso_box", BcHookLassoBox, 0, G_OTHERS),
	CC_PERF("bc_air_jump", BcAirJump, 0, G_OTHERS),
	CC_PERF("bc_hammer_slash", BcHammerSlash, 0, G_OTHERS),
	CC_PERF("bc_gathered_energy", BcGatheredEnergy, 0, G_OTHERS),
	CC_PERF("bc_freeze_pull", BcFreezePull, 0, G_OTHERS),
	CC_PERF("bc_freeze_lightning", BcFreezeLightning, 0, G_OTHERS),
	CC_PERF("tc_tee_trail", TcTeeTrail, 0, G_OTHERS),
	CC_PERF("tc_pet_show", TcPetShow, 0, G_OTHERS),
	CC_PERF("tc_rainbow_tees", TcRainbowTees, 0, G_OTHERS),
	CC_PERF("tc_rainbow_hook", TcRainbowHook, 0, G_OTHERS),
	CC_PERF("tc_rainbow_weapon", TcRainbowWeapon, 0, G_OTHERS),
	CC_PERF("tc_rainbow_others", TcRainbowOthers, 0, G_OTHERS),
	// simpler map
	CC_PERF("gfx_high_detail", GfxHighDetail, 0, G_MAP),
	CC_PERF("cl_showquads", ClShowQuads, 0, G_MAP),
	CC_PERF("tc_outline", TcOutline, 0, G_MAP),
	CC_PERF("cl_freeze_stars", ClFreezeStars, 0, G_MAP),
	// no particles (BestClient optimizer)
	CC_PERF("bc_optimizer", BcOptimizer, 1, G_PARTICLES),
	CC_PERF("bc_optimizer_disable_particles", BcOptimizerDisableParticles, 1, G_PARTICLES),
	// no vertical sync, no anti-aliasing
	CC_PERF("gfx_vsync", GfxVsync, 0, G_SYNC),
	CC_PERF("gfx_fsaa_samples", GfxFsaaSamples, 0, G_SYNC),
	// extras on the screen
	CC_PERF("bc_music_player", BcMusicPlayer, 0, G_HUD),
	CC_PERF("bc_chat_media_preview", BcChatMediaPreview, 0, G_HUD),
	CC_PERF("bc_chat_bubbles", BcChatBubbles, 0, G_HUD),
	CC_PERF("bc_keystrokes_keyboard", BcKeystrokesKeyboard, 0, G_HUD),
	CC_PERF("bc_keystrokes_mouse", BcKeystrokesMouse, 0, G_HUD),
	CC_PERF("bc_animations", BcAnimations, 0, G_HUD),
	CC_PERF("bc_menu_media_background", BcMenuMediaBackground, 0, G_HUD),
	CC_PERF("bc_gui_light", BcGuiLight, 1, G_HUD),
	// system
	CC_PERF("bc_high_process_priority", BcHighProcessPriority, 1, G_SYSTEM),
	CC_PERF("bc_discord_rpc", BcDiscordRPC, 0, G_SYSTEM),
};

#undef CC_PERF

constexpr const char *RESTORE_FILE = "chainsaw/perf_restore.txt";

struct SChange
{
	std::string m_Command;
	int m_Old;
	int m_Set;
};

bool s_Loaded = false;
bool s_Active = false; // the mode's values are in place
int s_Mask = 0; // groups that were applied
std::vector<SChange> s_vChanges;

int WantedMask()
{
	if(!g_Config.m_BcPerfMode)
		return 0;
	int Mask = 0;
	for(int g = 0; g < NUM_GROUPS; g++)
		if(*GroupSwitch(g))
			Mask |= 1 << g;
	return Mask;
}

const SEntry *FindEntry(const char *pCommand)
{
	for(const SEntry &Entry : s_aEntries)
		if(str_comp(Entry.m_pCommand, pCommand) == 0)
			return &Entry;
	return nullptr;
}

void Execute(IConsole *pConsole, const char *pCommand, int Value)
{
	char aBuf[128];
	str_format(aBuf, sizeof(aBuf), "%s %d", pCommand, Value);
	pConsole->ExecuteLine(aBuf, IConsole::CLIENT_ID_UNSPECIFIED);
}

void Save(IStorage *pStorage)
{
	pStorage->CreateFolder("chainsaw", IStorage::TYPE_SAVE);
	IOHANDLE File = pStorage->OpenFile(RESTORE_FILE, IOFLAG_WRITE, IStorage::TYPE_SAVE);
	if(!File)
		return;
	char aLine[160];
	str_format(aLine, sizeof(aLine), "# ChainsawClient: settings changed by the optimization mode (setting, your value, value of the mode)\nmask %d\n", s_Mask);
	io_write(File, aLine, str_length(aLine));
	for(const SChange &Change : s_vChanges)
	{
		str_format(aLine, sizeof(aLine), "%s %d %d\n", Change.m_Command.c_str(), Change.m_Old, Change.m_Set);
		io_write(File, aLine, str_length(aLine));
	}
	io_close(File);
}

void Load(IStorage *pStorage)
{
	s_Loaded = true;
	char *pText = pStorage->ReadFileStr(RESTORE_FILE, IStorage::TYPE_SAVE);
	if(!pText)
		return;
	s_Active = true;
	s_Mask = 0;
	s_vChanges.clear();
	char *pLine = pText;
	while(pLine && *pLine)
	{
		char *pEnd = (char *)str_find(pLine, "\n");
		if(pEnd)
			*pEnd = '\0';
		char aCommand[64];
		int Old = 0, Set = 0, Mask = 0;
		if(std::sscanf(pLine, "mask %d", &Mask) == 1)
			s_Mask = Mask;
		else if(pLine[0] != '#' && std::sscanf(pLine, "%63s %d %d", aCommand, &Old, &Set) == 3 && FindEntry(aCommand))
			s_vChanges.push_back({aCommand, Old, Set});
		pLine = pEnd ? pEnd + 1 : nullptr;
	}
	free(pText);
}

void ApplyGroups(IConsole *pConsole, int Mask)
{
	for(const SEntry &Entry : s_aEntries)
	{
		if(!(Mask & (1 << Entry.m_Group)) || *Entry.m_pValue == Entry.m_Set)
			continue;
		s_vChanges.push_back({Entry.m_pCommand, *Entry.m_pValue, Entry.m_Set});
		Execute(pConsole, Entry.m_pCommand, Entry.m_Set);
	}
}

void RestoreGroups(IConsole *pConsole, int Mask)
{
	std::vector<SChange> vKeep;
	for(const SChange &Change : s_vChanges)
	{
		const SEntry *pEntry = FindEntry(Change.m_Command.c_str());
		if(pEntry && !(Mask & (1 << pEntry->m_Group)))
		{
			vKeep.push_back(Change);
			continue;
		}
		// changed by hand while the mode was on: keep it
		if(pEntry && *pEntry->m_pValue == Change.m_Set)
			Execute(pConsole, pEntry->m_pCommand, Change.m_Old);
	}
	s_vChanges = vKeep;
}
} // namespace

namespace CcPerf
{
void Update(IConsole *pConsole, IStorage *pStorage)
{
	if(!s_Loaded)
		Load(pStorage);
	const int Want = WantedMask();
	if(Want == s_Mask && s_Active == (Want != 0))
		return;
	if(Want == 0)
	{
		// off: everything back
		RestoreGroups(pConsole, ~0);
		s_vChanges.clear();
		s_Active = false;
		s_Mask = 0;
		pStorage->RemoveFile(RESTORE_FILE, IStorage::TYPE_SAVE);
		return;
	}
	// on, or groups changed while on: give back the groups that were taken out, apply the new ones
	if(s_Active)
		RestoreGroups(pConsole, s_Mask & ~Want);
	else
		s_vChanges.clear();
	ApplyGroups(pConsole, s_Active ? Want & ~s_Mask : Want);
	s_Active = true;
	s_Mask = Want;
	Save(pStorage);
}

int NumChanged()
{
	return s_Active ? (int)s_vChanges.size() : 0;
}
} // namespace CcPerf
