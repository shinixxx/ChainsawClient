/* ChainsawClient: the render command of the post-processing pass. It lives in the GL platform group, so renderers
   that do not know it (Vulkan, null) let it fall through to the SDL fragment, which ignores it. */
#ifndef ENGINE_CLIENT_BC_POSTFX_CMD_H
#define ENGINE_CLIENT_BC_POSTFX_CMD_H

#include "graphics_threaded.h"

#include <engine/bc_postfx_params.h>

enum
{
	CMD_BC_POSTFX = CCommandBuffer::CMDGROUP_PLATFORM_GL + 900,
};

struct SCommand_BcPostFx : public CCommandBuffer::SCommand
{
	SCommand_BcPostFx() :
		SCommand(CMD_BC_POSTFX) {}
	SBcPostFxParams m_Params;
};

#endif
