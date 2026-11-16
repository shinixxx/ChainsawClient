/* ChainsawClient: realistic blocks for the OpenGL 3.3 renderer (engine/cc_blocks_params.h).
   One pass over the screen: every pixel finds its block of the map in the material texture and draws earth with grass,
   rock or water by the world position (so nothing repeats by the grid). Earth, rock and moss are photo textures
   (color + height, normal + shadow) lit by a sun from the top left; water bends the picture behind it (refraction).
   If anything fails the pass turns itself off and the game draws as before. */
#include <engine/client/cc_blocks_cmd.h>

static const char *const s_pCcBlocksFragment = R"(#version 330
// ChainsawClient: realistic blocks. One pass over the screen: every pixel finds its block of the map in uMat
// (see cc_blocks_core.h) and draws earth with grass, rock or water by the world position, so nothing repeats by the grid.
// Earth, rock and moss are photo textures (color + height, normal + shadow) lit by a sun from the top left;
// the water bends the picture behind it (uScene = the screen before the blocks).
uniform sampler2D uMat;
uniform sampler2D uScene;
uniform sampler2D uSoilA; // rgb color, a height
uniform sampler2D uSoilN; // rg normal, b shadow in the cracks
uniform sampler2D uRockA;
uniform sampler2D uRockN;
uniform sampler2D uMossA;
uniform sampler2D uMossN;
uniform vec2 uOrigin; // world position of the top left corner of the screen, in blocks
uniform vec2 uSize; // size of the screen in blocks
uniform float uTime;
uniform int uQuality; // 0 low .. 3 ultra
uniform int uDecor; // grass and flowers above the blocks
in vec2 vUv;
out vec4 FragColor;

// x right, y up, z to the viewer
const vec3 SUN_DIR = vec3(-0.5013, 0.6216, 0.6016);
const vec3 SUN = vec3(1.30, 1.20, 1.02);
const vec3 SKY = vec3(0.40, 0.50, 0.66);

float Hash(vec2 p)
{
	vec3 p3 = fract(vec3(p.xyx) * 0.1031);
	p3 += dot(p3, p3.yzx + 33.33);
	return fract((p3.x + p3.y) * p3.z);
}

float Noise(vec2 p)
{
	vec2 i = floor(p), f = fract(p);
	f = f * f * (3.0 - 2.0 * f);
	return mix(mix(Hash(i), Hash(i + vec2(1.0, 0.0)), f.x), mix(Hash(i + vec2(0.0, 1.0)), Hash(i + vec2(1.0, 1.0)), f.x), f.y);
}

// 0..1, the middle is 0.5
float Fbm(vec2 p, int Oct)
{
	float s = 0.0, a = 0.5;
	for(int i = 0; i < Oct; i++)
	{
		s += a * Noise(p);
		p = mat2(1.6, 1.2, -1.2, 1.6) * p + 17.1;
		a *= 0.5;
	}
	return s / (1.0 - 2.0 * a);
}

ivec4 Tile(ivec2 p)
{
	p = clamp(p, ivec2(0), textureSize(uMat, 0) - 1);
	return ivec4(texelFetch(uMat, p, 0) * 255.0 + 0.5);
}

bool GrassTop(ivec4 t)
{
	return t.r == 1 && (t.a & 1) != 0;
}

// distance (in blocks) to the border of the group of same blocks, Dir = the way out (y down); f = position inside the block
float Edge(int m, vec2 f, out vec2 Dir)
{
	float d = 1.0;
	Dir = vec2(0.0);
	if((m & 1) == 0 && f.y < d)
	{
		d = f.y;
		Dir = vec2(0.0, -1.0);
	}
	if((m & 4) == 0 && 1.0 - f.y < d)
	{
		d = 1.0 - f.y;
		Dir = vec2(0.0, 1.0);
	}
	if((m & 8) == 0 && f.x < d)
	{
		d = f.x;
		Dir = vec2(-1.0, 0.0);
	}
	if((m & 2) == 0 && 1.0 - f.x < d)
	{
		d = 1.0 - f.x;
		Dir = vec2(1.0, 0.0);
	}
	// inner corners: both sides go on, the diagonal block is different
	vec2 v = vec2(1.0 - f.x, -f.y);
	if((m & 19) == 3 && length(v) < d)
	{
		d = length(v);
		Dir = v / max(d, 0.0001);
	}
	v = vec2(1.0 - f.x, 1.0 - f.y);
	if((m & 38) == 6 && length(v) < d)
	{
		d = length(v);
		Dir = v / max(d, 0.0001);
	}
	v = vec2(-f.x, 1.0 - f.y);
	if((m & 76) == 12 && length(v) < d)
	{
		d = length(v);
		Dir = v / max(d, 0.0001);
	}
	v = vec2(-f.x, -f.y);
	if((m & 137) == 9 && length(v) < d)
	{
		d = length(v);
		Dir = v / max(d, 0.0001);
	}
	return d;
}

struct SSurf
{
	vec3 m_Alb; // linear color
	vec3 m_Nor;
	float m_Ao;
	float m_H;
};

SSurf Sample(sampler2D A, sampler2D N, vec2 Uv)
{
	vec4 a = texture(A, Uv);
	vec3 n = texture(N, Uv).rgb;
	SSurf s;
	s.m_Alb = a.rgb * a.rgb;
	s.m_H = a.a;
	vec2 xy = n.xy * 2.0 - 1.0;
	s.m_Nor = vec3(xy, sqrt(max(1.0 - dot(xy, xy), 0.05)));
	s.m_Ao = n.b;
	return s;
}

// Scale = blocks the picture covers. On ultra it is taken twice with different shifts and mixed by slow noise,
// which hides the repeat of the picture.
SSurf Surface(sampler2D A, sampler2D N, vec2 w, float Scale)
{
	SSurf s = Sample(A, N, w / Scale);
	if(uQuality >= 3)
	{
		SSurf s2 = Sample(A, N, w / (Scale * 1.37) + vec2(0.37, 0.61));
		float m = smoothstep(0.40, 0.60, Fbm(w * 0.21 + 5.0, 2) + (s2.m_H - s.m_H) * 0.25);
		s.m_Alb = mix(s.m_Alb, s2.m_Alb, m);
		s.m_Nor = normalize(mix(s.m_Nor, s2.m_Nor, m));
		s.m_Ao = mix(s.m_Ao, s2.m_Ao, m);
		s.m_H = mix(s.m_H, s2.m_H, m);
	}
	return s;
}

vec3 Light(vec3 Alb, vec3 N, float Ao, float Spec, float Gloss)
{
	float d = max(dot(N, SUN_DIR), 0.0);
	vec3 c = Alb * (SKY * (0.60 + 0.40 * N.y) * Ao + SUN * d * mix(0.55, 1.0, Ao));
	vec3 Half = normalize(SUN_DIR + vec3(0.0, 0.0, 1.0));
	c += SUN * Spec * pow(max(dot(N, Half), 0.0), Gloss) * Ao;
	return c;
}

void Over(inout vec4 Acc, vec3 c, float a)
{
	Acc = Acc * (1.0 - a) + vec4(c, 1.0) * a;
}

/* ---------------- earth with a cap of moss and grass ---------------- */

