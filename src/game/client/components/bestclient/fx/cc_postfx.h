/* ChainsawClient: "shaders" - asks the renderer for the post-processing pass once per frame, after the world
   and before the HUD (engine/bc_postfx_params.h). Only the OpenGL 3.3 renderer draws it. */
#ifndef GAME_CLIENT_COMPONENTS_BESTCLIENT_FX_CC_POSTFX_H
#define GAME_CLIENT_COMPONENTS_BESTCLIENT_FX_CC_POSTFX_H

#include <game/client/component.h>

class CCcPostFx : public CComponent
{
public:
	int Sizeof() const override { return sizeof(*this); }
	void OnRender() override;

	static bool AnyEnabled();
	static bool RendererSupported();
};

#endif
