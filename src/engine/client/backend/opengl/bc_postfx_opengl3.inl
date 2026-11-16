/* ChainsawClient: post-processing pass for the OpenGL 3.3 renderer ("shaders" in the menu).
   The picture drawn so far is copied into a texture (with mip maps, the small levels give a cheap bloom),
   then drawn back through one fragment shader: color grading, bloom, sharpening, vignette, grain, chromatic
   aberration. If anything fails the pass turns itself off and the game draws as before. */
#include <engine/client/bc_postfx_cmd.h>

#include <base/log.h>
#include <base/str.h>

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


/* ---- 08.11: the filter chain (BC_FILTER_*, engine/bc_postfx_params.h) ----
   Every enabled filter: the picture is copied into the texture again and drawn back through the filter's own shader.
   uP = the 4 numbers of the filter, uColor = its color. A filter whose shader fails is skipped (the rest still work). */
static const char *const s_pBcFilterHeader = R"(#version 330
uniform sampler2D uTex;
uniform vec2 uTexel;
uniform vec4 uP;
uniform vec3 uColor;
uniform float uTime;
in vec2 vUv;
out vec4 FragColor;
vec3 T(vec2 Uv) { return textureLod(uTex, clamp(Uv, vec2(0.0), vec2(1.0)), 0.0).rgb; }
float Luma(vec3 c) { return dot(c, vec3(0.2126, 0.7152, 0.0722)); }
float Hash(vec2 p) { return fract(sin(dot(p, vec2(12.9898, 78.233))) * 43758.5453); }
float Sobel(vec2 Uv, float Width)
{
	vec2 d = uTexel * Width;
	float tl = Luma(T(Uv + vec2(-d.x, d.y))), t = Luma(T(Uv + vec2(0.0, d.y))), tr = Luma(T(Uv + d));
	float l = Luma(T(Uv - vec2(d.x, 0.0))), r = Luma(T(Uv + vec2(d.x, 0.0)));
	float bl = Luma(T(Uv - d)), b = Luma(T(Uv - vec2(0.0, d.y))), br = Luma(T(Uv + vec2(d.x, -d.y)));
	float gx = -tl - 2.0 * l - bl + tr + 2.0 * r + br;
	float gy = -tl - 2.0 * t - tr + bl + 2.0 * b + br;
	return sqrt(gx * gx + gy * gy);
}
)";

