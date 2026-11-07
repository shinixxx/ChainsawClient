/* ChainsawClient: post-processing pass for the OpenGL 3.3 renderer ("shaders" in the menu).
   The picture drawn so far is copied into a texture (with mip maps, the small levels give a cheap bloom),
   then drawn back through one fragment shader: color grading, bloom, sharpening, vignette, grain, chromatic
   aberration. If anything fails the pass turns itself off and the game draws as before. */
#include <engine/client/bc_postfx_cmd.h>

#include <base/log.h>

static const char *const s_pBcPostFxVertex = R"(#version 330
out vec2 vUv;
void main()
{
	vec2 p = vec2(float((gl_VertexID << 1) & 2), float(gl_VertexID & 2));
	vUv = p;
	gl_Position = vec4(p * 2.0 - 1.0, 0.0, 1.0);
}
)";

static const char *const s_pBcPostFxFragment = R"(#version 330
uniform sampler2D uTex;
uniform vec2 uTexel;
uniform float uSaturation;
uniform float uVibrance;
uniform float uContrast;
uniform float uBrightness;
uniform float uWarmth;
uniform float uVignette;
uniform float uBloom;
uniform float uThreshold;
uniform float uRadius;
uniform float uSharpen;
uniform float uGrain;
uniform float uChroma;
uniform float uTime;
uniform vec3 uTint;
in vec2 vUv;
out vec4 FragColor;

vec3 Fetch(vec2 Uv) { return textureLod(uTex, Uv, 0.0).rgb; }

void main()
{
	vec2 Uv = vUv;
	vec2 FromCenter = Uv - 0.5;
	vec3 c;
	if(uChroma > 0.0)
	{
		vec2 Shift = FromCenter * dot(FromCenter, FromCenter) * uChroma * 0.06;
		c = vec3(textureLod(uTex, Uv + Shift, 0.0).r, textureLod(uTex, Uv, 0.0).g, textureLod(uTex, Uv - Shift, 0.0).b);
	}
	else
		c = Fetch(Uv);

	if(uSharpen > 0.0)
	{
		vec3 n = Fetch(Uv + vec2(uTexel.x, 0.0)) + Fetch(Uv - vec2(uTexel.x, 0.0)) + Fetch(Uv + vec2(0.0, uTexel.y)) + Fetch(Uv - vec2(0.0, uTexel.y));
		c = max(c + (c * 4.0 - n) * uSharpen * 0.25, 0.0);
	}

	if(uBloom > 0.0)
	{
		vec3 b = textureLod(uTex, Uv, 2.0 + uRadius).rgb * 0.45 + textureLod(uTex, Uv, 3.5 + uRadius).rgb * 0.35 + textureLod(uTex, Uv, 5.0 + uRadius).rgb * 0.2;
		float Bright = max(b.r, max(b.g, b.b));
		float Knee = smoothstep(uThreshold, min(uThreshold + 0.35, 1.0), Bright);
		c += b * Knee * uBloom;
	}

	c *= uBrightness * uTint;
	c *= vec3(1.0 + 0.08 * uWarmth, 1.0 + 0.015 * uWarmth, 1.0 - 0.08 * uWarmth);
	c = (c - 0.5) * uContrast + 0.5;

	float Luma = dot(c, vec3(0.2126, 0.7152, 0.0722));
	c = mix(vec3(Luma), c, uSaturation);
	float Spread = max(c.r, max(c.g, c.b)) - min(c.r, min(c.g, c.b));
	c = mix(vec3(Luma), c, 1.0 + uVibrance * (1.0 - clamp(Spread * 2.0, 0.0, 1.0)));

	if(uVignette > 0.0)
		c *= 1.0 - uVignette * smoothstep(0.3, 0.95, length(FromCenter * vec2(1.25, 1.0)) * 1.25);

	if(uGrain > 0.0)
	{
		float Noise = fract(sin(dot(Uv * 1000.0 + vec2(uTime * 17.0, uTime * 31.0), vec2(12.9898, 78.233))) * 43758.5453);
		c += (Noise - 0.5) * uGrain * 0.08;
	}
	FragColor = vec4(clamp(c, 0.0, 1.0), 1.0);
}
)";

static TWGLuint BcPostFxCompile(GLenum Type, const char *pSource)
{
	const TWGLuint Shader = glCreateShader(Type);
	glShaderSource(Shader, 1, &pSource, nullptr);
	glCompileShader(Shader);
	GLint Ok = 0;
	glGetShaderiv(Shader, GL_COMPILE_STATUS, &Ok);
	if(!Ok)
	{
		char aLog[1024] = "";
		glGetShaderInfoLog(Shader, sizeof(aLog), nullptr, aLog);
		log_error("chainsaw", "post-processing shader failed: %s", aLog);
		glDeleteShader(Shader);
		return 0;
	}
	return Shader;
}

