// The night sky of the sky mod: the global values that the mod sets, and the effects.
#ifndef MOA_SKY_COMMON_INCLUDED
#define MOA_SKY_COMMON_INCLUDED

// x: 1 when the effects of the mod are on. y: the mip of the night cubemap for the sky behind the stars.
// z: 1 to keep the painted stars of the game. w: the strength of the Milky Way.
float4 _MoaSkyParams;

#include "MoaTheme.hlsl"
#include "MoaStars.hlsl"
#include "MoaAurora.hlsl"

bool MoaSkyEffectsOn()
{
    return _MoaSkyParams.x > 0.5;
}

float MoaNightMip()
{
    return _MoaSkyParams.y;
}

bool MoaPaintedStarsOn()
{
    return _MoaSkyParams.z > 0.5;
}

// Return the light that the mod adds to the sky in the world direction `direction` (length 1). `nightSoft` is the
// night cubemap without the painted stars. The blend values are those of the game: 1 at full night.
float3 MoaNightSky(float3 direction, float3 nightSoft, float sunsetBlend, float nightBlend, float2 pixel)
{
    float altitude = direction.y;
    if (altitude <= 0.0)
    {
        return 0;
    }
    // The painted clouds of the night cubemap hide the stars and the aurora. The night sky of the mod has no clouds.
    float level = dot(nightSoft, float3(0.2126, 0.7152, 0.0722));
    float clear = MoaThemeNightOn() ? 1.0 : 1.0 - smoothstep(_MoaStarParams.z, _MoaStarParams.w, level);
    // The stars come out with the night of the game. In the twilight only the bright stars show.
    float night = saturate((nightBlend - 0.2) / 0.6);
    float3 light = 0;
    if (_MoaStarParams.x > 0.0 && night > 0.0)
    {
        float limit = _MoaStarParams.y - 5.0 * (1.0 - night);
        // The air dims the stars and makes them redder near the horizon.
        float airMass = 1.0 / max(altitude, 0.03);
        float3 extinction = exp(-float3(0.10, 0.17, 0.32) * (airMass - 1.0));
        // The Milky Way is a dull red in the evil sky.
        float3 milkyWay = MoaMilkyWay(direction) * lerp(1.0, float3(1.0, 0.3, 0.15), saturate(MoaEvilWeight()));
        float3 stars = MoaStars(direction, limit, altitude) + milkyWay * _MoaSkyParams.w;
        light += stars * extinction * night * saturate(altitude * 30.0);
    }
    // The clouds of the game are thin at their edges: some light of the aurora goes through them.
    light *= clear;
    light += MoaAurora(direction, pixel) * (night * lerp(0.35, 1.0, clear) * MoaAuroraLight());
    return light;
}

#endif