// in the order of BC_FILTER_*
static const char *const s_apBcFilterBody[] = {
	// GAMMA: uP.x = gamma
	R"(void main() { FragColor = vec4(pow(T(vUv), vec3(1.0 / max(uP.x, 0.05))), 1.0); })",
	// HUE: uP.x = turn (0..1 of a full circle)
	R"(void main()
{
	vec3 c = T(vUv);
	float a = uP.x * 6.2831853;
	vec3 k = vec3(0.57735);
	c = c * cos(a) + cross(k, c) * sin(a) + k * dot(k, c) * (1.0 - cos(a));
	FragColor = vec4(clamp(c, 0.0, 1.0), 1.0);
})",
	// BW: uP.x = amount, uP.y = contrast boost
	R"(void main()
{
	vec3 c = T(vUv);
	float l = Luma(c);
	l = clamp((l - 0.5) * (1.0 + uP.y) + 0.5, 0.0, 1.0);
	FragColor = vec4(mix(c, vec3(l), uP.x), 1.0);
})",
	// SEPIA: uP.x = amount
	R"(void main()
{
	vec3 c = T(vUv);
	vec3 s = vec3(dot(c, vec3(0.393, 0.769, 0.189)), dot(c, vec3(0.349, 0.686, 0.168)), dot(c, vec3(0.272, 0.534, 0.131)));
	FragColor = vec4(mix(c, min(s, 1.0), uP.x), 1.0);
})",
	// INVERT: uP.x = amount
	R"(void main() { vec3 c = T(vUv); FragColor = vec4(mix(c, 1.0 - c, uP.x), 1.0); })",
	// THERMAL: uP.x = amount
	R"(void main()
{
	vec3 c = T(vUv);
	float l = Luma(c);
	vec3 h = l < 0.25 ? mix(vec3(0.0, 0.0, 0.25), vec3(0.45, 0.0, 0.6), l / 0.25)
		: l < 0.5 ? mix(vec3(0.45, 0.0, 0.6), vec3(0.9, 0.1, 0.1), (l - 0.25) / 0.25)
		: l < 0.75 ? mix(vec3(0.9, 0.1, 0.1), vec3(1.0, 0.85, 0.0), (l - 0.5) / 0.25)
		: mix(vec3(1.0, 0.85, 0.0), vec3(1.0), (l - 0.75) / 0.25);
	FragColor = vec4(mix(c, h, uP.x), 1.0);
})",
	// NIGHTVISION: uP.x = amount, uP.y = noise
	R"(void main()
{
	vec3 c = T(vUv);
	float l = Luma(c) * 1.8 + 0.06;
	l += (Hash(vUv * 900.0 + uTime * 37.0) - 0.5) * 0.25 * uP.y;
	l *= 0.92 + 0.08 * sin(vUv.y * 900.0);
	vec2 v = vUv - 0.5;
	l *= 1.0 - smoothstep(0.3, 0.72, length(v * vec2(1.4, 1.0)));
	FragColor = vec4(mix(c, vec3(0.15, 1.0, 0.25) * l, uP.x), 1.0);
})",
	// SPLIT: uP.x = hue of the shadows, uP.y = hue of the lights (0..1), uP.z = amount
	R"(vec3 HueColor(float h)
{
	return clamp(abs(mod(h * 6.0 + vec3(0.0, 4.0, 2.0), 6.0) - 3.0) - 1.0, 0.0, 1.0);
}
void main()
{
	vec3 c = T(vUv);
	float l = Luma(c);
	vec3 Tone = mix(HueColor(uP.x), HueColor(uP.y), smoothstep(0.15, 0.85, l));
	vec3 Toned = c * mix(vec3(1.0), Tone * 1.6, 0.5);
	FragColor = vec4(mix(c, clamp(Toned, 0.0, 1.0), uP.z), 1.0);
})",
	// HORROR: uP.x = amount, uP.y = heartbeat
	R"(void main()
{
	vec3 c = T(vUv);
	float l = Luma(c);
	vec3 h = vec3(l * 1.05, l * 0.55, l * 0.5);
	h = (h - 0.5) * 1.25 + 0.45;
	float Beat = pow(abs(sin(uTime * 2.4)), 16.0) + 0.6 * pow(abs(sin(uTime * 2.4 + 0.5)), 16.0);
	vec2 v = vUv - 0.5;
	float Vig = smoothstep(0.25, 0.8, length(v * vec2(1.3, 1.0)));
	h *= 1.0 - Vig * (0.75 + 0.2 * Beat * uP.y);
	h += vec3(0.25, 0.0, 0.0) * Vig * Beat * uP.y;
	h += (Hash(vUv * 700.0 + uTime * 13.0) - 0.5) * 0.06;
	FragColor = vec4(mix(c, clamp(h, 0.0, 1.0), uP.x), 1.0);
})",
	// WAVES: uP.x = strength, uP.y = speed, uP.z = size
	R"(void main()
{
	vec2 Uv = vUv;
	float f = 6.0 + 30.0 * uP.z;
	Uv.x += sin(Uv.y * f + uTime * 3.0 * uP.y) * 0.006 * uP.x;
	Uv.y += cos(Uv.x * f * 0.8 + uTime * 2.4 * uP.y) * 0.006 * uP.x;
	FragColor = vec4(T(Uv), 1.0);
})",
	// FISHEYE: uP.x = strength
	R"(void main()
{
	vec2 v = vUv - 0.5;
	float r2 = dot(v * vec2(1.6, 1.0), v * vec2(1.6, 1.0));
	vec2 Uv = 0.5 + v * (1.0 - 0.45 * uP.x) * (1.0 + uP.x * 1.1 * r2);
	if(Uv.x < 0.0 || Uv.x > 1.0 || Uv.y < 0.0 || Uv.y > 1.0)
	{
		FragColor = vec4(0.0, 0.0, 0.0, 1.0);
		return;
	}
	FragColor = vec4(T(Uv), 1.0);
})",
	// DRUNK: uP.x = strength
	R"(void main()
{
	float s = uP.x;
	vec2 v = vUv - 0.5;
	float a = sin(uTime * 0.7) * 0.02 * s;
	v = mat2(cos(a), -sin(a), sin(a), cos(a)) * v * (1.0 - 0.03 * s * (0.5 + 0.5 * sin(uTime * 0.9)));
	vec2 Uv = v + 0.5;
	vec2 Off = vec2(sin(uTime * 1.3), cos(uTime * 1.7)) * 0.008 * s;
	vec3 c = mix(T(Uv + Off), T(Uv - Off), 0.5);
	FragColor = vec4(c, 1.0);
})",
	// SWIRL: uP.x = strength, uP.y = radius
	R"(void main()
{
	vec2 v = vUv - 0.5;
	v.x *= uTexel.y / uTexel.x;
	float r = length(v);
	float Radius = max(uP.y, 0.05);
	float a = uP.x * 4.0 * pow(1.0 - smoothstep(0.0, Radius, r), 2.0) * (1.0 + 0.15 * sin(uTime * 0.8));
	v = mat2(cos(a), -sin(a), sin(a), cos(a)) * v;
	v.x /= uTexel.y / uTexel.x;
	FragColor = vec4(T(v + 0.5), 1.0);
})",
	// BLUR: uP.x = radius (mip level)
	R"(void main()
{
	float Lod = uP.x;
	vec2 d = uTexel * exp2(Lod) * 0.8;
	vec3 c = textureLod(uTex, vUv, Lod).rgb * 0.25;
	c += (textureLod(uTex, vUv + vec2(d.x, 0.0), Lod).rgb + textureLod(uTex, vUv - vec2(d.x, 0.0), Lod).rgb + textureLod(uTex, vUv + vec2(0.0, d.y), Lod).rgb + textureLod(uTex, vUv - vec2(0.0, d.y), Lod).rgb) * 0.125;
	c += (textureLod(uTex, vUv + d, Lod).rgb + textureLod(uTex, vUv - d, Lod).rgb + textureLod(uTex, vUv + vec2(d.x, -d.y), Lod).rgb + textureLod(uTex, vUv - vec2(d.x, -d.y), Lod).rgb) * 0.0625;
	FragColor = vec4(c, 1.0);
})",
	// TILTSHIFT: uP.x = blur (mip level), uP.y = focus line (0 = bottom, 1 = top), uP.z = sharp band half height
	R"(void main()
{
	float d = abs(vUv.y - uP.y);
	float Lod = uP.x * smoothstep(uP.z, uP.z + 0.25, d);
	vec2 o = uTexel * exp2(Lod) * 0.7;
	vec3 c = (textureLod(uTex, vUv, Lod).rgb * 2.0 + textureLod(uTex, vUv + vec2(o.x, 0.0), Lod).rgb + textureLod(uTex, vUv - vec2(o.x, 0.0), Lod).rgb
		+ textureLod(uTex, vUv + vec2(0.0, o.y), Lod).rgb + textureLod(uTex, vUv - vec2(0.0, o.y), Lod).rgb) / 6.0;
	c = mix(vec3(Luma(c)), c, 1.15);
	FragColor = vec4(clamp(c, 0.0, 1.0), 1.0);
})",
	// DREAM: uP.x = amount
	R"(void main()
{
	vec3 c = textureLod(uTex, vUv, 0.0).rgb;
	vec3 Glow = textureLod(uTex, vUv, 3.0).rgb * 0.5 + textureLod(uTex, vUv, 4.5).rgb * 0.5;
	vec3 d = c + Glow * 0.7 * uP.x;
	d = mix(d, max(d, Glow), 0.35 * uP.x);
	d *= vec3(1.04, 0.99, 1.06);
	FragColor = vec4(clamp(mix(c, d, clamp(uP.x, 0.0, 1.0)), 0.0, 1.0), 1.0);
})",
	// POSTERIZE: uP.x = levels
	R"(void main()
{
	vec3 c = T(vUv);
	float n = max(uP.x - 1.0, 1.0);
	FragColor = vec4(floor(c * n + 0.5) / n, 1.0);
})",
	// OIL: uP.x = brush radius (Kuwahara)
	R"(void main()
{
	int R = int(clamp(uP.x, 1.0, 6.0));
	vec3 m[4];
	vec3 s[4];
	for(int k = 0; k < 4; k++)
	{
		m[k] = vec3(0.0);
		s[k] = vec3(0.0);
	}
	for(int j = -6; j <= 6; j++)
	{
		if(j < -R || j > R)
			continue;
		for(int i = -6; i <= 6; i++)
		{
			if(i < -R || i > R)
				continue;
			vec3 c = T(vUv + vec2(float(i), float(j)) * uTexel * 1.5);
			vec3 c2 = c * c;
			if(i <= 0 && j <= 0) { m[0] += c; s[0] += c2; }
			if(i >= 0 && j <= 0) { m[1] += c; s[1] += c2; }
			if(i <= 0 && j >= 0) { m[2] += c; s[2] += c2; }
			if(i >= 0 && j >= 0) { m[3] += c; s[3] += c2; }
		}
	}
	float n = float((R + 1) * (R + 1));
	float Best = 1e9;
	vec3 Out = T(vUv);
	for(int k = 0; k < 4; k++)
	{
		vec3 mean = m[k] / n;
		vec3 var = abs(s[k] / n - mean * mean);
		float v = var.r + var.g + var.b;
		if(v < Best)
		{
			Best = v;
			Out = mean;
		}
	}
	FragColor = vec4(Out, 1.0);
})",
	// SKETCH: uP.x = lines, uP.y = hatching
	R"(void main()
{
	float l = Luma(T(vUv));
	float e = Sobel(vUv, 1.0) * uP.x * 1.5;
	vec2 px = vUv / uTexel;
	float h1 = step(mod(px.x + px.y, 7.0), 1.0) * step(l, 0.62);
	float h2 = step(mod(px.x - px.y, 7.0), 1.0) * step(l, 0.38);
	float h3 = step(mod(px.x + px.y + 3.5, 7.0), 1.0) * step(l, 0.18);
	float Ink = clamp(e + (h1 + h2 + h3) * uP.y * 0.7, 0.0, 1.0);
	vec3 Paper = vec3(0.96, 0.94, 0.89) * (0.97 + 0.03 * Hash(floor(px / 3.0)));
	FragColor = vec4(mix(Paper, vec3(0.16, 0.14, 0.13), Ink), 1.0);
})",
	// WATERCOLOR: uP.x = amount
	R"(void main()
{
	vec3 c = T(vUv);
	vec3 w = textureLod(uTex, vUv, 1.6).rgb;
	w = floor(w * 6.0 + 0.5) / 6.0 * 0.5 + w * 0.5;
	float e = clamp(Sobel(vUv, 2.0), 0.0, 1.0);
	w *= 1.0 - 0.35 * e;
	vec2 px = vUv / uTexel;
	float Paper = 0.93 + 0.07 * Hash(floor(px / 2.0));
	w = mix(vec3(1.0), w, 0.85) * Paper;
	FragColor = vec4(mix(c, w, uP.x), 1.0);
})",
	// CARTOON: uP.x = levels, uP.y = outline strength, uP.z = outline width, uColor = outline color
	R"(void main()
{
	vec3 c = T(vUv);
	float l = Luma(c);
	float n = max(uP.x - 1.0, 1.0);
	float q = floor(l * n + 0.5) / n;
	c = c * (q / max(l, 0.001));
	c = mix(vec3(Luma(c)), c, 1.25);
	float e = smoothstep(0.25, 0.6, Sobel(vUv, uP.z) * uP.y);
	FragColor = vec4(mix(clamp(c, 0.0, 1.0), uColor, e), 1.0);
})",
	// EDGES: uP.x = strength, uP.y = width, uP.z = only the outlines (0..1), uColor = line color
	R"(void main()
{
	vec3 c = T(vUv);
	float e = clamp(Sobel(vUv, uP.y) * uP.x, 0.0, 1.0);
	vec3 Base = mix(c, vec3(1.0) - uColor, uP.z);
	FragColor = vec4(mix(Base, uColor, e), 1.0);
})",
	// PIXEL: uP.x = pixel size in screen pixels
	R"(void main()
{
	vec2 Cell = uTexel * max(uP.x, 1.0);
	FragColor = vec4(T((floor(vUv / Cell) + 0.5) * Cell), 1.0);
})",
	// HEX: uP.x = cell size in screen pixels, uP.y = dark edges
	R"(void main()
{
	float Size = max(uP.x, 3.0);
	vec2 p = vUv / uTexel / Size;
	vec2 r = vec2(1.0, 1.7320508);
	vec2 h = r * 0.5;
	vec2 a = mod(p, r) - h;
	vec2 b = mod(p - h, r) - h;
	vec2 g = dot(a, a) < dot(b, b) ? a : b;
	vec2 Center = p - g;
	vec3 c = T(Center * Size * uTexel);
	vec2 q = abs(g);
	float d = max(dot(q, normalize(vec2(1.0, 1.7320508))), q.x);
	c *= 1.0 - uP.y * smoothstep(0.42, 0.5, d);
	FragColor = vec4(c, 1.0);
})",
	// STAINED: uP.x = glass size in screen pixels, uP.y = lead lines
	R"(vec2 Point(vec2 Cell) { return vec2(Hash(Cell), Hash(Cell + 17.31)) * 0.8 + 0.1; }
