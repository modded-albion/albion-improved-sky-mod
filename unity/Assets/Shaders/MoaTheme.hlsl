// The alignment theme of the sky mod: the night sky of the mod in the place of the painted night sky of the game.
// A neutral player sees a black sky. A good player sees a faint, slow rainbow. An evil player sees a dark red sky
// with a glow of embers at the horizon and slow smoke. The theme goes from -1 (evil) through 0 (neutral) to +1 (good),
// and the colours change smoothly with it.
#ifndef MOA_THEME_INCLUDED
#define MOA_THEME_INCLUDED

// x: 1 when the night sky of the mod replaces the painted night sky. y: the theme, -1 evil to +1 good. z: the light
// of the aurora (0: no aurora).
float4 _MoaThemeParams;
// x: the scale of the size of the stars (0 is 1). y: the strength of the good colours. z: of the evil colours.
// w: the scale of the light of the stars (0 is 1).
float4 _MoaThemeStyle;

static const float3 k_MoaLuminance = float3(0.2126, 0.7152, 0.0722);

bool MoaThemeNightOn()
{
    return _MoaThemeParams.x > 0.5;
}

// The part of the good colours, 0 to the strength of the settings. It starts softly from neutral.
float MoaGoodWeight()
{
    return smoothstep(0.0, 1.0, saturate(_MoaThemeParams.y)) * _MoaThemeStyle.y;
}

float MoaEvilWeight()
{
    return smoothstep(0.0, 1.0, saturate(-_MoaThemeParams.y)) * _MoaThemeStyle.z;
}

float MoaAuroraLight()
{
    return _MoaThemeParams.z;
}

float MoaStarSize()
{
    return _MoaThemeStyle.x > 0.0 ? _MoaThemeStyle.x : 1.0;
}

float MoaStarLight()
{
    return _MoaThemeStyle.w > 0.0 ? _MoaThemeStyle.w : 1.0;
}

// A colour of the rainbow for a position on the colour circle (0 to 1).
float3 MoaRainbow(float hue)
{
    return 0.5 + 0.5 * cos(6.2831853 * (hue + float3(0.0, 0.6667, 0.3333)));
}

// A colour with a luminance of 1, so that a change of colour does not change the brightness.
float3 MoaUnitLuminance(float3 colour, float leastLuminance)
{
    return colour / max(dot(colour, k_MoaLuminance), leastLuminance);
}

float MoaHash3(float3 p)
{
    p = frac(p * 0.3183099 + 0.1);
    p *= 17.0;
    return frac(p.x * p.y * p.z * (p.x + p.y + p.z));
}

// Smooth value noise in three dimensions, 0 to 1.
float MoaNoise3(float3 x)
{
    float3 i = floor(x);
    float3 f = frac(x);
    f = f * f * (3.0 - 2.0 * f);
    return lerp(lerp(lerp(MoaHash3(i), MoaHash3(i + float3(1, 0, 0)), f.x),
                     lerp(MoaHash3(i + float3(0, 1, 0)), MoaHash3(i + float3(1, 1, 0)), f.x), f.y),
                lerp(lerp(MoaHash3(i + float3(0, 0, 1)), MoaHash3(i + float3(1, 0, 1)), f.x),
                     lerp(MoaHash3(i + float3(0, 1, 1)), MoaHash3(i + float3(1, 1, 1)), f.x), f.y), f.z);
}

// Three octaves of value noise, 0 to about 1.
float MoaSmoke(float3 x)
{
    return 0.55 * MoaNoise3(x) + 0.3 * MoaNoise3(x * 2.03 + 11.7) + 0.15 * MoaNoise3(x * 4.07 + 23.1);
}