vec3 Ground(vec2 w, vec2 f, ivec4 t, float Px)
{
	SSurf s = Surface(uSoilA, uSoilN, w, 3.0);
	vec3 Alb = s.m_Alb * vec3(1.55, 1.40, 1.25);
	float Depth = float(t.b);
	ivec4 Next = Tile(ivec2(floor(w)) + ivec2(f.x < 0.5 ? -1 : 1, 0));
	if(Next.r == 1)
		Depth = mix(Depth, float(Next.b), abs(f.x - 0.5));
	Alb *= mix(1.0, 0.60, smoothstep(0.0, 8.0, Depth + f.y));
	float Gloss = 0.02;
	if((t.a & 1) != 0)
	{
		// the green cap grows into the earth where the earth is low (height of the picture), so the border is ragged
		float Th = 0.20 + 0.12 * Noise(vec2(w.x * 1.7, 3.0)) + 0.05 * Noise(vec2(w.x * 6.0, 9.0));
		float Cap = smoothstep(-0.04, 0.05, (Th - f.y) + (0.5 - s.m_H) * 0.20);
		SSurf g = Surface(uMossA, uMossN, w, 2.2);
		vec3 Green = g.m_Alb * vec3(0.50, 0.95, 0.38) * mix(0.75, 1.25, smoothstep(0.30, 0.0, f.y));
		Alb *= mix(1.0, 0.45, smoothstep(Th + 0.22, Th, f.y) * (1.0 - Cap)); // damp dark earth under the grass
		Alb = mix(Alb, Green, Cap);
		s.m_Nor = normalize(mix(s.m_Nor, g.m_Nor, Cap));
		s.m_Ao = mix(s.m_Ao, g.m_Ao, Cap);
	}
	vec2 Dir;
	float e = Edge(t.g, f, Dir);
	vec3 N = s.m_Nor;
	if(uQuality >= 1)
	{
		N.xy *= 1.6;
		// the block is a thick slab: its edges turn away
		N = normalize(N + vec3(Dir.x, -Dir.y, 0.0) * (1.0 - smoothstep(0.0, 0.17, e)) * 1.5);
	}
	else
		N = vec3(0.0, 0.0, 1.0);
	float Ao = s.m_Ao * mix(0.62, 1.0, smoothstep(0.0, 0.10, e));
	return Light(Alb, N, Ao, Gloss, 12.0);
}

/* ---------------- rock ---------------- */

vec3 Stone(vec2 w, vec2 f, ivec4 t, float Px)
{
	SSurf s = Surface(uRockA, uRockN, w, 2.8);
	vec3 Alb = s.m_Alb * vec3(1.55, 1.52, 1.50);
	if(uQuality >= 2 && (t.a & 1) != 0)
	{
		// moss lies in the hollows of the open top
		float Th = (0.10 + 0.22 * Noise(vec2(w.x * 1.3, 12.0))) * smoothstep(0.25, 0.55, Noise(vec2(w.x * 0.6, 4.0)));
		float Cap = smoothstep(-0.03, 0.05, (Th - f.y) + (0.45 - s.m_H) * 0.30);
		SSurf g = Surface(uMossA, uMossN, w, 2.2);
		Alb = mix(Alb, g.m_Alb * vec3(0.55, 0.95, 0.35), Cap);
		s.m_Nor = normalize(mix(s.m_Nor, g.m_Nor, Cap));
	}
	vec2 Dir;
	float e = Edge(t.g, f, Dir);
	vec3 N = s.m_Nor;
	if(uQuality >= 1)
	{
		N.xy *= 1.5;
		N = normalize(N + vec3(Dir.x, -Dir.y, 0.0) * (1.0 - smoothstep(0.0, 0.15, e)) * 1.7);
	}
	else
		N = vec3(0.0, 0.0, 1.0);
	float Ao = s.m_Ao * mix(0.60, 1.0, smoothstep(0.0, 0.09, e));
	return Light(Alb, N, Ao, 0.16, 28.0);
}

/* ---------------- water ---------------- */

// moving net of light on the bottom (tileable water caustic by Dave Hoskins)
float Caustic(vec2 Uv, float t)
{
	vec2 p = mod(Uv * 6.28318, 6.28318) - 250.0;
	vec2 i = p;
	float c = 1.0;
	for(int n = 0; n < 4; n++)
	{
		float tt = t * (1.0 - (3.5 / float(n + 1)));
		i = p + vec2(cos(tt - i.x) + sin(tt + i.y), sin(tt - i.y) + cos(tt + i.x));
		c += 1.0 / length(vec2(p.x / (sin(i.x + tt) / 0.005), p.y / (cos(i.y + tt) / 0.005)));
	}
	c /= 4.0;
	c = 1.17 - pow(c, 1.4);
	return pow(abs(c), 8.0);
}

vec4 Water(vec2 w, vec2 f, ivec4 t, float Px)
{
	bool Open = (t.a & 1) != 0;
	float Wave = 0.12 + 0.030 * sin(w.x * 4.5 + uTime * 1.9) + 0.018 * sin(w.x * 10.0 - uTime * 2.9) + 0.007 * sin(w.x * 23.0 + uTime * 4.1);
	float s = Open ? f.y - Wave : 1.0; // below the line of the surface (only in the top block)
	float D = max(float(t.b) + f.y - 0.12, 0.0); // how deep, in blocks

	// what is behind the water, bent by slow ripples
	vec2 Rip = vec2(Noise(w * 2.3 + vec2(uTime * 0.6, uTime * 0.4)), Noise(w * 2.3 + 7.0 - vec2(uTime * 0.5, uTime * 0.7))) - 0.5;
	vec2 Uv = vUv;
	if(uQuality >= 2)
		Uv += Rip * 0.22 * min(D * 3.0, 1.0) / uSize * vec2(1.0, -1.0);
	vec3 Scene = textureLod(uScene, Uv, 0.0).rgb; // only level 0 of the screen copy is fresh
	Scene *= Scene;

	float Fog = 1.0 - exp(-D * 0.55);
	vec3 c = Scene * mix(vec3(0.72, 0.94, 1.0), vec3(0.22, 0.55, 0.78), Fog);
	c = mix(c, mix(vec3(0.10, 0.36, 0.46), vec3(0.01, 0.07, 0.17), clamp(D / 5.0, 0.0, 1.0)), Fog * 0.72);
	if(uQuality >= 1)
	{
		float Ca = Caustic(w * 0.37 + Rip * 0.05, uTime * 0.55);
		c += vec3(0.50, 0.85, 0.80) * Ca * exp(-D * 0.40) * 0.50;
	}
	if(uQuality >= 2)
	{
		// shafts of light going down from the surface
		float Shaft = smoothstep(0.42, 0.78, Noise(vec2((w.x + w.y * 0.38) * 1.5 + uTime * 0.07, uTime * 0.13)));
		c += vec3(0.30, 0.55, 0.55) * Shaft * exp(-D * 0.55) * 0.16;
	}
	if(uQuality >= 3)
	{
		// dust in the water
		vec2 q = w * 9.0 + vec2(uTime * 0.25, -uTime * 0.18);
		float Speck = smoothstep(0.985, 1.0, Hash(floor(q))) * (1.0 - smoothstep(0.12, 0.30, length(fract(q) - 0.5)));
		c += vec3(0.6, 0.8, 0.8) * Speck * 0.5;
	}
	// the sides that touch air: a thin light edge
	float Side = 1.0;
	if((t.a & 2) != 0)
		Side = min(Side, 1.0 - f.x);
	if((t.a & 8) != 0)
		Side = min(Side, f.x);
	if((t.a & 4) != 0)
		Side = min(Side, 1.0 - f.y);
	c += vec3(0.35, 0.50, 0.55) * exp(-Side * 34.0);
	float a = 1.0;
	if(Open)
	{
		// the surface: a bright line and foam that fades down
		float Line = exp(-max(s, 0.0) * 42.0);
		float Foam = smoothstep(0.30, 0.80, Noise(vec2(w.x * 6.0 - uTime * 0.8, uTime * 0.5)) * 0.7 + 0.3 * Noise(vec2(w.x * 17.0 + uTime * 0.6, 5.0))) * exp(-max(s, 0.0) * 13.0);
		c = mix(c, vec3(0.90, 0.97, 1.0), clamp(Line * 0.90 + Foam * 0.65, 0.0, 1.0));
		a = smoothstep(-Px, Px, s);
	}
	else if((t.g & 1) == 0)
		c *= mix(0.55, 1.0, smoothstep(0.0, 0.16, f.y)); // under a ceiling
	return vec4(c, a);
}

/* ---------------- grass ----------------
   h = height above the top of the grass blocks (below 0 inside the block). Cols: are the columns left / here / right
   grass tops; a blade is drawn only if its root stands in such a column, so blades hang over the ends of the grass. */

