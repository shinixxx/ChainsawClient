/* ChainsawClient: the render command for realistic blocks. Like light and post-processing it lives in the GL platform
   group, so renderers that do not know it (Vulkan, null) let it fall through to the SDL fragment, which ignores it. */
#ifndef ENGINE_CLIENT_CC_BLOCKS_CMD_H
#define ENGINE_CLIENT_CC_BLOCKS_CMD_H

#include "graphics_threaded.h"

#include <engine/cc_blocks_params.h>

enum
{
	CMD_CC_BLOCKS = CCommandBuffer::CMDGROUP_PLATFORM_GL + 902,
};

struct SCommand_CcBlocks : public CCommandBuffer::SCommand
{
	SCommand_CcBlocks() :
		SCommand(CMD_CC_BLOCKS) {}
	SCcBlocksParams m_Params;
};

#endif