void main()
{
	float Size = max(uP.x, 4.0);
	vec2 p = vUv / uTexel / Size;
	vec2 Cell = floor(p);
	float d1 = 9.0, d2 = 9.0;
	vec2 Best = Cell;
	for(int j = -1; j <= 1; j++)
		for(int i = -1; i <= 1; i++)
		{
			vec2 n = Cell + vec2(float(i), float(j));
			float d = length(p - (n + Point(n)));
			if(d < d1)
			{
				d2 = d1;
				d1 = d;
				Best = n;
			}
			else if(d < d2)
				d2 = d;
		}
	vec3 c = T((Best + Point(Best)) * Size * uTexel);
	c = mix(vec3(Luma(c)), c, 1.35) * 1.1;
	float Lead = 1.0 - smoothstep(0.03, 0.08, d2 - d1);
	c = mix(c, vec3(0.05), Lead * uP.y);
	FragColor = vec4(clamp(c, 0.0, 1.0), 1.0);
})",
	// HALFTONE: uP.x = dot cell in screen pixels, uP.y = amount
	R"(void main()
{
	vec2 Px = vUv / uTexel;
	float a = 0.785398;
	mat2 Rot = mat2(cos(a), -sin(a), sin(a), cos(a));
	vec2 p = Rot * Px / max(uP.x, 2.0);
	vec2 Cell = floor(p) + 0.5;
	vec2 CenterUv = (transpose(Rot) * (Cell * max(uP.x, 2.0))) * uTexel;
	vec3 c = T(CenterUv);
	float l = Luma(c);
	float r = sqrt(1.0 - l) * 0.62;
	float d = length(p - Cell);
	float Ink = 1.0 - smoothstep(r - 0.08, r + 0.08, d);
	vec3 Paper = vec3(0.97, 0.95, 0.88);
	vec3 Dots = mix(Paper, c * 0.55, Ink);
	FragColor = vec4(mix(T(vUv), Dots, uP.y), 1.0);
})",
	// ASCII: uP.x = character cell in screen pixels, uP.y = color (0 = green text, 1 = the picture's colors)
	R"(float Glyph(int n, vec2 p)
{
	p = floor(p * vec2(-4.0, 4.0) + 2.5);
	if(clamp(p.x, 0.0, 4.0) == p.x && clamp(p.y, 0.0, 4.0) == p.y)
	{
		int a = int(round(p.x) + 5.0 * round(p.y));
		if(((n >> a) & 1) == 1)
			return 1.0;
	}
	return 0.0;
}
void main()
{
	float Size = max(uP.x, 4.0);
	vec2 Px = vUv / uTexel;
	vec2 CellUv = (floor(Px / Size) * Size + Size * 0.5) * uTexel;
	vec3 c = T(CellUv);
	float l = Luma(c);
	int n = 0;
	if(l > 0.1) n = 4096;
	if(l > 0.2) n = 65600;
	if(l > 0.3) n = 332772;
	if(l > 0.4) n = 15255086;
	if(l > 0.5) n = 23385164;
	if(l > 0.6) n = 15252014;
	if(l > 0.7) n = 13199452;
	if(l > 0.8) n = 11512810;
	vec2 p = mod(Px / (Size * 0.5), 2.0) - vec2(1.0);
	float g = Glyph(n, p);
	vec3 Ink = mix(vec3(0.2, 1.0, 0.35), min(c * 1.3, 1.0), uP.y);
	FragColor = vec4(Ink * g, 1.0);
})",
	// GAMEBOY: uP.x = amount, uP.y = pixel size
	R"(void main()
{
	vec2 Cell = uTexel * max(uP.y, 1.0);
	vec2 Uv = (floor(vUv / Cell) + 0.5) * Cell;
	vec3 c = T(Uv);
	float l = Luma(c);
	vec2 Pix = floor(vUv / Cell);
	float b = mod(Pix.x + Pix.y * 2.0, 4.0) / 4.0 - 0.375;
	float q = clamp(floor((l + b * 0.25) * 4.0), 0.0, 3.0);
	vec3 Pal = q < 1.0 ? vec3(0.06, 0.22, 0.06) : q < 2.0 ? vec3(0.19, 0.38, 0.19) : q < 3.0 ? vec3(0.55, 0.67, 0.06) : vec3(0.61, 0.74, 0.06);
	FragColor = vec4(mix(T(vUv), Pal, uP.x), 1.0);
})",
	// DITHER: uP.x = levels per color, uP.y = pixel size
	R"(void main()
{
	vec2 Cell = uTexel * max(uP.y, 1.0);
	vec2 Pix = floor(vUv / Cell);
	vec3 c = T((Pix + 0.5) * Cell);
	int x = int(mod(Pix.x, 4.0)), y = int(mod(Pix.y, 4.0));
	int idx = x + y * 4;
	float Bayer[16] = float[16](0.0, 8.0, 2.0, 10.0, 12.0, 4.0, 14.0, 6.0, 3.0, 11.0, 1.0, 9.0, 15.0, 7.0, 13.0, 5.0);
	float t = (Bayer[idx] + 0.5) / 16.0 - 0.5;
	float n = max(uP.x - 1.0, 1.0);
	c = floor(c * n + 0.5 + t) / n;
	FragColor = vec4(clamp(c, 0.0, 1.0), 1.0);
})",
	// GLITCH: uP.x = strength, uP.y = how often
	R"(void main()
{
	vec2 Uv = vUv;
	float Slot = floor(uTime * (2.0 + 10.0 * uP.y));
	float Row = floor(Uv.y * 24.0);
	float r = Hash(vec2(Row, Slot));
	float On = step(1.0 - 0.35 * uP.y, Hash(vec2(Slot, 7.0)));
	Uv.x += (r - 0.5) * 0.12 * uP.x * On * step(0.6, r);
	float Split = 0.006 * uP.x * (0.3 + On);
	vec3 c = vec3(T(Uv + vec2(Split, 0.0)).r, T(Uv).g, T(Uv - vec2(Split, 0.0)).b);
	FragColor = vec4(c, 1.0);
})",
	// VHS: uP.x = strength, uP.y = noise
	R"(void main()
{
	vec2 Uv = vUv;
	float Wob = sin(Uv.y * 12.0 + uTime * 2.0) * 0.0015 + sin(Uv.y * 120.0 + uTime * 9.0) * 0.0006;
	float Tear = smoothstep(0.03, 0.0, abs(fract(Uv.y + uTime * 0.07) - 0.5)) * 0.01;
	Uv.x += (Wob + Tear) * uP.x;
	float Split = 0.004 * uP.x;
	vec3 c = vec3(T(Uv + vec2(Split, 0.0)).r, T(Uv).g, T(Uv - vec2(Split, 0.0)).b);
	c = mix(vec3(Luma(c)), c, 1.0 - 0.25 * uP.x);
	c += (Hash(vec2(Uv.y * 400.0, uTime * 60.0)) - 0.5) * 0.12 * uP.y;
	c *= 0.95 + 0.05 * sin(Uv.y * 700.0);
	FragColor = vec4(clamp(c, 0.0, 1.0), 1.0);
})",
	// CRT: uP.x = scanlines, uP.y = curve, uP.z = color mask
	R"(void main()
{
	vec2 Uv = vUv * 2.0 - 1.0;
	Uv *= 1.0 + 0.08 * uP.y * vec2(Uv.y * Uv.y, Uv.x * Uv.x);
	Uv = Uv * 0.5 + 0.5;
	if(Uv.x < 0.0 || Uv.x > 1.0 || Uv.y < 0.0 || Uv.y > 1.0)
	{
		FragColor = vec4(0.0, 0.0, 0.0, 1.0);
		return;
	}
	vec3 c = T(Uv);
	float Px = Uv.y / uTexel.y;
	c *= 1.0 - uP.x * 0.35 * (0.5 + 0.5 * cos(Px * 3.14159 * 0.5));
	float m = mod(floor(Uv.x / uTexel.x), 3.0);
	vec3 Mask = m < 1.0 ? vec3(1.0, 0.75, 0.75) : m < 2.0 ? vec3(0.75, 1.0, 0.75) : vec3(0.75, 0.75, 1.0);
	c *= mix(vec3(1.0), Mask, uP.z);
	c *= 1.0 + 0.15 * uP.z;
	vec2 v = Uv - 0.5;
	c *= 1.0 - smoothstep(0.35, 0.75, length(v)) * 0.6 * uP.y;
	FragColor = vec4(clamp(c, 0.0, 1.0), 1.0);
})",
};
static_assert(std::size(s_apBcFilterBody) == NUM_BC_FILTERS, "one shader per filter");