void Blades(inout vec4 Acc, vec2 w, float h, float Px, bvec3 Cols, float ColX, float N, float Seed, float MaxH, float Front)
{
	float hh = h + 0.07;
	if(hh < 0.0 || hh > MaxH)
		return;
	// the wind bends the whole field in slow waves
	float Wind = sin(uTime * 1.5 + w.x * 1.9) * 0.6 + sin(uTime * 0.8 + w.x * 0.7 + 1.3) * 0.4;
	float Bend = hh / MaxH;
	float x = w.x - Wind * Bend * Bend * MaxH * 0.20;
	float Base = floor(x * N);
	for(int i = -2; i <= 2; i++)
	{
		float Id = Base + float(i);
		float r = Hash(vec2(Id, Seed));
		float r2 = Hash(vec2(Id, Seed + 5.3));
		float r3 = Hash(vec2(Id, Seed + 9.1));
		float RootX = (Id + r2) / N;
		int Col = int(floor(RootX) - ColX) + 1;
		if(Col < 0 || Col > 2 || !Cols[Col])
			continue;
		float Height = MaxH * (0.38 + 0.62 * r * r);
		float k = hh / Height;
		if(k >= 1.0)
			continue;
		float Cx = (x - RootX) - (r3 - 0.5) * 2.4 / N * k * k;
		float HalfW = (0.011 + 0.013 * r2) * (1.0 - 0.9 * k * k) + Px * 0.30;
		float a = 1.0 - smoothstep(HalfW - Px, HalfW + Px, abs(Cx));
		if(a <= 0.0)
			continue;
		vec3 Lo = mix(vec3(0.010, 0.040, 0.012), vec3(0.020, 0.085, 0.018), Front);
		vec3 Hi = mix(vec3(0.050, 0.170, 0.050), vec3(0.230, 0.470, 0.070), Front);
		Hi = mix(Hi, vec3(0.42, 0.44, 0.10), step(0.82, r3) * 0.55); // some dry blades
		vec3 c = mix(Lo, Hi, pow(k, 0.75)) * (0.75 + 0.5 * r);
		c *= 1.0 - 0.30 * clamp(Cx / HalfW, -1.0, 1.0); // the side to the sun is lighter
		Over(Acc, c * SUN, a);
	}
}

void Flowers(inout vec4 Acc, vec2 w, float h, float Px, bvec3 Cols, float ColX)
{
	if(h < 0.0 || h > 0.62)
		return;
	float N = 2.6;
	float x = w.x * N + 0.37;
	float Id = floor(x);
	float r = Hash(vec2(Id, 91.0));
	if(r < 0.45)
		return;
	float r2 = Hash(vec2(Id, 13.0));
	float RootX = (Id - 0.37 + 0.28 + 0.44 * r2) / N;
	int Col = int(floor(RootX) - ColX) + 1;
	if(Col < 0 || Col > 2 || !Cols[Col])
		return;
	bool Yellow = r > 0.72;
	float Top = 0.22 + 0.30 * r2;
	float Rad = Yellow ? 0.050 : 0.044;
	float Lean = (sin(uTime * 1.5 + RootX * 1.9) * 0.6 + sin(uTime * 0.8 + RootX * 0.7 + 1.3) * 0.4) * 0.045;
	float k = clamp(h / Top, 0.0, 1.0);
	float Stem = (1.0 - smoothstep(0.006, 0.006 + Px, abs(w.x - (RootX + Lean * k * k)))) * step(h, Top);
	Over(Acc, vec3(0.05, 0.16, 0.03), Stem);
	vec2 d = vec2(w.x - RootX - Lean, h - Top);
	float Len = length(d);
	float Petals = Yellow ? 5.0 : 9.0;
	float Shape = Rad * (0.62 + 0.38 * abs(cos(atan(d.y, d.x) * Petals * 0.5 + r2 * 6.28)));
	float a = 1.0 - smoothstep(-Px, Px, Len - Shape);
	vec3 c = Yellow ? vec3(1.0, 0.62, 0.03) : vec3(0.92, 0.93, 0.88);
	c *= 0.72 + 0.40 * smoothstep(-Rad, Rad, d.y - d.x) - 0.25 * smoothstep(Rad * 0.3, Rad, Len) * 0.5;
	Over(Acc, c * SUN, a);
	float Core = 1.0 - smoothstep(-Px, Px, Len - Rad * 0.30);
	Over(Acc, (Yellow ? vec3(0.55, 0.22, 0.01) : vec3(0.95, 0.60, 0.04)) * SUN, Core);
}

void Grass(inout vec4 Acc, vec2 w, float h, float Px, bvec3 Cols, float ColX)
{
	if(uDecor == 0 || uQuality < 1)
		return;
	if(uQuality >= 3)
		Blades(Acc, w, h, Px, Cols, ColX, 20.0, 11.0, 0.66, 0.0);
	if(uQuality >= 2)
		Blades(Acc, w, h, Px, Cols, ColX, 30.0, 23.0, 0.50, 0.40);
	if(uQuality >= 3)
		Flowers(Acc, w, h, Px, Cols, ColX);
	Blades(Acc, w, h, Px, Cols, ColX, 40.0, 37.0, 0.38, 0.75);
	if(uQuality >= 2)
		Blades(Acc, w, h, Px, Cols, ColX, 52.0, 51.0, 0.26, 1.0);
}

void main()
{
	vec2 w = uOrigin + vec2(vUv.x, 1.0 - vUv.y) * uSize;
	float Px = fwidth(w.x) * 0.75;
	ivec2 Ip = ivec2(floor(w));
	vec2 f = w - floor(w);
	ivec4 t = Tile(Ip);
	vec4 o = vec4(0.0);
	if(t.r == 1)
		o = vec4(Ground(w, f, t, Px), 1.0);
	else if(t.r == 2)
		o = vec4(Stone(w, f, t, Px), 1.0);
	else if(t.r == 3)
		o = Water(w, f, t, Px);

	// the grass of the blocks around: of the row below (it grows up into this block) and of this row
	vec4 Acc = vec4(0.0);
	float ColX = floor(w.x);
	if(f.y > 0.30)
	{
		bvec3 Cols = bvec3(GrassTop(Tile(Ip + ivec2(-1, 1))), GrassTop(Tile(Ip + ivec2(0, 1))), GrassTop(Tile(Ip + ivec2(1, 1))));
		if(any(Cols))
			Grass(Acc, w, 1.0 - f.y, Px, Cols, ColX);
	}
	if(f.y < 0.08)
	{
		bvec3 Cols = bvec3(GrassTop(Tile(Ip + ivec2(-1, 0))), GrassTop(t), GrassTop(Tile(Ip + ivec2(1, 0))));
		if(any(Cols))
			Grass(Acc, w, -f.y, Px, Cols, ColX);
	}
	vec4 Pre = vec4(o.rgb * o.a, o.a) * (1.0 - Acc.a) + Acc;
	FragColor = Pre.a > 0.0 ? vec4(sqrt(clamp(Pre.rgb / Pre.a, 0.0, 1.0)), Pre.a) : vec4(0.0);
}
)";

bool CCommandProcessorFragment_OpenGL3_3::CcBlocksEnsure()
{
	if(m_CcBlocksFailed || !BcPostFxEnsure())
		return false;
	if(m_CcBlocksProgram == 0)
	{
		const TWGLuint Vertex = BcPostFxCompile(GL_VERTEX_SHADER, s_pBcPostFxVertex);
		const TWGLuint Fragment = BcPostFxCompile(GL_FRAGMENT_SHADER, s_pCcBlocksFragment);
		if(Vertex == 0 || Fragment == 0)
		{
			if(Vertex)
				glDeleteShader(Vertex);
			if(Fragment)
				glDeleteShader(Fragment);
			m_CcBlocksFailed = true;
			return false;
		}
		m_CcBlocksProgram = glCreateProgram();
		glAttachShader(m_CcBlocksProgram, Vertex);
		glAttachShader(m_CcBlocksProgram, Fragment);
		glLinkProgram(m_CcBlocksProgram);
		glDeleteShader(Vertex);
		glDeleteShader(Fragment);
		GLint Ok = 0;
		glGetProgramiv(m_CcBlocksProgram, GL_LINK_STATUS, &Ok);
		if(!Ok)
		{
			char aLog[1024] = "";
			glGetProgramInfoLog(m_CcBlocksProgram, sizeof(aLog), nullptr, aLog);
			log_error("chainsaw", "blocks shader link failed: %s", aLog);
			glDeleteProgram(m_CcBlocksProgram);
			m_CcBlocksProgram = 0;
			m_CcBlocksFailed = true;
			return false;
		}
	}
	return true;
}

