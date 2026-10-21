// The skybox of the sky mod. It replaces the skybox material of the game (shader Skybox/Marcos_Sky_Box_Blender).
// It has the same property names, so the sky script of the game writes its blend values into this material.
//
// With all the effects of the mod off, the result is the same as the shader of the game:
//   the vertex turns about the up axis by _Rotation degrees, the cubemap direction is the vertex before the turn,
//   colour = lerp(lerp(_Tex, _OverlayTex, _Blend), _OverlayTex2, _Blend2) (all four channels),
//   rgb = colour.rgb * _Tex_HDR.x * pow(_Tex_HDR.w * (colour.a - 1) + 1, _Tex_HDR.y), alpha = 1.
//
// The mod sets the global values _Moa* in each frame. Their default value is zero, which turns each effect off.
// With the alignment theme on, the night sky of the mod (MoaTheme.hlsl) replaces the night cubemap in the blend; the
// day and the sunrise cubemaps and the blend values stay as in the game.
Shader "MoA/Sky/Skybox"
{
    Properties
    {
        _Blend ("Sunset blend", Range(0, 1)) = 1
        _Blend2 ("Night blend", Range(0, 1)) = 0
        _Rotation ("Rotation", Range(0, 360)) = 180
        [NoScaleOffset] _Tex ("Day", Cube) = "grey" {}
        [NoScaleOffset] _OverlayTex ("Sunrise", Cube) = "grey" {}
        [NoScaleOffset] _OverlayTex2 ("Night", Cube) = "grey" {}
        [NoScaleOffset] _MoaStarIndex ("Star cells", 2D) = "black" {}
    }

    SubShader
    {
        Tags { "Queue" = "Background" "RenderType" = "Background" "PreviewType" = "Skybox" }
        Cull Off
        ZWrite Off

        Pass
        {
            Name "MoaSkybox"

            HLSLPROGRAM
            #pragma target 4.5
            #pragma vertex Vertex
            #pragma fragment Fragment

            #include "Packages/com.unity.render-pipelines.universal/ShaderLibrary/Core.hlsl"
            #include "MoaSkyCommon.hlsl"

            // The values of the material. They are not in a UnityPerMaterial buffer, as in the shader of the game.
            float _Blend;
            float _Blend2;
            float _Rotation;
            float4 _Tex_HDR;
            TEXTURECUBE(_Tex);
            SAMPLER(sampler_Tex);
            TEXTURECUBE(_OverlayTex);
            SAMPLER(sampler_OverlayTex);
            TEXTURECUBE(_OverlayTex2);
            SAMPLER(sampler_OverlayTex2);

            struct Attributes
            {
                float4 positionOS : POSITION;
            };

            struct Varyings
            {
                float4 positionCS : SV_POSITION;
                float3 cubeDirection : TEXCOORD0;
                float3 worldDirection : TEXCOORD1;
            };

            Varyings Vertex(Attributes input)
            {
                // The same turn as the shader of the game: a turn of the vertex in the xz plane.
                float angle = _Rotation * 0.017453;
                float s;
                float c;
                sincos(angle, s, c);
                float3 turned = float3(c * input.positionOS.x - s * input.positionOS.z, input.positionOS.y,
                                       s * input.positionOS.x + c * input.positionOS.z);
                float3 positionWS = TransformObjectToWorld(turned);
                Varyings output;
                output.positionCS = TransformWorldToHClip(positionWS);
                output.cubeDirection = input.positionOS.xyz;
                output.worldDirection = positionWS - _WorldSpaceCameraPos;
                return output;
            }

            float4 Fragment(Varyings input) : SV_Target
            {
                float3 direction = input.cubeDirection;
                // A plain sample, as in the program of the game. The URP macros add the global mip bias.
                float4 day = _Tex.Sample(sampler_Tex, direction);
                float4 sunrise = _OverlayTex.Sample(sampler_OverlayTex, direction);
                float4 night;
                float4 nightSoft = 0;
                bool effects = MoaSkyEffectsOn();
                if (effects && MoaThemeNightOn())
                {
                    // The night sky of the mod replaces the painted night sky, with its clouds. The HDR decode below
                    // multiplies by _Tex_HDR.x, so the colour goes in before that scale.
                    night = float4(MoaNightBackground(normalize(input.worldDirection)) / max(_Tex_HDR.x, 1e-4), 1.0);
                }
                else if (effects)
                {
                    // A low mip of the night cubemap has no painted stars. It keeps the clouds and the glow.
                    nightSoft = _OverlayTex2.SampleLevel(sampler_OverlayTex2, direction, MoaNightMip());
                    night = MoaPaintedStarsOn() ? _OverlayTex2.Sample(sampler_OverlayTex2, direction) : nightSoft;
                }
                else
                {
                    night = _OverlayTex2.Sample(sampler_OverlayTex2, direction);
                }
                float4 colour = lerp(lerp(day, sunrise, _Blend), night, _Blend2);
                float alpha = _Tex_HDR.w * (colour.a - 1.0) + 1.0;
                // The program of the game does pow as log, multiply and exp: the same operations as these.
                float3 rgb = colour.rgb * (_Tex_HDR.x * exp2(log2(alpha) * _Tex_HDR.y));
                if (effects)
                {
                    rgb += MoaNightSky(normalize(input.worldDirection), nightSoft.rgb, _Blend, _Blend2, input.positionCS.xy);
                }
                return float4(rgb, 1.0);
            }
            ENDHLSL
        }
    }
    Fallback Off
}
