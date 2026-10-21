// The aurora of the sky mod: curtains of light in a shell from 95 km to 320 km above a round Earth. The ray of each
// pixel goes through the shell in steps. At each step the shader finds the geomagnetic latitude and longitude of
// the point, the distance to the folded curtain lines of the band, and the colour for the height:
// green (557.7 nm) from 100 km, red (630 nm) above 200 km, and a thin purple bottom edge.
#ifndef MOA_AURORA_INCLUDED
#define MOA_AURORA_INCLUDED

// x: 1 when the aurora is on; y: geomagnetic latitude of the edge of the band (degrees); z: width. The theme gives
// the light of the aurora (MoaAuroraLight).
float4 _MoaAuroraParams;
// The rows of the matrix from the world to the geomagnetic axes (z to the north geomagnetic pole).
float4 _MoaGeomagnetic0;
float4 _MoaGeomagnetic1;
float4 _MoaGeomagnetic2;

#define MOA_AURORA_STEPS 24
static const float k_MoaEarthRadius = 6371.0;
static const float k_MoaShellBottom = 95.0;
static const float k_MoaShellTop = 320.0;

// The distance along a ray from the observer to the height `height` (km). `sinAltitude` is the sine of the
// altitude of the ray.
float MoaShellDistance(float sinAltitude, float height)
{
    float b = k_MoaEarthRadius * sinAltitude;
    return -b + sqrt(b * b + 2.0 * k_MoaEarthRadius * height + height * height);
}

// The absolute geomagnetic latitude (degrees) and the geomagnetic longitude (radians) of a direction from the
// centre of the Earth.
float2 MoaGeomagnetic(float3 fromCentre)
{
    float3 n = normalize(fromCentre);
    float3 m = float3(dot(_MoaGeomagnetic0.xyz, n), dot(_MoaGeomagnetic1.xyz, n), dot(_MoaGeomagnetic2.xyz, n));
    return float2(abs(degrees(asin(clamp(m.z, -1.0, 1.0)))), atan2(m.y, m.x));
}

float MoaNoise1(float x)
{
    float i = floor(x);
    float f = frac(x);
    float a = frac(sin(i * 127.1) * 43758.5453);
    float b = frac(sin((i + 1.0) * 127.1) * 43758.5453);
    return lerp(a, b, f * f * (3.0 - 2.0 * f));
}

// The light of the curtains at one point: `latitude` in degrees, `longitude` in radians, `height` in km.
float3 MoaAuroraEmission(float latitude, float longitude, float height, float time)
{
    float edge = _MoaAuroraParams.y;
    float width = _MoaAuroraParams.z;
    float density = 0.0;
    [unroll]
    for (int k = 0; k < 3; ++k)
    {
        // Each curtain is a line along the band that folds slowly.
        float fold = 0.55 * sin(longitude * 9.0 + time * 0.05 + k * 2.1) + 0.3 * sin(longitude * 23.0 - time * 0.11 + k * 1.3) +
                     0.12 * sin(longitude * 61.0 + time * 0.23 + k * 4.7);
        float centre = edge + width * (0.1 + 0.28 * k) + fold;
        float distance = latitude - centre;
        // A sharp lower edge toward the equator, a softer upper edge toward the pole.
        float thickness = distance < 0.0 ? 0.08 : 0.22;
        float sheet = exp(-distance * distance / (thickness * thickness));
        // Rays along the curtain that flicker.
        float rays = MoaNoise1(longitude * 900.0 + k * 37.0 + time * 0.8) * 0.65 + MoaNoise1(longitude * 2600.0 - time * 1.7) * 0.35;
        float glow = 0.45 + 0.55 * sin(longitude * 5.0 + time * 0.07 + k);
        density += sheet * (0.25 + rays * rays * 1.5) * saturate(glow) * (1.0 - 0.25 * k);
    }
    float green = smoothstep(95.0, 108.0, height) * exp(-max(height - 108.0, 0.0) / 50.0);
    float red = smoothstep(160.0, 230.0, height) * exp(-max(height - 230.0, 0.0) / 70.0) * 0.45;
    float purple = exp(-(height - 99.0) * (height - 99.0) / 25.0) * 0.35;
    float3 natural = green * float3(0.2, 2.0, 0.7) + red * float3(1.0, 0.12, 0.22) + purple * float3(0.55, 0.12, 0.9);
    return density * MoaThemeAuroraColour(natural, longitude, height, time);
}

float3 MoaAurora(float3 direction, float2 pixel)
{
    if (_MoaAuroraParams.x <= 0.0 || MoaAuroraLight() <= 0.0 || direction.y <= 0.0)
    {
        return 0;
    }
    float near = MoaShellDistance(direction.y, k_MoaShellBottom);
    float far = MoaShellDistance(direction.y, k_MoaShellTop);
    const float3 centre = float3(0.0, k_MoaEarthRadius, 0.0);
    // Most rays do not go near the band: stop early.
    float a = MoaGeomagnetic(direction * near + centre).x;
    float b = MoaGeomagnetic(direction * far + centre).x;
    float c = MoaGeomagnetic(direction * (0.5 * (near + far)) + centre).x;
    float lowest = _MoaAuroraParams.y - 2.0;
    float highest = _MoaAuroraParams.y + _MoaAuroraParams.z + 2.0;
    if (max(max(a, b), c) < lowest || min(min(a, b), c) > highest)
    {
        return 0;
    }
    // A different start in each pixel hides the steps.
    float jitter = frac(52.9829189 * frac(dot(pixel, float2(0.06711056, 0.00583715))));
    float stride = (far - near) / MOA_AURORA_STEPS;
    float time = _Time.y;
    float3 light = 0;
    for (int i = 0; i < MOA_AURORA_STEPS; ++i)
    {
        float t = near + (i + jitter) * stride;
        float3 p = direction * t + centre;
        float height = length(p) - k_MoaEarthRadius;
        float2 place = MoaGeomagnetic(p);
        light += MoaAuroraEmission(place.x, place.y, height, time);
    }
    // The haze near the horizon dims the aurora.
    float haze = exp(-0.15 * (1.0 / max(direction.y, 0.03) - 1.0));
    return light * (stride * 0.012 * _MoaAuroraParams.x * haze);
}

#endif
