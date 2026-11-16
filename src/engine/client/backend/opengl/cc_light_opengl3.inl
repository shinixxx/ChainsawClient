/* ChainsawClient: light and shadows for the OpenGL 3.3 renderer (engine/cc_light_params.h).
   Two steps:
   1. the light picture, smaller than the screen: for every pixel the light of all lamps that reaches it. A ray walks
      block by block through the texture of the map to the lamp; a block on the way = shadow. Several rays to different
      points of the lamp, shifted from pixel to pixel, make the edges of the shadows soft.
   2. the screen drawn so far is copied (the texture of the post-processing pass) and drawn back: picture * light,
      plus the glow of the air in the light (haze and specks of dust).
   If anything fails the pass turns itself off and the game draws as before. */
#include <engine/client/cc_light_cmd.h>

static const char *const s_pCcLightCommon = R"(#version 330
uniform sampler2D uMat;
uniform vec2 uOrigin;
uniform vec2 uSize;
uniform float uTime;
in vec2 vUv;
out vec4 FragColor;

float Hash(vec2 p)
{
	vec3 p3 = fract(vec3(p.xyx) * 0.1031);
	p3 += dot(p3, p3.yzx + 33.33);
	return fract((p3.x + p3.y) * p3.z);
}

bool Solid(ivec2 p)
{
	if(p.y < 0)
		return false; // above the map is the sky
	p = clamp(p, ivec2(0), textureSize(uMat, 0) - 1);
	return texelFetch(uMat, p, 0).r > 0.5;
}
)";

static const char *const s_pCcLightMapBody = R"(
uniform vec4 uLamp[24]; // x, y, range, size
uniform vec3 uLampColor[24];
uniform int uNumLamps;
uniform int uRays;
uniform int uSunRays;
uniform vec4 uSun; // direction to the sun (xy), reach, softness; reach 0 = no sun
uniform vec3 uSunColor;

// How much of a lamp at To reaches the point w. A point inside a block is lit only close to the face that looks
// at the lamp (the light gets a little way in), so the lit sides of the blocks are bright and their middle stays dark.
float LightRay(vec2 w, vec2 To, bool Inside)
{
	vec2 Dir = To - w;
	float Len = length(Dir);
	Dir /= max(Len, 0.0001);
	ivec2 Ip = ivec2(floor(w));
	ivec2 Step = ivec2(Dir.x < 0.0 ? -1 : 1, Dir.y < 0.0 ? -1 : 1);
	vec2 Delta = 1.0 / max(abs(Dir), vec2(0.00001));
	vec2 Next = (vec2(Step) * 0.5 + 0.5 - (w - floor(w))) * vec2(Step) * Delta;
	float Walked = 0.0;
	for(int i = 0; i < 100; i++)
	{
		float t = min(Next.x, Next.y);
		if(Inside)
		{
			if(min(t, Len) > 0.85)
				return 0.0;
			Walked = t;
		}
		if(t >= Len)
			break;
		if(Next.x < Next.y)
		{
			Next.x += Delta.x;
			Ip.x += Step.x;
		}
		else
		{
			Next.y += Delta.y;
			Ip.y += Step.y;
		}
		bool Block = Solid(Ip);
		if(Inside)
			Inside = Block;
		else if(Block)
			return 0.0;
	}
	float In = 1.0 - Walked / 0.85;
	return In * In;
}

void main()
{
	vec2 w = uOrigin + vec2(vUv.x, 1.0 - vUv.y) * uSize;
	bool InBlock = Solid(ivec2(floor(w)));
	float Jit = fract(52.9829189 * fract(dot(gl_FragCoord.xy, vec2(0.06711056, 0.00583715))));
	float RaysF = float(uRays);
	vec3 Sum = vec3(0.0);
	for(int i = 0; i < uNumLamps; i++)
	{
		vec2 Pos = uLamp[i].xy;
		vec2 To = Pos - w;
		float Dist = length(To);
		float Fall = clamp(1.0 - Dist / uLamp[i].z, 0.0, 1.0);
		if(Fall <= 0.0)
			continue;
		Fall = Fall * Fall * (0.35 + 0.65 * Fall);
		vec2 Perp = vec2(-To.y, To.x) / max(Dist, 0.001);
		float Vis = 0.0;
		for(int k = 0; k < uRays; k++)
			Vis += LightRay(w, Pos + Perp * uLamp[i].w * ((float(k) + Jit) / RaysF - 0.5) * 2.0, InBlock);
		Sum += uLampColor[i] * (Vis / RaysF) * Fall;
	}
	if(uSun.z > 0.0)
	{
		vec2 Perp = vec2(-uSun.y, uSun.x);
		float Vis = 0.0;
		float SunRaysF = float(uSunRays);
		for(int k = 0; k < uSunRays; k++)
			Vis += LightRay(w, w + (uSun.xy + Perp * uSun.w * 0.02 * ((float(k) + Jit) / SunRaysF - 0.5) * 2.0) * uSun.z, InBlock);
		Sum += uSunColor * (Vis / SunRaysF);
	}
	FragColor = vec4(Sum, 1.0);
}
)";