static bool BcFilterNeedsMips(int Id) { return Id == BC_FILTER_BLUR || Id == BC_FILTER_TILTSHIFT || Id == BC_FILTER_DREAM || Id == BC_FILTER_WATERCOLOR; }

bool CCommandProcessorFragment_OpenGL3_3::BcFilterEnsure(int Id)
{
	if(Id < 0 || Id >= NUM_BC_FILTERS || Id >= (int)std::size(m_aBcFilterProgram) || m_aBcFilterFailed[Id])
		return false;
	if(m_aBcFilterProgram[Id] != 0)
		return true;
	char aSource[8192];
	str_copy(aSource, s_pBcFilterHeader);
	str_append(aSource, s_apBcFilterBody[Id]);
	const TWGLuint Vertex = BcPostFxCompile(GL_VERTEX_SHADER, s_pBcPostFxVertex);
	const TWGLuint Fragment = BcPostFxCompile(GL_FRAGMENT_SHADER, aSource);
	if(Vertex == 0 || Fragment == 0)
	{
		if(Vertex)
			glDeleteShader(Vertex);
		if(Fragment)
			glDeleteShader(Fragment);
		m_aBcFilterFailed[Id] = true;
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
		log_error("chainsaw", "filter %d shader link failed", Id);
		glDeleteProgram(Program);
		m_aBcFilterFailed[Id] = true;
		return false;
	}
	m_aBcFilterProgram[Id] = Program;
	const char *apNames[] = {"uTex", "uTexel", "uP", "uColor", "uTime"};
	for(size_t i = 0; i < std::size(apNames); i++)
		m_aaBcFilterLoc[Id][i] = glGetUniformLocation(Program, apNames[i]);
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
	const auto CopyScreen = [&](bool Mips) {
		glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
		glBindFramebuffer(GL_DRAW_FRAMEBUFFER, m_BcPostFxFbo);
		glBlitFramebuffer(m_ViewportX, m_ViewportY, m_ViewportX + W, m_ViewportY + H, 0, 0, W, H, GL_COLOR_BUFFER_BIT, GL_NEAREST);
		glBindFramebuffer(GL_FRAMEBUFFER, 0);
		glActiveTexture(GL_TEXTURE0);
		glBindSampler(0, 0);
		glBindTexture(GL_TEXTURE_2D, m_BcPostFxTex);
		if(Mips)
			glGenerateMipmap(GL_TEXTURE_2D);
	};
	glViewport(m_ViewportX, m_ViewportY, W, H);
	glDisable(GL_BLEND);
	if(m_BcPostFxVao == 0)
		glGenVertexArrays(1, &m_BcPostFxVao);

	// the filter chain, after the one-shader pass
	const auto Filters = [&]() {
		for(int i = 0; i < P.m_NumFilters && i < NUM_BC_FILTERS; i++)
		{
			const SBcFilter &F = P.m_aFilters[i];
			if(!BcFilterEnsure(F.m_Id))
				continue;
			CopyScreen(BcFilterNeedsMips(F.m_Id));
			glUseProgram(m_aBcFilterProgram[F.m_Id]);
			const int *pLoc = m_aaBcFilterLoc[F.m_Id];
			glUniform1i(pLoc[0], 0);
			glUniform2f(pLoc[1], 1.0f / W, 1.0f / H);
			glUniform4f(pLoc[2], F.m_aP[0], F.m_aP[1], F.m_aP[2], F.m_aP[3]);
			glUniform3f(pLoc[3], F.m_aColor[0], F.m_aColor[1], F.m_aColor[2]);
			glUniform1f(pLoc[4], P.m_Time);
			glBindVertexArray(m_BcPostFxVao);
			glDrawArrays(GL_TRIANGLES, 0, 3);
		}
		glBindVertexArray(0);
		glBindTexture(GL_TEXTURE_2D, 0);
		glUseProgram(0);
		m_LastProgramId = 0;
		m_LastBlendMode = EBlendMode::NONE;
	};
	if(!P.m_Base)
	{
		Filters();
		return;
	}

	CopyScreen(P.m_Bloom > 0.0f);
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

	// the filters, then leave the state the way the renderer expects it
	Filters();
}
