// The moon of the sky mod, on the moon quad of the game (Moon_Plane). It shows a lit ball: the colour map of the
// moon (NASA SVS CGI Moon Kit), lit by the real sun, seen from the real direction of the moon. The game sets _Color
// in each frame; the shader uses it for the brightness and a light tint. The disc covers the sky behind it
// (premultiplied alpha), so no star shows through the dark side.
//
// The mod sets the global values: _MoaSunWorld (the sun direction, w: earthshine), _MoaMoonWorld (the moon
// direction, w: brightness), _MoaMoonParams (x: lit part, y: disc radius on the quad, z: halo, w: on) and
// _MoaCelestial2 (the world direction of the pole of the sky).
Shader "MoA/Sky/Moon"
{
    Properties
    {
        [NoScaleOffset] _MainTex ("Moon map", 2D) = "grey" {}
        _Color ("Colour", Color) = (0.192, 0.853, 0.992, 0.435)
    }

    SubShader
    {
        Tags { "Queue" = "Transparent" "RenderType" = "Transparent" "IgnoreProjector" = "True" "PreviewType" = "Plane" }
        Blend One OneMinusSrcAlpha
        ZWrite Off
        Cull Off

        Pass
        {
            Name "MoaMoon"

            HLSLPROGRAM
            #pragma target 4.5
            #pragma vertex Vertex
            #pragma fragment Fragment

            #include "Packages/com.unity.render-pipelines.universal/ShaderLibrary/Core.hlsl"
            #include "MoaTheme.hlsl"

            TEXTURE2D(_MainTex);
            SAMPLER(sampler_MainTex);
            CBUFFER_START(UnityPerMaterial)
                float4 _Color;
            CBUFFER_END

            float4 _MoaSunWorld;
            float4 _MoaMoonWorld;
            float4 _MoaMoonParams;
            float4 _MoaCelestial2;
            // The blood moon of the evil night: x the part of the blood moon, y the scale of the disc (0 is 1), z the dark
            // rim.
            float4 _MoaEvilMoon;

            // The mean of the near side of the map is 0.22 (linear). With the night colour of the game, this gain
            // makes the moon about as bright as the moon of the game (the lit side about 0.3 to 0.7).
            static const float k_MapGain = 1.5;
            // The mesh of Moon_Plane has the texture coordinates of one frame of the strip of eight phases: u goes
            // from 0 to 1/8 across the quad.
            static const float k_FramesInStrip = 8.0;

            struct Attributes
            {
                float4 positionOS : POSITION;
                float2 uv : TEXCOORD0;
            };

            struct Varyings
            {
                float4 positionCS : SV_POSITION;
                float2 uv : TEXCOORD0;
            };

            // The test mode of the query sky.debug: _MoaMoonParams.z below zero.
            float DebugMode()
            {
                return _MoaMoonParams.z < -0.5 ? round(-_MoaMoonParams.z) : 0.0;
            }

            Varyings Vertex(Attributes input)
            {
                Varyings output;
                output.positionCS = TransformObjectToHClip(input.positionOS.xyz);
                output.uv = input.uv;
                float debugMode = DebugMode();
                // Test mode 4: a magenta rectangle at the centre of the screen, in front of all, with no transform.
                // It shows that Unity draws the renderer. Test mode 5: the quad at its place, in front of all. It
                // shows a fault of the depth test or of the far plane.
                float nearDepth = UNITY_NEAR_CLIP_VALUE * 0.99;
                if (debugMode == 4.0)
                {
                    float2 corner = float2(input.uv.x * k_FramesInStrip > 0.5 ? 1.0 : -1.0,
                                           input.uv.y > 0.5 ? 1.0 : -1.0);
                    output.positionCS = float4(corner * float2(0.2, 0.1), nearDepth, 1.0);
                }
                else if (debugMode == 5.0)
                {
                    output.positionCS.z = nearDepth * output.positionCS.w;
                }
                return output;
            }

            float4 Fragment(Varyings input) : SV_Target
            {
                // A test mode of the query sky.debug (z below zero): 1, 4 and 5 paint the whole quad magenta, 2
                // shows the texture coordinates of the quad, 3 shows the disc with an opaque quad.
                float debugMode = DebugMode();
                if (debugMode == 1.0 || debugMode >= 4.0)
                {
                    return float4(1.0, 0.0, 1.0, 1.0);
                }
                if (debugMode == 2.0)
                {
                    return float4(frac(input.uv.x * k_FramesInStrip), input.uv.y, 0.0, 1.0);
                }
                if (_MoaMoonParams.w < 0.5)
                {
                    return 0;
                }
                float2 quadUv = float2(input.uv.x * k_FramesInStrip, input.uv.y);
                float blood = saturate(_MoaEvilMoon.x);
                float discRadius = _MoaMoonParams.y * max(_MoaEvilMoon.y, 1.0);
                float2 d = (quadUv - 0.5) / discRadius;
                float r = length(d);
                float coverage = saturate((1.0 - r) / max(fwidth(r), 1e-4) + 0.5);

                // The axes of the disc: toward the viewer, and the right and the top of the quad. The texture of the
                // quad has its right on the local -x axis and its top on the local +y axis.
                float3 toViewer = -normalize(_MoaMoonWorld.xyz);
                float3 quadRight = -normalize(UNITY_MATRIX_M._m00_m10_m20);
                float3 quadUp = normalize(UNITY_MATRIX_M._m01_m11_m21);
                float3 up = normalize(quadUp - dot(quadUp, toViewer) * toViewer);
                float3 right = normalize(quadRight - dot(quadRight, toViewer) * toViewer - dot(quadRight, up) * up);

                float2 inside = d / max(r, 1.0);
                float z = sqrt(saturate(1.0 - dot(inside, inside)));
                float3 normal = inside.x * right + inside.y * up + z * toViewer;

                // The surface point in the axes of the moon: the near side faces the Earth, north up. Lunar east is
                // on the west side of the sky, which is at the right for a viewer with the north up.
                float3 pole = _MoaCelestial2.xyz;
                float3 north = normalize(pole - dot(pole, toViewer) * toViewer);
                float3 skyRight = cross(north, -toViewer);
                float longitude = atan2(dot(normal, skyRight), dot(normal, toViewer));
                float latitude = asin(clamp(dot(normal, north), -1.0, 1.0));
                float2 mapUv = float2(0.5 + longitude / (2.0 * PI), 0.5 + latitude / PI);
                float3 albedo = SAMPLE_TEXTURE2D_GRAD(_MainTex, sampler_MainTex, mapUv, ddx(quadUv) * 0.5,
                                                      ddy(quadUv) * 0.5).rgb;

                // Lommel-Seeliger: the full moon is almost flat, with no dark limb. The earthshine lights the dark
                // side a little, with a blue tint.
                float3 sun = normalize(_MoaSunWorld.xyz);
                float incidence = max(dot(normal, sun), 0.0);
                float lit = 2.0 * incidence / (incidence + z + 1e-4);
                float3 earthshine = _MoaSunWorld.w * float3(0.8, 0.9, 1.15) * z;
                float3 colour = albedo * k_MapGain * (lit + earthshine);

                // The moon shader of the game multiplies the texture by _Color and by its alpha, so the brightness
                // of the game moon is the largest channel of _Color times its alpha.
                float level = max(_Color.r, max(_Color.g, _Color.b));
                float3 tint = lerp(1.0, _Color.rgb / max(level, 1e-3), 0.15);
                float gain = level * _Color.a * _MoaMoonWorld.w;
                // In the evil sky the moon is a little red, as in an eclipse.
                float3 bloodTint = lerp(1.0, float3(1.0, 0.55, 0.42), 0.6 * saturate(MoaEvilWeight()));
                colour *= tint * gain * bloodTint;
                // The blood moon: a deep crimson with the same phase and light, darker at the rim.
                float brightness = dot(colour, float3(0.2126, 0.7152, 0.0722));
                float3 crimson = brightness * float3(1.8, 0.08, 0.04) * lerp(1.0, 1.0 - _MoaEvilMoon.z, pow(1.0 - z, 1.5));
                colour = lerp(colour, crimson, blood);

                // A soft halo outside the disc, from the lit part of the disc.
                float halo = max(_MoaMoonParams.z, 0.0) * _MoaMoonParams.x * exp(-(r - 1.0) * 8.0) * gain;
                // The halo goes to zero before the edge of the quad.
                halo *= 1.0 - smoothstep(max(0.38, discRadius + 0.02), 0.5, length(quadUv - 0.5));
                float3 haloColour = lerp(float3(0.75, 0.85, 1.0) * bloodTint, float3(0.9, 0.08, 0.03), blood) * halo * step(1.0, r);
                if (debugMode == 3.0)
                {
                    return float4(colour * coverage + float3(0.0, 0.2, 0.0) * (1.0 - coverage), 1.0);
                }
                return float4(colour * coverage + haloColour * (1.0 - coverage), coverage);
            }
            ENDHLSL
        }
    }
    Fallback Off
}