static const char *const s_pCcLightDrawBody = R"(
uniform sampler2D uScene;
uniform sampler2D uLightTex;
uniform vec3 uAmbient;
uniform vec3 uFx; // haze, dust, dark corners
uniform vec2 uLightTexel;

float Noise(vec2 p)
{
	vec2 i = floor(p), f = fract(p);
	f = f * f * (3.0 - 2.0 * f);
	return mix(mix(Hash(i), Hash(i + vec2(1.0, 0.0)), f.x), mix(Hash(i + vec2(0.0, 1.0)), Hash(i + vec2(1.0, 1.0)), f.x), f.y);
}

void main()
{
	vec2 w = uOrigin + vec2(vUv.x, 1.0 - vUv.y) * uSize;
	vec3 Scene = textureLod(uScene, vUv, 0.0).rgb;
	Scene *= Scene;
	// four taps around the pixel: hides the grain of the shifted rays
	vec3 Light = (textureLod(uLightTex, vUv + uLightTexel * vec2(0.75, 0.75), 0.0).rgb + textureLod(uLightTex, vUv + uLightTexel * vec2(-0.75, 0.75), 0.0).rgb +
			     textureLod(uLightTex, vUv + uLightTexel * vec2(0.75, -0.75), 0.0).rgb + textureLod(uLightTex, vUv + uLightTexel * vec2(-0.75, -0.75), 0.0).rgb) *
		     0.25;
	vec3 c = Scene * (uAmbient + Light);
	if(!Solid(ivec2(floor(w))))
	{
		// the air in the light: slow clouds of haze and specks of dust that drift
		float Haze = 0.0;
		if(uFx.x > 0.0)
		{
			vec2 q = w * 0.55 + vec2(uTime * 0.03, uTime * 0.02);
			Haze = (0.86 + 0.18 * Noise(q * 0.5) + 0.08 * Noise(q * 1.1 + 7.0)) * uFx.x;
		}
		float Dust = 0.0;
		if(uFx.y > 0.0)
		{
			vec2 q = w * 6.0 + vec2(uTime * 0.10, uTime * 0.16) + vec2(Noise(w * 0.7 + uTime * 0.05), Noise(w * 0.7 + 9.0 - uTime * 0.04)) * 1.5;
			float Twinkle = 0.6 + 0.4 * sin(uTime * 2.0 + Hash(floor(q)) * 40.0);
			Dust = smoothstep(0.955, 1.0, Hash(floor(q))) * (1.0 - smoothstep(0.03, 0.12, length(fract(q) - 0.5))) * Twinkle * uFx.y;
		}
		c += Light * (0.17 * Haze + 0.75 * Dust);
	}
	c *= 1.0 - uFx.z * smoothstep(0.35, 0.95, length(vUv - 0.5) * 1.25);
	c = c / (1.0 + c * 0.25) * 1.25;
	FragColor = vec4(sqrt(clamp(c, 0.0, 1.0)), 1.0);
}
)";

static TWGLuint CcLightProgram(const char *pBody)
{
	char aSource[12288];
	str_copy(aSource, s_pCcLightCommon);
	str_append(aSource, pBody);
	const TWGLuint Vertex = BcPostFxCompile(GL_VERTEX_SHADER, s_pBcPostFxVertex);
	const TWGLuint Fragment = BcPostFxCompile(GL_FRAGMENT_SHADER, aSource);
	if(Vertex == 0 || Fragment == 0)
	{
		if(Vertex)
			glDeleteShader(Vertex);
		if(Fragment)
			glDeleteShader(Fragment);
		return 0;
	}
	const TWGLuint Program = glCreateProgram();
	glAttachShader(Program, Vertex);
	glAttachShader(Program, Fragment);
	glLinkProgram(Program);
	glDeleteShader(Vertex);
	glDeleteShader(Fragment);
	GLint Ok = 0;
	glGetProgramiv(Program, GL_LINK_STATUS, &Ok);
	if(!Ok)
	{
		char aLog[1024] = "";
		glGetProgramInfoLog(Program, sizeof(aLog), nullptr, aLog);
		log_error("chainsaw", "light shader link failed: %s", aLog);
		glDeleteProgram(Program);
		return 0;
	}
	return Program;
}