void CCommandProcessorFragment_OpenGL3_3::Cmd_CcBlocks(const SCommand_CcBlocks *pCommand)
{
	const SCcBlocksParams &P = pCommand->m_Params;
	if(P.m_Style != 0)
	{
		CcBlocksPackDraw(P); // a style out of a texture pack
		return;
	}
	// texture units 0 and 2..7 (unit 1 is the copy of the screen), in the order of the uniforms below
	const int aIds[7] = {P.m_MatTexture, P.m_SoilATexture, P.m_SoilNTexture, P.m_RockATexture, P.m_RockNTexture, P.m_MossATexture, P.m_MossNTexture};
	for(const int Id : aIds)
	{
		if(Id < 0 || Id >= (int)m_vTextures.size() || m_vTextures[Id].m_Tex == 0)
			return;
	}
	if(!CcBlocksEnsure())
		return;

	const int W = m_BcPostFxW, H = m_BcPostFxH;
	if(W <= 0 || H <= 0)
		return;

	if(m_LastClipEnable)
	{
		glDisable(GL_SCISSOR_TEST);
		m_LastClipEnable = false;
	}
	if(m_BcPostFxVao == 0)
		glGenVertexArrays(1, &m_BcPostFxVao);
	glBindVertexArray(m_BcPostFxVao);

	// 1. the screen so far into the texture: the water bends it (a blit also resolves multisampling)
	glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
	glBindFramebuffer(GL_DRAW_FRAMEBUFFER, m_BcPostFxFbo);
	glBlitFramebuffer(m_ViewportX, m_ViewportY, m_ViewportX + W, m_ViewportY + H, 0, 0, W, H, GL_COLOR_BUFFER_BIT, GL_NEAREST);
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	glViewport(m_ViewportX, m_ViewportY, W, H);

	// 2. the blocks over the screen; the shader gives a color and how much of the pixel it covers
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glUseProgram(m_CcBlocksProgram);
	{
		const TWGLuint Prog = m_CcBlocksProgram;
		glUniform1i(glGetUniformLocation(Prog, "uMat"), 0);
		glUniform1i(glGetUniformLocation(Prog, "uScene"), 1);
		glUniform1i(glGetUniformLocation(Prog, "uSoilA"), 2);
		glUniform1i(glGetUniformLocation(Prog, "uSoilN"), 3);
		glUniform1i(glGetUniformLocation(Prog, "uRockA"), 4);
		glUniform1i(glGetUniformLocation(Prog, "uRockN"), 5);
		glUniform1i(glGetUniformLocation(Prog, "uMossA"), 6);
		glUniform1i(glGetUniformLocation(Prog, "uMossN"), 7);
		glUniform2f(glGetUniformLocation(Prog, "uOrigin"), P.m_OriginX, P.m_OriginY);
		glUniform2f(glGetUniformLocation(Prog, "uSize"), P.m_SizeX, P.m_SizeY);
		glUniform1f(glGetUniformLocation(Prog, "uTime"), P.m_Time);
		glUniform1i(glGetUniformLocation(Prog, "uQuality"), P.m_Quality < 0 ? 0 : (P.m_Quality > 3 ? 3 : P.m_Quality));
		glUniform1i(glGetUniformLocation(Prog, "uDecor"), P.m_Decor);
	}
	// The photo textures repeat over the world and are seen from far away. Without a sampler object the texture's own
	// settings count; the renderer itself always draws with its sampler objects, so setting these here changes nothing for it.
	for(int i = 1; i < 7; i++)
	{
		glActiveTexture(GL_TEXTURE1 + i);
		glBindSampler(1 + i, 0);
		glBindTexture(GL_TEXTURE_2D, m_vTextures[aIds[i]].m_Tex);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	}
	glActiveTexture(GL_TEXTURE1);
	glBindSampler(1, 0);
	glBindTexture(GL_TEXTURE_2D, m_BcPostFxTex);
	glActiveTexture(GL_TEXTURE0);
	glBindSampler(0, 0);
	glBindTexture(GL_TEXTURE_2D, m_vTextures[aIds[0]].m_Tex);
	glDrawArrays(GL_TRIANGLES, 0, 3);

	// leave the state the way the renderer expects it (it remembers its program and blend mode and works on unit 0)
	for(int i = 7; i >= 0; i--)
	{
		glActiveTexture(GL_TEXTURE0 + i);
		glBindTexture(GL_TEXTURE_2D, 0);
	}
	glBindVertexArray(0);
	glUseProgram(0);
	glDisable(GL_BLEND);
	m_LastProgramId = 0;
	m_LastBlendMode = EBlendMode::NONE;
}

// ---- the styles out of a texture pack (bc_blocks_style 1..): the pictures give the look, this shader puts them together ----
// The same text runs on the preview stand (E:\BestClient-backups\blocks_preview\pack_stand\hang\pack.frag).
static const char *const s_pCcBlocksPackFragment = R"(#version 330
// Realistic blocks out of a texture pack: the pictures give the look, the shader only puts them together.
//   uFillSoil / uFillRock  seamless fills, repeated over the world
//   uCap                   strip of grass along every open top of the earth (repeats horizontally); under it, if the pack
//                          has them (uCapRows): the strip of moss for the open tops of the rock and the uneven rim of the rock
//   uSprites               sheet of plants, one per cell, standing on the bottom of the cell
// uSharp = 1: pixel art (borders between texels stay crisp at any zoom, without shimmer), 0: painted (smooth)
uniform sampler2D uMat;
uniform sampler2D uScene;
uniform sampler2D uFillSoil;
uniform sampler2D uFillRock;
uniform sampler2D uCap;
uniform sampler2D uSprites;
uniform sampler2D uDeco; // per block: which grass and which plants lie over it (see cc_blocks_core.h)
uniform vec2 uOrigin;
uniform vec2 uSize;
uniform float uTime;
uniform int uQuality;
uniform int uDecor;
uniform float uPpb; // texels of the pack per block
uniform int uSharp;
uniform float uCapBase; // row of the cap that lies on the top of the block
uniform vec2 uSprCell; // size of a cell of the sheet, texels
uniform vec2 uSprGrid; // cells across, cells down
uniform float uSprCount;
uniform float uCapRows; // strips in uCap, one under the other: 1 grass; 2 + moss of the rock tops; 3 + uneven rim of the rock
uniform float uSprRigid; // the sprites from this number on stand still in the wind (stones, crystals)
uniform sampler2D uFillWater; // seamless fill of the body of the water
uniform sampler2D uWaterStrips; // two strips that repeat along X: the surface, under it the bed (pebbles, water plants)
uniform vec4 uWater; // x rows of the surface strip (0: the pack has no water, the formula draws it), y its row on the top of the block, z rows of the bed strip,
// w the sprites from this number on float on the water (0: none)
uniform sampler2D uHangSprites; // sheet of the things that hang from the ceilings (vines, moss), the top of a cell is the ceiling
uniform vec4 uHang; // x y size of a cell of that sheet, texels, z cells across (0: the pack has none), w the number of its first sprite
uniform float uHangOver; // rows of a cell that lie over the block of the ceiling
uniform float uOutline; // 1: the dark outline of the earth and the rock, 0: none
uniform float uClear; // how clear the water is: 0 only the bright things behind it show (a tee), 1 almost everything does
in vec2 vUv;
out vec4 FragColor;

vec2 gFw; // texels of the pack per screen pixel
float gLod;

float Hash(vec2 p)
{
	vec3 p3 = fract(vec3(p.xyx) * 0.1031);
	p3 += dot(p3, p3.yzx + 33.33);
	return fract((p3.x + p3.y) * p3.z);
}

ivec4 Tile(ivec2 p)
{
	ivec2 c = clamp(p, ivec2(0), textureSize(uMat, 0) - 1);
	ivec4 t = ivec4(texelFetch(uMat, c, 0) * 255.0 + 0.5);
	if(c != p)
	{
		// outside of the map the border blocks go on as one mass: no outlines, no grass, no surface
		t.g = 255;
		t.a = 0;
		if(t.r == 0)
			t.b = 0;
	}
	return t;
}