bool CCommandProcessorFragment_OpenGL3_3::BcPostFxEnsure()
{
	if(m_BcPostFxFailed)
		return false;
	if(m_BcPostFxProgram == 0)
	{
		const TWGLuint Vertex = BcPostFxCompile(GL_VERTEX_SHADER, s_pBcPostFxVertex);
		const TWGLuint Fragment = BcPostFxCompile(GL_FRAGMENT_SHADER, s_pBcPostFxFragment);
		if(Vertex == 0 || Fragment == 0)
		{
			m_BcPostFxFailed = true;
			return false;
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
			log_error("chainsaw", "post-processing shader link failed: %s", aLog);
			glDeleteProgram(Program);
			m_BcPostFxFailed = true;
			return false;
		}
		m_BcPostFxProgram = Program;
		const char *apNames[] = {"uTex", "uTexel", "uSaturation", "uVibrance", "uContrast", "uBrightness", "uWarmth", "uVignette", "uBloom", "uThreshold", "uRadius", "uSharpen", "uGrain", "uChroma", "uTime", "uTint"};
		static_assert(std::size(apNames) <= std::size(m_aBcPostFxLoc));
		for(size_t i = 0; i < std::size(apNames); i++)
			m_aBcPostFxLoc[i] = glGetUniformLocation(Program, apNames[i]);
		glGenVertexArrays(1, &m_BcPostFxVao);
	}
	const int W = (int)m_CanvasWidth, H = (int)m_CanvasHeight;
	if(W <= 0 || H <= 0)
		return false;
	if(m_BcPostFxTex == 0 || m_BcPostFxW != W || m_BcPostFxH != H)
	{
		if(m_BcPostFxTex != 0)
			glDeleteTextures(1, &m_BcPostFxTex);
		if(m_BcPostFxFbo != 0)
			glDeleteFramebuffers(1, &m_BcPostFxFbo);
		glGenTextures(1, &m_BcPostFxTex);
		glBindTexture(GL_TEXTURE_2D, m_BcPostFxTex);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, W, H, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
		glGenerateMipmap(GL_TEXTURE_2D);
		glGenFramebuffers(1, &m_BcPostFxFbo);
		glBindFramebuffer(GL_FRAMEBUFFER, m_BcPostFxFbo);
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_BcPostFxTex, 0);
		const GLenum Status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
		glBindFramebuffer(GL_FRAMEBUFFER, 0);
		glBindTexture(GL_TEXTURE_2D, 0);
		if(Status != GL_FRAMEBUFFER_COMPLETE)
		{
			log_error("chainsaw", "post-processing framebuffer is not complete (0x%x)", (unsigned)Status);
			m_BcPostFxFailed = true;
			return false;
		}
		m_BcPostFxW = W;
		m_BcPostFxH = H;
	}
	return true;
}

void CCommandProcessorFragment_OpenGL3_3::Cmd_BcPostFx(const SCommand_BcPostFx *pCommand)
{
	if(!BcPostFxEnsure())
		return;
	const SBcPostFxParams &P = pCommand->m_Params;
	const int W = m_BcPostFxW, H = m_BcPostFxH;

	if(m_LastClipEnable)
	{
		glDisable(GL_SCISSOR_TEST);
		m_LastClipEnable = false;
	}
	// the picture so far into the texture (a blit also resolves multisampling)
	glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
	glBindFramebuffer(GL_DRAW_FRAMEBUFFER, m_BcPostFxFbo);
	glBlitFramebuffer(m_ViewportX, m_ViewportY, m_ViewportX + W, m_ViewportY + H, 0, 0, W, H, GL_COLOR_BUFFER_BIT, GL_NEAREST);
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	glActiveTexture(GL_TEXTURE0);
	glBindSampler(0, 0);
	glBindTexture(GL_TEXTURE_2D, m_BcPostFxTex);
	if(P.m_Bloom > 0.0f)
		glGenerateMipmap(GL_TEXTURE_2D);

	glViewport(m_ViewportX, m_ViewportY, W, H);
	glDisable(GL_BLEND);
	glUseProgram(m_BcPostFxProgram);
	glUniform1i(m_aBcPostFxLoc[0], 0);
	glUniform2f(m_aBcPostFxLoc[1], 1.0f / W, 1.0f / H);
	glUniform1f(m_aBcPostFxLoc[2], P.m_Saturation);
	glUniform1f(m_aBcPostFxLoc[3], P.m_Vibrance);
	glUniform1f(m_aBcPostFxLoc[4], P.m_Contrast);
	glUniform1f(m_aBcPostFxLoc[5], P.m_Brightness);
	glUniform1f(m_aBcPostFxLoc[6], P.m_Warmth);
	glUniform1f(m_aBcPostFxLoc[7], P.m_Vignette);
	glUniform1f(m_aBcPostFxLoc[8], P.m_Bloom);
	glUniform1f(m_aBcPostFxLoc[9], P.m_BloomThreshold);
	glUniform1f(m_aBcPostFxLoc[10], P.m_BloomRadius);
	glUniform1f(m_aBcPostFxLoc[11], P.m_Sharpen);
	glUniform1f(m_aBcPostFxLoc[12], P.m_Grain);
	glUniform1f(m_aBcPostFxLoc[13], P.m_Chroma);
	glUniform1f(m_aBcPostFxLoc[14], P.m_Time);
	glUniform3f(m_aBcPostFxLoc[15], P.m_aTint[0], P.m_aTint[1], P.m_aTint[2]);
	glBindVertexArray(m_BcPostFxVao);
	glDrawArrays(GL_TRIANGLES, 0, 3);

	// leave the state the way the renderer expects it
	glBindVertexArray(0);
	glBindTexture(GL_TEXTURE_2D, 0);
	glUseProgram(0);
	m_LastProgramId = 0;
	m_LastBlendMode = EBlendMode::NONE;
}