bool CCommandProcessorFragment_OpenGL3_3::CcLightEnsure(int Scale)
{
	if(m_CcLightFailed || !BcPostFxEnsure())
		return false;
	if(m_CcLightMapProgram == 0)
	{
		m_CcLightMapProgram = CcLightProgram(s_pCcLightMapBody);
		m_CcLightDrawProgram = m_CcLightMapProgram ? CcLightProgram(s_pCcLightDrawBody) : 0;
		if(m_CcLightMapProgram == 0 || m_CcLightDrawProgram == 0)
		{
			m_CcLightFailed = true;
			return false;
		}
	}
	const int W = m_BcPostFxW / Scale > 8 ? m_BcPostFxW / Scale : 8, H = m_BcPostFxH / Scale > 8 ? m_BcPostFxH / Scale : 8;
	if(m_CcLightTex == 0 || m_CcLightW != W || m_CcLightH != H)
	{
		if(m_CcLightTex != 0)
			glDeleteTextures(1, &m_CcLightTex);
		if(m_CcLightFbo != 0)
			glDeleteFramebuffers(1, &m_CcLightFbo);
		glGenTextures(1, &m_CcLightTex);
		glBindTexture(GL_TEXTURE_2D, m_CcLightTex);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, W, H, 0, GL_RGBA, GL_FLOAT, nullptr);
		glGenFramebuffers(1, &m_CcLightFbo);
		glBindFramebuffer(GL_FRAMEBUFFER, m_CcLightFbo);
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_CcLightTex, 0);
		const GLenum Status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
		glBindFramebuffer(GL_FRAMEBUFFER, 0);
		glBindTexture(GL_TEXTURE_2D, 0);
		if(Status != GL_FRAMEBUFFER_COMPLETE)
		{
			log_error("chainsaw", "light framebuffer is not complete (0x%x)", (unsigned)Status);
			m_CcLightFailed = true;
			return false;
		}
		m_CcLightW = W;
		m_CcLightH = H;
	}
	return true;
}