// The night sky of the mod in the world direction `direction` (length 1), in linear light. Below the horizon it keeps
// the colour of the horizon, which the land and the sea cover.
float3 MoaNightBackground(float3 direction)
{
    float height = saturate(direction.y);
    float time = _Time.y;

    // Neutral: black, with a very small lift near the horizon so that the edge of the land shows.
    float3 sky = float3(0.0022, 0.0026, 0.0038) * exp(-height * 9.0);

    float good = MoaGoodWeight();
    if (good > 0.0)
    {
        // Wide, soft bands of colour that move slowly over the sky. The colour is pale: a part of white.
        float3 p = direction;
        float phase = 0.35 * sin(dot(p, float3(1.7, 0.4, 1.1)) * 2.0 + time * 0.021) +
                      0.25 * sin(dot(p, float3(-0.6, 1.3, 0.8)) * 3.3 - time * 0.017) + 0.45 * height + time * 0.004;
        float3 hue = MoaRainbow(phase);
        float3 colour = lerp(float3(0.55, 0.55, 0.6), hue, 0.55);
        float veil = 0.6 + 0.4 * MoaSmoke(p * 2.2 + float3(time * 0.006, 0.0, time * 0.004));
        float level = lerp(0.0075, 0.0035, sqrt(height));
        sky += colour * (level * veil * good);
    }

    float evil = MoaEvilWeight();
    if (evil > 0.0)
    {
        // A dark red dome, darker at the top, with a glow of embers at the horizon and slow smoke over it.
        float3 smokePoint = direction * 2.6 + float3(time * 0.011, -time * 0.004, time * 0.007);
        float smoke = MoaSmoke(smokePoint);
        float3 dome = lerp(float3(0.032, 0.0024, 0.0010), float3(0.0065, 0.0004, 0.0002), sqrt(height));
        float pulse = 1.0 + 0.08 * sin(time * 0.6) + 0.05 * sin(time * 1.7 + 1.3);
        float3 embers = float3(0.11, 0.022, 0.004) * exp(-height * 5.0) * pulse;
        float3 hell = (dome + embers) * lerp(0.45, 1.35, smoke);
        // The neutral lift goes: the red takes its place.
        sky = lerp(sky, hell, saturate(evil)) + hell * max(evil - 1.0, 0.0);
    }
    return sky;
}

// The colour of a star for the theme: `natural` is the colour of its B-V index, `seed` a number of the star.
float3 MoaThemeStarColour(float3 natural, float seed)
{
    float good = saturate(MoaGoodWeight());
    float evil = saturate(MoaEvilWeight());
    // Good: strong colours of the whole rainbow, one for each star.
    float3 many = MoaUnitLuminance(pow(MoaRainbow(frac(seed * 0.61803)), 1.6), 0.3);
    // Evil: red to orange, as embers.
    float3 ember = MoaUnitLuminance(lerp(float3(1.0, 0.16, 0.025), float3(1.0, 0.42, 0.07), frac(seed * 0.7548)), 0.3);
    return lerp(lerp(natural, many, good), ember, evil);
}

// A change of the brightness of a star that burns: slow and irregular, with short flares.
float MoaEmberFlicker(float seed, float time)
{
    float beat = time * 2.3 + seed * 13.1;
    float a = frac(sin(floor(beat) * 12.9898 + seed) * 43758.5453);
    float b = frac(sin((floor(beat) + 1.0) * 12.9898 + seed) * 43758.5453);
    float wave = lerp(a, b, smoothstep(0.0, 1.0, frac(beat)));
    return 0.6 + 0.8 * wave * wave;
}

// The colour of the aurora for the theme. A good player sees a soft shimmer of many colours.
float3 MoaThemeAuroraColour(float3 natural, float longitude, float height, float time)
{
    float good = saturate(MoaGoodWeight());
    if (good <= 0.0)
    {
        return natural;
    }
    // The hue changes along the curtain and with the height, and moves slowly, so that one view shows many colours.
    float level = dot(natural, k_MoaLuminance);
    float hue = longitude * 6.0 + height * 0.012 + time * 0.04;
    float3 shimmer = MoaUnitLuminance(pow(MoaRainbow(hue), 1.5) + 0.08, 0.25) * level;
    return lerp(natural, shimmer, 0.85 * good);
}

#endif
