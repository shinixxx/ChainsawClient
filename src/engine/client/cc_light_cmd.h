/* ChainsawClient: the render command of the light pass. Like the post-processing command it lives in the GL platform
   group, so renderers that do not know it (Vulkan, null) let it fall through to the SDL fragment, which ignores it. */
#ifndef ENGINE_CLIENT_CC_LIGHT_CMD_H
#define ENGINE_CLIENT_CC_LIGHT_CMD_H

#include "graphics_threaded.h"

#include <engine/cc_light_params.h>

enum
{
	CMD_CC_LIGHT = CCommandBuffer::CMDGROUP_PLATFORM_GL + 901,
};

struct SCommand_CcLight : public CCommandBuffer::SCommand
{
	SCommand_CcLight() :
		SCommand(CMD_CC_LIGHT) {}
	SCcLightParams m_Params;
};

#endif