// texel coordinate -> the same with crisp borders between the texels (one screen pixel of blend, no shimmer)
vec2 Crisp(vec2 t)
{
	if(uSharp == 0)
		return t;
	vec2 f = fract(t);
	return floor(t) + clamp(f / gFw, 0.0, 0.5) + clamp((f - 1.0) / gFw + 0.5, 0.0, 0.5);
}

vec4 Pick(sampler2D Tex, vec2 t)
{
	return textureLod(Tex, Crisp(t) / vec2(textureSize(Tex, 0)), gLod);
}

vec4 Over(vec4 Top, vec4 Bottom)
{
	float a = Top.a + Bottom.a * (1.0 - Top.a);
	return vec4((Top.rgb * Top.a + Bottom.rgb * Bottom.a * (1.0 - Top.a)) / max(a, 0.0001), a);
}

// The uneven rim of the rock (third strip of uCap): stones stick out of the block, on every block in their own way.
// Along: texels along the edge, Out: texels from the edge of the block into the air (below zero: inside the block).
// a = how much of the texel the rock covers, r = how much of that is the dark outline.
vec4 RockRim(float Along, float Out)
{
	float CapH = float(textureSize(uCap, 0).y) / uCapRows;
	float y = uCapBase - Out;
	if(y < 0.5 || y > CapH - 0.5)
		return vec4(0.0);
	return Pick(uCap, vec2(Along, y + CapH * 2.0));
}

// What is behind the water shows through it. A bright thing (a tee) stays clear to see; a dark one (a tee in the freeze, the
// dark tile the map draws there) shows as much as uClear says. The water loses only half of what the dark picture
// behind it would take: a map that draws black tiles under its freeze does not turn the water black.
vec3 Behind(vec3 c, vec3 Scene)
{
	float See = smoothstep(0.15, 0.42, max(Scene.r, max(Scene.g, Scene.b)));
	float Thru = mix(0.60 * uClear * uClear, 0.70 + 0.25 * uClear, See);
	return c * (1.0 - Thru * mix(0.5, 1.0, See)) + Scene * Thru;
}

// q: place in blocks, Deep: blocks under the surface, Under: texels under the waving surface (big = far below)
vec3 Water(vec2 q, float Deep, float Under)
{
	vec3 c = mix(vec3(0.17, 0.52, 0.80), vec3(0.03, 0.15, 0.38), clamp(Deep / 5.0, 0.0, 1.0));
	vec2 Uv = vUv;
	if(uQuality >= 2)
		Uv.x += sin(q.y * 7.0 + uTime * 1.7) * 0.0012;
	c = Behind(c, textureLod(uScene, Uv, 0.0).rgb);
	if(uQuality >= 1)
	{
		// slow cells of light, weaker in the deep
		float n = sin(q.x * 3.1 + uTime * 0.9 + sin(q.y * 2.3 + uTime * 0.6) * 1.7) * sin(q.y * 3.7 - uTime * 0.7 + sin(q.x * 1.9 + uTime * 0.3) * 1.3);
		c += vec3(0.16, 0.26, 0.26) * smoothstep(0.66, 0.74, n) * exp(-Deep * 0.45);
	}
	// foam on the surface, a lighter band right under it
	c = mix(c, vec3(0.50, 0.80, 0.95), 0.55 * clamp(3.5 - Under, 0.0, 1.0));
	c = mix(c, vec3(0.86, 0.95, 1.0), clamp(1.5 - Under, 0.0, 1.0));
	return c;
}

// The water of the pack. a: place in texels, In: texels from the top left corner of the block, Deep: blocks under the surface.
// The picture only: what is behind the water and the foam of the open sides are added by the caller.
vec3 WaterPack(vec2 a, ivec2 Ip, ivec4 t, vec2 In, float Deep, float Lift)
{
	// body: the rows of texels sway one after another
	vec2 b = a;
	if(uQuality >= 2)
	{
		float Sh = sin((uSharp != 0 ? floor(a.y) : a.y) * 0.55 + uTime * 1.3) * 1.2;
		b.x += uSharp != 0 ? floor(Sh + 0.5) : Sh;
	}
	float Dark = mix(1.0, 0.62, clamp((Deep - 1.0) / 6.0, 0.0, 1.0));
	vec3 c = Pick(uFillWater, b).rgb * Dark;
	if(uWater.z > 0.0 && uDecor != 0)
	{
		// bed: pebbles and plants stand on the ground under the water, up to two blocks high
		float Rise = -1.0; // texels above the ground
		if((t.g & 4) == 0 && (t.a & 4) == 0)
		{
			if(t.b > 0)
				Rise = uPpb - In.y;
		}
		else if((t.g & 4) != 0 && uWater.z > uPpb)
		{
			ivec4 d = Tile(Ip + ivec2(0, 1));
			if((d.g & 4) == 0 && (d.a & 4) == 0)
				Rise = 2.0 * uPpb - In.y;
		}
		if(Rise > 0.5 && Rise < uWater.z - 0.5)
		{
			float x = a.x;
			if(uQuality >= 2)
			{
				// the plants lean in the stream, the pebbles lie still
				float Up = clamp((Rise - 9.0) / (uWater.z - 9.0), 0.0, 1.0);
				float Lean = sin(uTime * 1.4 + a.x * 0.05) * Up * Up * 3.0;
				x -= uSharp != 0 ? floor(Lean + 0.5) : Lean;
			}
			vec4 s = Pick(uWaterStrips, vec2(x, uWater.x + uWater.z - Rise));
			c = mix(c, s.rgb * Dark, s.a);
		}
	}
	if((t.a & 1) != 0)
	{
		// surface: seen a little from above, it drifts along and rises with the wave
		float y = uWater.y + In.y + Lift;
		if(y > 0.5 && y < uWater.x - 0.5)
		{
			vec4 s = Pick(uWaterStrips, vec2(a.x + uTime * 1.5, y));
			c = mix(c, s.rgb, s.a);
		}
	}
	return c;
}

// height of the surface above the top of the block, texels
float Wave(float x)
{
	float h = (0.5 + 0.5 * sin(x * 4.0 + uTime * 1.8)) * 0.10 + (0.5 + 0.5 * sin(x * 9.0 - uTime * 2.6)) * 0.06;
	h *= uPpb;
	return uSharp != 0 ? floor(h + 0.5) : h;
}

// the rim of foam on the sides and the bottom: mostly one texel, with gaps that travel along it
float Rim(float x)
{
	float h = clamp(sin(x * 6.0 + uTime * 1.4) * 2.0 - 1.0, 0.0, 1.0) * 0.07 * uPpb;
	return uSharp != 0 ? floor(h + 0.5) : h;
}