void CCommandProcessorFragment_OpenGL3_3::Cmd_CcLight(const SCommand_CcLight *pCommand)
{
	const SCcLightParams &P = pCommand->m_Params;
	if(P.m_MapTexture < 0 || P.m_MapTexture >= (int)m_vTextures.size() || m_vTextures[P.m_MapTexture].m_Tex == 0)
		return;
	static const int s_aScale[4] = {6, 4, 3, 2};
	static const int s_aRays[4] = {2, 3, 5, 10};
	static const int s_aSunRays[4] = {1, 2, 3, 6};
	const int Quality = P.m_Quality < 0 ? 0 : (P.m_Quality > 3 ? 3 : P.m_Quality);
	if(!CcLightEnsure(s_aScale[Quality]))
		return;
	const int W = m_BcPostFxW, H = m_BcPostFxH;

	if(m_LastClipEnable)
	{
		glDisable(GL_SCISSOR_TEST);
		m_LastClipEnable = false;
	}
	glDisable(GL_BLEND);
	if(m_BcPostFxVao == 0)
		glGenVertexArrays(1, &m_BcPostFxVao);
	glBindVertexArray(m_BcPostFxVao);
	glActiveTexture(GL_TEXTURE0);
	glBindSampler(0, 0);
	glBindTexture(GL_TEXTURE_2D, m_vTextures[P.m_MapTexture].m_Tex);

	// 1. the light picture
	glBindFramebuffer(GL_FRAMEBUFFER, m_CcLightFbo);
	glViewport(0, 0, m_CcLightW, m_CcLightH);
	glUseProgram(m_CcLightMapProgram);
	{
		const TWGLuint Prog = m_CcLightMapProgram;
		glUniform1i(glGetUniformLocation(Prog, "uMat"), 0);
		glUniform2f(glGetUniformLocation(Prog, "uOrigin"), P.m_OriginX, P.m_OriginY);
		glUniform2f(glGetUniformLocation(Prog, "uSize"), P.m_SizeX, P.m_SizeY);
		glUniform1f(glGetUniformLocation(Prog, "uTime"), P.m_Time);
		const int Num = P.m_NumLamps < 0 ? 0 : (P.m_NumLamps > CC_LIGHT_MAX_LAMPS ? (int)CC_LIGHT_MAX_LAMPS : P.m_NumLamps);
		float aLamp[CC_LIGHT_MAX_LAMPS * 4] = {};
		float aColor[CC_LIGHT_MAX_LAMPS * 3] = {};
		for(int i = 0; i < Num; i++)
		{
			aLamp[i * 4 + 0] = P.m_aLamps[i].m_X;
			aLamp[i * 4 + 1] = P.m_aLamps[i].m_Y;
			aLamp[i * 4 + 2] = P.m_aLamps[i].m_Range;
			aLamp[i * 4 + 3] = P.m_aLamps[i].m_Size;
			aColor[i * 3 + 0] = P.m_aLamps[i].m_aColor[0];
			aColor[i * 3 + 1] = P.m_aLamps[i].m_aColor[1];
			aColor[i * 3 + 2] = P.m_aLamps[i].m_aColor[2];
		}
		glUniform4fv(glGetUniformLocation(Prog, "uLamp"), CC_LIGHT_MAX_LAMPS, aLamp);
		glUniform3fv(glGetUniformLocation(Prog, "uLampColor"), CC_LIGHT_MAX_LAMPS, aColor);
		glUniform1i(glGetUniformLocation(Prog, "uNumLamps"), Num);
		glUniform1i(glGetUniformLocation(Prog, "uRays"), s_aRays[Quality]);
		glUniform1i(glGetUniformLocation(Prog, "uSunRays"), s_aSunRays[Quality]);
		glUniform4f(glGetUniformLocation(Prog, "uSun"), P.m_aSunDir[0], P.m_aSunDir[1], P.m_Sun ? P.m_SunReach : 0.0f, P.m_SunSoft);
		glUniform3f(glGetUniformLocation(Prog, "uSunColor"), P.m_aSunColor[0], P.m_aSunColor[1], P.m_aSunColor[2]);
	}
	glDrawArrays(GL_TRIANGLES, 0, 3);

	// 2. the screen so far into the texture (a blit also resolves multisampling), then back with the light
	glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
	glBindFramebuffer(GL_DRAW_FRAMEBUFFER, m_BcPostFxFbo);
	glBlitFramebuffer(m_ViewportX, m_ViewportY, m_ViewportX + W, m_ViewportY + H, 0, 0, W, H, GL_COLOR_BUFFER_BIT, GL_NEAREST);
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	glViewport(m_ViewportX, m_ViewportY, W, H);
	glUseProgram(m_CcLightDrawProgram);
	{
		const TWGLuint Prog = m_CcLightDrawProgram;
		glUniform1i(glGetUniformLocation(Prog, "uMat"), 0);
		glUniform1i(glGetUniformLocation(Prog, "uScene"), 1);
		glUniform1i(glGetUniformLocation(Prog, "uLightTex"), 2);
		glUniform2f(glGetUniformLocation(Prog, "uOrigin"), P.m_OriginX, P.m_OriginY);
		glUniform2f(glGetUniformLocation(Prog, "uSize"), P.m_SizeX, P.m_SizeY);
		glUniform1f(glGetUniformLocation(Prog, "uTime"), P.m_Time);
		glUniform3f(glGetUniformLocation(Prog, "uAmbient"), P.m_aAmbient[0], P.m_aAmbient[1], P.m_aAmbient[2]);
		glUniform3f(glGetUniformLocation(Prog, "uFx"), P.m_Haze, P.m_Dust, P.m_Vignette);
		glUniform2f(glGetUniformLocation(Prog, "uLightTexel"), 1.0f / m_CcLightW, 1.0f / m_CcLightH);
	}
	glActiveTexture(GL_TEXTURE1);
	glBindSampler(1, 0);
	glBindTexture(GL_TEXTURE_2D, m_BcPostFxTex);
	glActiveTexture(GL_TEXTURE2);
	glBindSampler(2, 0);
	glBindTexture(GL_TEXTURE_2D, m_CcLightTex);
	glDrawArrays(GL_TRIANGLES, 0, 3);

	// leave the state the way the renderer expects it
	glBindTexture(GL_TEXTURE_2D, 0);
	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_2D, 0);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, 0);
	glBindVertexArray(0);
	glUseProgram(0);
	m_LastProgramId = 0;
	m_LastBlendMode = EBlendMode::NONE;
}
