// The real stars of the sky mod: a lookup in the star index texture (Assets/Editor/StarIndexBaker.cs makes it).
#ifndef MOA_STARS_INCLUDED
#define MOA_STARS_INCLUDED

#define MOA_CELLS_PER_FACE 32
#define MOA_INDEX_WIDTH 256u
#define MOA_TABLE_ROWS 24u
// The most entries that the shader reads in one cell. The baker writes the largest count to its log.
#define MOA_MOST_ENTRIES 40u

Texture2D<float4> _MoaStarIndex;

// x: star brightness (0: no stars); y: limiting magnitude; z: night cubemap level of a clear sky; w: of a full cloud.
float4 _MoaStarParams;
// The rows of the matrix from the world to the J2000 axes of the star catalogue.
float4 _MoaCelestial0;
float4 _MoaCelestial1;
float4 _MoaCelestial2;

float3 MoaToCelestial(float3 world)
{
    return float3(dot(_MoaCelestial0.xyz, world), dot(_MoaCelestial1.xyz, world), dot(_MoaCelestial2.xyz, world));
}

int MoaCellCoordinate(float t)
{
    float warped = atan(t) * (4.0 / PI);
    return clamp((int)floor((warped + 1.0) * 0.5 * MOA_CELLS_PER_FACE), 0, MOA_CELLS_PER_FACE - 1);
}

// The same rule as StarIndexBaker.CellOf.
uint MoaStarCell(float3 d)
{
    float3 a = abs(d);
    int face;
    float u;
    float v;
    if (a.x >= a.y && a.x >= a.z)
    {
        face = d.x > 0.0 ? 0 : 1;
        u = d.y / a.x;
        v = d.z / a.x;
    }
    else if (a.y >= a.z)
    {
        face = d.y > 0.0 ? 2 : 3;
        u = d.x / a.y;
        v = d.z / a.y;
    }
    else
    {
        face = d.z > 0.0 ? 4 : 5;
        u = d.x / a.z;
        v = d.y / a.z;
    }
    return (uint)(face * MOA_CELLS_PER_FACE * MOA_CELLS_PER_FACE + MoaCellCoordinate(v) * MOA_CELLS_PER_FACE +
                  MoaCellCoordinate(u));
}

float4 MoaIndexTexel(uint index)
{
    return _MoaStarIndex.Load(int3(index % MOA_INDEX_WIDTH, index / MOA_INDEX_WIDTH, 0));
}

// A hash of a number to 0..1.
float MoaHash(float n)
{
    return frac(sin(n * 12.9898) * 43758.5453);
}

// The colour of a black body at the temperature of a B-V colour index (Ballesteros 2012), with a luminance of 1.
// The colour is mild: the eye sees little colour at night.
float3 MoaStarColour(float colourIndex)
{
    float temperature = 4600.0 * (1.0 / (0.92 * colourIndex + 1.7) + 1.0 / (0.92 * colourIndex + 0.62));
    // Planck's law at the red, green and blue wavelengths (micrometres), divided by the value for 6500 K.
    const float3 wavelength = float3(0.61, 0.55, 0.465);
    float3 planck = 1.0 / (pow(wavelength, 5.0) * (exp(14388.0 / (wavelength * temperature)) - 1.0));
    float3 white = 1.0 / (pow(wavelength, 5.0) * (exp(14388.0 / (wavelength * 6500.0)) - 1.0));
    float3 colour = planck / white;
    colour /= max(dot(colour, float3(0.2126, 0.7152, 0.0722)), 1e-4);
    return lerp(1.0, colour, 0.6);
}

// Return the light of the stars in the world direction `world`. `limit` is the faintest magnitude that shows.
// `altitude` is the sine of the height of the direction above the horizon.
float3 MoaStars(float3 world, float limit, float altitude)
{
    float3 celestial = MoaToCelestial(world);
    // The angle of one pixel, in radians.
    float pixel = max(max(length(ddx(celestial)), length(ddy(celestial))), 1e-5);
    float4 cell = MoaIndexTexel(MoaStarCell(celestial));
    uint first = (uint)cell.x;
    uint count = min((uint)cell.y, MOA_MOST_ENTRIES);
    // The stars twinkle near the horizon, where the light goes through more air.
    float twinkleStrength = 0.6 * saturate(1.0 - altitude / 0.42);
    float3 light = 0;
    for (uint i = 0; i < count; ++i)
    {
        float4 star = MoaIndexTexel(MOA_TABLE_ROWS * MOA_INDEX_WIDTH + first + i);
        float magnitude = floor(star.w) / 100.0 - 2.0;
        if (magnitude > limit)
        {
            // The list is brightest first: all the next stars are fainter.
            break;
        }
        float distance = length(celestial - star.xyz);
        // Brighter stars are a little larger. The size follows the pixel, so a star is sharp at each resolution.
        // The depth of field of the game blurs the far sky: then the stars are larger, so that they keep more light.
        float sigma = pixel * (0.55 + 0.1 * max(0.0, 3.5 - magnitude)) * MoaStarSize();
        if (distance > 4.0 * sigma)
        {
            continue;
        }
        // 10^(-0.4 * 0.65 * (V - 6)): a perceptual scale of the flux, 0.04 at magnitude 6.
        float peak = 0.04 * exp2(-0.4 * 0.65 * 3.321928 * (magnitude - 6.0));
        float fade = saturate((limit - magnitude) / 0.75);
        float seed = star.w * 7.31 + star.x * 1031.0;
        float beat = _Time.y * 6.0;
        float wave = lerp(MoaHash(seed + floor(beat)), MoaHash(seed + floor(beat) + 1.0), smoothstep(0.0, 1.0, frac(beat)));
        float twinkle = 1.0 + twinkleStrength * (wave * 2.0 - 1.0);
        // The stars of the evil sky burn: they flicker as embers, also high in the sky.
        twinkle = lerp(twinkle, twinkle * MoaEmberFlicker(seed, _Time.y), saturate(MoaEvilWeight()));
        float shape = exp(-0.5 * distance * distance / (sigma * sigma));
        float3 colour = MoaThemeStarColour(MoaStarColour(frac(star.w) / 0.99 * 2.5 - 0.5), seed);
        light += colour * (peak * fade * twinkle * shape);
    }
    return light * (_MoaStarParams.x * MoaStarLight());
}

// A soft band of light along the plane of the galaxy, brighter toward the centre of the galaxy. The galactic pole
// and centre are the J2000 directions of the IAU definition.
float3 MoaMilkyWay(float3 world)
{
    const float3 galacticPole = float3(-0.86767, -0.19807, 0.45599);
    const float3 galacticCentre = float3(-0.05487, -0.87344, -0.48384);
    float3 celestial = MoaToCelestial(world);
    float latitude = asin(clamp(dot(celestial, galacticPole), -1.0, 1.0));
    float towardCentre = dot(normalize(celestial - galacticPole * dot(celestial, galacticPole)), galacticCentre);
    float band = exp(-latitude * latitude / (0.16 * 0.16)) * (0.35 + 0.65 * pow(0.5 + 0.5 * towardCentre, 2.0));
    // Patches of light and dark, from a sum of waves over the sky.
    float3 p = celestial * 9.0;
    float patches = 0.8 + 0.2 * sin(p.x + 1.7 * sin(p.y * 1.3)) * sin(p.y * 1.1 + 2.1 * sin(p.z * 0.7)) +
                    0.1 * sin(p.z * 3.1 + p.x * 2.3);
    return float3(0.92, 0.95, 1.0) * (0.012 * band * saturate(patches));
}

#endif