void main()
{
	vec2 w = uOrigin + vec2(vUv.x, 1.0 - vUv.y) * uSize;
	vec2 a = w * uPpb;
	gFw = max(abs(dFdx(a)) + abs(dFdy(a)), vec2(0.0001));
	gLod = max(log2(max(gFw.x, gFw.y)), 0.0);
	vec2 q = Crisp(a) / uPpb; // for everything drawn by a formula: steps like the texels do
	ivec2 Ip = ivec2(floor(w));
	vec2 fp = clamp(q - vec2(Ip), 0.0, 1.0);
	float U = 1.0 / uPpb;

	ivec4 t = Tile(Ip);
	bool Solid = t.r == 1 || t.r == 2;
	vec4 o = vec4(0.0);

	if(Solid)
	{
		vec3 c;
		if(t.r == 1)
			c = Pick(uFillSoil, a).rgb; // one color at any depth
		else
			c = Pick(uFillRock, a).rgb;
		if(t.g != 255)
		{
		// distance to the border of the material, blocks; l: the same for the outline. The sides of the rock that look
		// into the air have an uneven rim with its own outline (AirSides), their straight line is left out.
		int AirSides = t.r == 2 && uCapRows >= 3.0 ? (t.a & 15) : 0;
		float e = 9.0, l = 9.0;
		if((t.g & 1) == 0)
		{
			e = min(e, fp.y);
			if((AirSides & 1) == 0)
				l = min(l, fp.y);
		}
		if((t.g & 4) == 0)
		{
			e = min(e, 1.0 - fp.y);
			if((AirSides & 4) == 0)
				l = min(l, 1.0 - fp.y);
		}
		if((t.g & 8) == 0)
		{
			e = min(e, fp.x);
			if((AirSides & 8) == 0)
				l = min(l, fp.x);
		}
		if((t.g & 2) == 0)
		{
			e = min(e, 1.0 - fp.x);
			if((AirSides & 2) == 0)
				l = min(l, 1.0 - fp.x);
		}
		float Corner = 9.0;
		if((t.g & 16) == 0)
			Corner = min(Corner, max(1.0 - fp.x, fp.y));
		if((t.g & 32) == 0)
			Corner = min(Corner, max(1.0 - fp.x, 1.0 - fp.y));
		if((t.g & 64) == 0)
			Corner = min(Corner, max(fp.x, 1.0 - fp.y));
		if((t.g & 128) == 0)
			Corner = min(Corner, max(fp.x, fp.y));
		e = min(e, Corner);
		l = min(l, Corner);
		float Line, Shade, Lit;
		if(uSharp != 0)
		{
			Line = clamp(1.5 - l / U, 0.0, 1.0); // one texel of outline
			Shade = mix(0.60, 1.0, clamp((e / U - 1.0) / 4.0, 0.0, 1.0));
			Lit = clamp(2.5 - fp.y / U, 0.0, 1.0);
		}
		else
		{
			Line = 1.0 - smoothstep(0.02, 0.07, l);
			Shade = mix(0.58, 1.0, smoothstep(0.0, 0.40, e));
			Lit = 1.0 - smoothstep(0.05, 0.16, fp.y);
		}
		if(AirSides != 0)
		{
			// the outline goes on along the block only where no stone sticks out of it; in places it is a deeper dent
			vec2 In = a - vec2(Ip) * uPpb; // texels from the top left corner of the block
			if((AirSides & 1) != 0 && In.y < 3.0)
				Line = max(Line, RockRim(a.x + float(Ip.y) * 37.0, -In.y).r);
			if((AirSides & 4) != 0 && uPpb - In.y < 3.0)
				Line = max(Line, RockRim(a.x + float(Ip.y) * 37.0 + 61.0, In.y - uPpb).r);
			if((AirSides & 8) != 0 && In.x < 3.0)
				Line = max(Line, RockRim(a.y + float(Ip.x) * 53.0 + 97.0, -In.x).r);
			if((AirSides & 2) != 0 && uPpb - In.x < 3.0)
				Line = max(Line, RockRim(a.y + float(Ip.x) * 53.0 + 23.0, In.x - uPpb).r);
		}
		c *= Shade;
		// the top of the rock catches the light
		if(t.r == 2 && (t.a & 1) != 0)
			c *= 1.0 + 0.45 * Lit;
		c = mix(c, vec3(0.070, 0.058, 0.050), Line * uOutline);
		}
		o = vec4(c, 1.0);
	}
	else if(t.r == 3)
	{
		// texels to the nearest border with the air: the top waves, the sides and the bottom get their own rim of foam
		float Under = 999.0;
		bool Packed = uWater.x > 0.0; // the pack has its own water: the top is its surface strip
		if((t.a & 1) != 0 && !Packed)
			Under = fp.y * uPpb + Wave(q.x);
		if((t.a & 8) != 0)
			Under = min(Under, fp.x * uPpb + Rim(q.y));
		if((t.a & 2) != 0)
			Under = min(Under, (1.0 - fp.x) * uPpb + Rim(q.y + 3.7));
		if((t.a & 4) != 0)
			Under = min(Under, (1.0 - fp.y) * uPpb + Rim(q.x + 1.3));
		// corners where the water goes on both ways but the block across the corner is air
		if((t.g & 137) == 9 && Tile(Ip + ivec2(-1, -1)).r == 0)
			Under = min(Under, max(fp.x, fp.y) * uPpb);
		if((t.g & 19) == 3 && Tile(Ip + ivec2(1, -1)).r == 0)
			Under = min(Under, max(1.0 - fp.x, fp.y) * uPpb);
		if((t.g & 38) == 6 && Tile(Ip + ivec2(1, 1)).r == 0)
			Under = min(Under, max(1.0 - fp.x, 1.0 - fp.y) * uPpb);
		if((t.g & 76) == 12 && Tile(Ip + ivec2(-1, 1)).r == 0)
			Under = min(Under, max(fp.x, 1.0 - fp.y) * uPpb);
		if(Packed)
		{
			vec3 c = WaterPack(a, Ip, t, a - vec2(Ip) * uPpb, float(t.b) + fp.y, floor(Wave(q.x) * 0.5 + 0.5));
			c = Behind(c, textureLod(uScene, vUv, 0.0).rgb);
			c = mix(c, vec3(0.50, 0.80, 0.95), 0.55 * clamp(3.5 - Under, 0.0, 1.0));
			c = mix(c, vec3(0.86, 0.95, 1.0), clamp(1.5 - Under, 0.0, 1.0));
			o = vec4(c, 1.0);
		}
		else
			o = vec4(Water(q, float(t.b) + fp.y, Under), 1.0);
	}
	else
	{
		if(uQuality >= 3 && t.g != 255)
		{
			// the blocks shade what is behind them: the light comes from the top left
			float s = 0.0;
			if((t.g & 1) == 0)
				s = max(s, 1.0 - fp.y / 0.55);
			if((t.g & 8) == 0)
				s = max(s, 1.0 - fp.x / 0.40);
			if((t.g & 128) == 0)
				s = max(s, 1.0 - max(fp.x, fp.y) / 0.45);
			if(uSharp != 0)
				s = floor(s * 4.0 + 0.5) * 0.25;
			o = vec4(0.02, 0.03, 0.07, 0.24 * s * s);
		}
		if(t.r == 0 && (t.b & 15) != 0 && uCapRows >= 3.0)
		{
			// the stones of the rock next to this block stick out into it: 4 the rock is below, 1 above, 8 left, 2 right
			vec2 In = a - vec2(Ip) * uPpb;
			vec4 m = vec4(0.0);
			float Light = 1.0;
			if((t.b & 4) != 0)
			{
				m = RockRim(a.x + float(Ip.y + 1) * 37.0, uPpb - In.y);
				Light = 1.16; // looks up: lit
			}
			if((t.b & 8) != 0)
			{
				vec4 s = RockRim(a.y + float(Ip.x - 1) * 53.0 + 23.0, In.x);
				if(s.a > m.a)
				{
					m = s;
					Light = 0.86;
				}
			}
			if((t.b & 2) != 0)
			{
				vec4 s = RockRim(a.y + float(Ip.x + 1) * 53.0 + 97.0, uPpb - In.x);
				if(s.a > m.a)
				{
					m = s;
					Light = 1.06;
				}
			}
			if((t.b & 1) != 0)
			{
				vec4 s = RockRim(a.x + float(Ip.y - 1) * 37.0 + 61.0, In.y);
				if(s.a > m.a)
				{
					m = s;
					Light = 0.74; // looks down: in the shade
				}
			}
			if(m.a > 0.0)
				o = Over(vec4(mix(Pick(uFillRock, a).rgb * Light, vec3(0.070, 0.058, 0.050), m.r * uOutline), m.a), o);
		}
		if((t.a & 32) != 0)
		{
			// the wave rises over the top of the water block: nothing of the tile of the map is left to see
			if(uWater.x > 0.0)
			{
				// the crest of the surface strip of the pack
				float y = uWater.y + a.y - float(Ip.y + 1) * uPpb + floor(Wave(q.x) * 0.5 + 0.5);
				if(y > 0.5)
					o = Over(Pick(uWaterStrips, vec2(a.x + uTime * 1.5, y)), o);
			}
			else
			{
				float Under = Wave(q.x) - (1.0 - fp.y) * uPpb;
				float Cover = clamp(Under + 0.5, 0.0, 1.0);
				if(Cover > 0.0)
					o = Over(vec4(Water(q, 0.0, max(Under, 0.0)), Cover), o);
			}
		}
	}
	bool Hung = uHang.z > 0.0 && (t.r == 0 ? (t.b & 16) != 0 : (Solid && (t.a & 32) != 0));
	if((t.a & 208) == 0 && !Hung)
	{
		// nothing green reaches this block (16 from the earth, 64 from the rock, 128 from the water, Hung from a ceiling): done
		// (the sky and the inside of the ground end here)
		FragColor = o;
		return;
	}
	// R: grass strips over this block, G B: plants over it, A: where the plants stand
	ivec4 d = ivec4(texelFetch(uDeco, Ip, 0) * 255.0 + 0.5);

	// grass (or the moss of the rock) along the open tops: on the block itself, sticking up into the air, hanging a little over the sides
	if((d.r & 63) != 0)
	{
		// over the side: a few texels, every row its own length
		float Row = floor(q.y * uPpb);
		bool Left = fp.x * uPpb <= 1.0 + floor(Hash(vec2(Row, float(Ip.x - 1))) * 3.0);
		bool Right = (1.0 - fp.x) * uPpb <= 1.0 + floor(Hash(vec2(Row, float(Ip.x + 1))) * 3.0);
		bool aOn[2];
		aOn[0] = (d.r & 1) != 0 || ((d.r & 4) != 0 && Left) || ((d.r & 8) != 0 && Right);
		aOn[1] = (d.r & 2) != 0 || ((d.r & 16) != 0 && Left) || ((d.r & 32) != 0 && Right);
		float CapH = float(textureSize(uCap, 0).y) / uCapRows;
		for(int i = 0; i < 2; i++)
		{
			float y = a.y - float(Ip.y + i) * uPpb + uCapBase;
			if(aOn[i] && y > 0.5 && y < CapH - 0.5)
			{
				// the second strip is the moss of the rock
				bool Moss = (d.r & (i == 0 ? 64 : 128)) != 0;
				o = Over(Pick(uCap, vec2(a.x, Moss ? y + CapH : y)), o);
			}
		}
	}

	if(uDecor != 0 && uQuality >= 1 && d.g != 0)
	{
		for(int i = 0; i < 2; i++)
		{
			int Ref = i == 0 ? d.g : d.b;
			if(Ref == 0)
				break;
			int Where = i == 0 ? (d.a & 15) : (d.a >> 4);
			float Idx = float(Ref - 1);
			if(uHang.z > 0.0 && Idx >= uHang.w)
			{
				// a thing that hangs: Bp is the block of its ceiling, the top of the cell lies uHangOver rows over that block
				ivec2 Bp = Ip + ivec2(Where % 3 - 1, -(Where / 3));
				float Num = Idx - uHang.w;
				vec2 Cell = vec2(mod(Num, uHang.z), floor(Num / uHang.z));
				vec2 L = a - vec2((float(Bp.x) + 0.5) * uPpb - uHang.x * 0.5, float(Bp.y + 1) * uPpb - uHangOver);
				if(L.y <= 0.5 || L.y >= uHang.y - 0.5)
					continue;
				if(uQuality >= 2)
				{
					// the wind: the further below the ceiling, the further it swings
					float Down = clamp((L.y - uHangOver) / (2.5 * uPpb), 0.0, 1.0);
					float Sway = sin(uTime * 1.3 + float(Bp.x) * 1.7 + float(Bp.y) * 0.9) * 0.65 + sin(uTime * 2.3 + float(Bp.x) * 0.6) * 0.35;
					float Lean = Sway * Down * Down * 0.16 * uPpb;
					L.x -= uSharp != 0 ? floor(Lean + 0.5) : Lean;
				}
				if(L.x <= 0.5 || L.x >= uHang.x - 0.5)
					continue;
				if(Hash(vec2(Bp) + 17.7) > 0.5)
					L.x = uHang.x - L.x;
				o = Over(Pick(uHangSprites, Cell * uHang.xy + L), o);
				continue;
			}
			ivec2 Bp = Ip + ivec2(Where % 3 - 1, Where / 3);
			vec2 Cell = vec2(mod(Idx, uSprGrid.x), floor(Idx / uSprGrid.x));
			float Jit = floor((Hash(vec2(Bp) + 3.3) - 0.5) * 0.5 * uPpb);
			vec2 Foot = vec2((float(Bp.x) + 0.5) * uPpb + Jit, (float(Bp.y) + 0.14) * uPpb);
			bool Floats = uWater.w > 0.0 && Idx >= uWater.w;
			if(Floats)
				Foot.y = (float(Bp.y) + 0.5) * uPpb - floor(Wave(Foot.x / uPpb) * 0.5 + 0.5); // on the surface, up and down with the wave
			vec2 L = a - (Foot - vec2(uSprCell.x * 0.5, uSprCell.y));
			if(L.y <= 0.5 || L.y >= uSprCell.y - 0.5)
				continue;
			if(uQuality >= 2 && Idx < uSprRigid && !Floats)
			{
				// the wind: the higher above the foot, the further it leans
				float Up = clamp((uSprCell.y - 1.0 - L.y) / uSprCell.y, 0.0, 1.0);
				float Sway = sin(uTime * 1.6 + float(Bp.x) * 1.9) * 0.6 + sin(uTime * 2.9 + float(Bp.x) * 0.7) * 0.4;
				float Lean = Sway * Up * Up * 0.24 * uPpb;
				L.x -= uSharp != 0 ? floor(Lean + 0.5) : Lean;
			}
			if(L.x <= 0.5 || L.x >= uSprCell.x - 0.5)
				continue;
			if(Hash(vec2(Bp) + 11.1) > 0.5)
				L.x = uSprCell.x - L.x;
			o = Over(Pick(uSprites, Cell * uSprCell + L), o);
		}
	}
	FragColor = o;
}
)";

