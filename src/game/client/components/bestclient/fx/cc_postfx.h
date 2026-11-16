/* ChainsawClient: "shaders" - asks the renderer for the post-processing pass once per frame, after the world
   and before the HUD (engine/bc_postfx_params.h). Only the OpenGL 3.3 renderer draws it. */
#ifndef GAME_CLIENT_COMPONENTS_BESTCLIENT_FX_CC_POSTFX_H
#define GAME_CLIENT_COMPONENTS_BESTCLIENT_FX_CC_POSTFX_H

#include <game/client/component.h>

#include <string>

class CCcPostFx : public CComponent
{
public:
	int Sizeof() const override { return sizeof(*this); }
	void OnRender() override;

	static bool AnyEnabled();
	static bool RendererSupported();
	// ready sets of filters (08.11): FILTER_PRESET_*
	enum
	{
		FILTER_PRESET_OFF = 0,
		FILTER_PRESET_CINEMA,
		FILTER_PRESET_NEON,
		FILTER_PRESET_RETRO,
		FILTER_PRESET_HORROR,
		FILTER_PRESET_CARTOON,
		FILTER_PRESET_PAINTING,
		FILTER_PRESET_OLDGAME,
		FILTER_PRESET_SKETCH,
	};
	static void ApplyFilterPreset(int Preset);
	// 09.11: the order of the chain, own sets, the load
	static int FilterPosition(int Id);
	static void MoveFilter(int Id, int Dir);
	static std::string ExportFilters();
	static int ImportFilters(const char *pText);
	static bool SaveSlot(class IStorage *pStorage, int Slot);
	static bool LoadSlot(class IStorage *pStorage, int Slot);
	static int FilterCost(int Id);
	static int TotalCost();
	static int FilterIdOf(const int *pToggle);
};

#endif