// order of m_aCcBlocksPackLoc
static const char *const s_apCcBlocksPackUniforms[] = {
	"uMat", "uScene", "uFillSoil", "uFillRock", "uCap", "uSprites", "uDeco", // samplers: texture units 0..6
	"uOrigin", "uSize", "uTime", "uQuality", "uDecor", "uPpb", "uSharp", "uCapBase", "uSprCell", "uSprGrid", "uSprCount", "uCapRows", "uSprRigid",
	"uFillWater", "uWaterStrips", // samplers of the water of the pack: texture units 7, 8
	"uWater",
	"uHangSprites", // sampler of the things that hang from the ceilings: texture unit 9
	"uHang", "uHangOver", "uOutline", "uClear"};

bool CCommandProcessorFragment_OpenGL3_3::CcBlocksPackEnsure()
{
	if(m_CcBlocksPackFailed || !BcPostFxEnsure())
		return false;
	if(m_CcBlocksPackProgram == 0)
	{
		const TWGLuint Vertex = BcPostFxCompile(GL_VERTEX_SHADER, s_pBcPostFxVertex);
		const TWGLuint Fragment = BcPostFxCompile(GL_FRAGMENT_SHADER, s_pCcBlocksPackFragment);
		if(Vertex == 0 || Fragment == 0)
		{
			if(Vertex)
				glDeleteShader(Vertex);
			if(Fragment)
				glDeleteShader(Fragment);
			m_CcBlocksPackFailed = true;
			return false;
		}
		m_CcBlocksPackProgram = glCreateProgram();
		glAttachShader(m_CcBlocksPackProgram, Vertex);
		glAttachShader(m_CcBlocksPackProgram, Fragment);
		glLinkProgram(m_CcBlocksPackProgram);
		glDeleteShader(Vertex);
		glDeleteShader(Fragment);
		GLint Ok = 0;
		glGetProgramiv(m_CcBlocksPackProgram, GL_LINK_STATUS, &Ok);
		if(!Ok)
		{
			char aLog[1024] = "";
			glGetProgramInfoLog(m_CcBlocksPackProgram, sizeof(aLog), nullptr, aLog);
			log_error("chainsaw", "blocks pack shader link failed: %s", aLog);
			glDeleteProgram(m_CcBlocksPackProgram);
			m_CcBlocksPackProgram = 0;
			m_CcBlocksPackFailed = true;
			return false;
		}
		static_assert(sizeof(s_apCcBlocksPackUniforms) / sizeof(s_apCcBlocksPackUniforms[0]) == sizeof(m_aCcBlocksPackLoc) / sizeof(m_aCcBlocksPackLoc[0]), "one place per uniform");
		for(size_t i = 0; i < sizeof(s_apCcBlocksPackUniforms) / sizeof(s_apCcBlocksPackUniforms[0]); i++)
			m_aCcBlocksPackLoc[i] = glGetUniformLocation(m_CcBlocksPackProgram, s_apCcBlocksPackUniforms[i]);
	}
	return true;
}

void CCommandProcessorFragment_OpenGL3_3::CcBlocksPackDraw(const SCcBlocksParams &P)
{
	// texture units 0 and 2..6 (unit 1 is the copy of the screen)
	const int aIds[6] = {P.m_MatTexture, P.m_PackSoilTexture, P.m_PackRockTexture, P.m_PackCapTexture, P.m_PackSpritesTexture, P.m_DecoTexture};
	for(const int Id : aIds)
	{
		if(Id < 0 || Id >= (int)m_vTextures.size() || m_vTextures[Id].m_Tex == 0)
			return;
	}
	if(!CcBlocksPackEnsure())
		return;

	const int W = m_BcPostFxW, H = m_BcPostFxH;
	if(W <= 0 || H <= 0)
		return;

	if(m_LastClipEnable)
	{
		glDisable(GL_SCISSOR_TEST);
		m_LastClipEnable = false;
	}
	if(m_BcPostFxVao == 0)
		glGenVertexArrays(1, &m_BcPostFxVao);
	glBindVertexArray(m_BcPostFxVao);

	// 1. the screen so far into the texture, only with water in sight: what is in the water shows through it
	if(P.m_NeedScene)
	{
		glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
		glBindFramebuffer(GL_DRAW_FRAMEBUFFER, m_BcPostFxFbo);
		glBlitFramebuffer(m_ViewportX, m_ViewportY, m_ViewportX + W, m_ViewportY + H, 0, 0, W, H, GL_COLOR_BUFFER_BIT, GL_NEAREST);
	}
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	glViewport(m_ViewportX, m_ViewportY, W, H);

	// 2. the blocks over the screen; the shader gives a color and how much of the pixel it covers
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glUseProgram(m_CcBlocksPackProgram);
	const int *pLoc = m_aCcBlocksPackLoc;
	for(int i = 0; i < 7; i++)
		glUniform1i(pLoc[i], i);
	glUniform2f(pLoc[7], P.m_OriginX, P.m_OriginY);
	glUniform2f(pLoc[8], P.m_SizeX, P.m_SizeY);
	glUniform1f(pLoc[9], P.m_Time);
	glUniform1i(pLoc[10], P.m_Quality < 0 ? 0 : (P.m_Quality > 3 ? 3 : P.m_Quality));
	glUniform1i(pLoc[11], P.m_Decor);
	glUniform1f(pLoc[12], P.m_PackPpb);
	glUniform1i(pLoc[13], P.m_PackSharp);
	glUniform1f(pLoc[14], P.m_PackCapBase);
	glUniform2f(pLoc[15], P.m_PackSprCellW, P.m_PackSprCellH);
	glUniform2f(pLoc[16], P.m_PackSprCols, 1.0f);
	glUniform1f(pLoc[17], P.m_PackSprCount);
	glUniform1f(pLoc[18], P.m_PackCapRows >= 3.0f ? 3.0f : (P.m_PackCapRows >= 2.0f ? 2.0f : 1.0f));
	glUniform1f(pLoc[19], P.m_PackSprRigid);
	// the water of the pack: both of its textures or none (then the formula draws the water)
	const auto Loaded = [this](int Id) {
		return Id >= 0 && Id < (int)m_vTextures.size() && m_vTextures[Id].m_Tex != 0;
	};
	const bool Water = P.m_PackWaterTopRows > 0.0f && Loaded(P.m_PackWaterTexture) && Loaded(P.m_PackWaterStripsTexture);
	glUniform1i(pLoc[20], 7);
	glUniform1i(pLoc[21], 8);
	glUniform4f(pLoc[22], Water ? P.m_PackWaterTopRows : 0.0f, P.m_PackWaterTopBase, P.m_PackWaterBedRows, Water ? P.m_PackWaterSprFirst : 0.0f);
	// the things that hang from the ceilings: with their sheet, or none
	const bool Hang = P.m_PackHangCols > 0.0f && P.m_PackHangFirst > 0.0f && Loaded(P.m_PackHangTexture);
	glUniform1i(pLoc[23], 9);
	glUniform4f(pLoc[24], P.m_PackHangCellW, P.m_PackHangCellH, Hang ? P.m_PackHangCols : 0.0f, P.m_PackHangFirst);
	glUniform1f(pLoc[25], P.m_PackHangOverlap);
	glUniform1f(pLoc[26], P.m_PackOutline < 0.0f ? 0.0f : (P.m_PackOutline > 1.0f ? 1.0f : P.m_PackOutline));
	glUniform1f(pLoc[27], P.m_PackWaterClear < 0.0f ? 0.0f : (P.m_PackWaterClear > 1.0f ? 1.0f : P.m_PackWaterClear));

	// The fills repeat over the world, the strip of grass repeats sideways, the sheet of plants does not repeat.
	// Without a sampler object the texture's own settings count; the renderer itself always draws with its sampler
	// objects, so setting these here changes nothing for it.
	static const GLint s_aWrapS[4] = {GL_REPEAT, GL_REPEAT, GL_REPEAT, GL_CLAMP_TO_EDGE};
	static const GLint s_aWrapT[4] = {GL_REPEAT, GL_REPEAT, GL_CLAMP_TO_EDGE, GL_CLAMP_TO_EDGE};
	for(int i = 0; i < 4; i++)
	{
		glActiveTexture(GL_TEXTURE2 + i);
		glBindSampler(2 + i, 0);
		glBindTexture(GL_TEXTURE_2D, m_vTextures[aIds[1 + i]].m_Tex);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, s_aWrapS[i]);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, s_aWrapT[i]);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	}
	if(Water)
	{
		// the body repeats over the world, the strips repeat sideways
		const int aWaterIds[2] = {P.m_PackWaterTexture, P.m_PackWaterStripsTexture};
		for(int i = 0; i < 2; i++)
		{
			glActiveTexture(GL_TEXTURE7 + i);
			glBindSampler(7 + i, 0);
			glBindTexture(GL_TEXTURE_2D, m_vTextures[aWaterIds[i]].m_Tex);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, i == 0 ? GL_REPEAT : GL_CLAMP_TO_EDGE);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		}
	}
	if(Hang)
	{
		// the sheet of hanging things does not repeat
		glActiveTexture(GL_TEXTURE9);
		glBindSampler(9, 0);
		glBindTexture(GL_TEXTURE_2D, m_vTextures[P.m_PackHangTexture].m_Tex);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	}
	// the decor texture is read pixel by pixel, like the material texture
	glActiveTexture(GL_TEXTURE6);
	glBindSampler(6, 0);
	glBindTexture(GL_TEXTURE_2D, m_vTextures[aIds[5]].m_Tex);
	glActiveTexture(GL_TEXTURE1);
	glBindSampler(1, 0);
	glBindTexture(GL_TEXTURE_2D, m_BcPostFxTex);
	glActiveTexture(GL_TEXTURE0);
	glBindSampler(0, 0);
	glBindTexture(GL_TEXTURE_2D, m_vTextures[aIds[0]].m_Tex);
	glDrawArrays(GL_TRIANGLES, 0, 3);

	// leave the state the way the renderer expects it (it remembers its program and blend mode and works on unit 0)
	for(int i = Hang ? 9 : (Water ? 8 : 6); i >= 0; i--)
	{
		glActiveTexture(GL_TEXTURE0 + i);
		glBindTexture(GL_TEXTURE_2D, 0);
	}
	glBindVertexArray(0);
	glUseProgram(0);
	glDisable(GL_BLEND);
	m_LastProgramId = 0;
	m_LastBlendMode = EBlendMode::NONE;
}
