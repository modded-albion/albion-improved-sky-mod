// A test shader for the sky mod. It shows a flat colour that is bright enough to see in the night sky.
Shader "MoA/Sky/Test"
{
    Properties
    {
        [HDR] _BaseColor ("Colour", Color) = (4, 0.2, 2, 1)
    }

    SubShader
    {
        Tags { "RenderPipeline" = "UniversalPipeline" "Queue" = "Transparent" "RenderType" = "Transparent" "IgnoreProjector" = "True" }

        Pass
        {
            Name "SkyTest"
            Blend One Zero
            ZWrite Off
            Cull Off

            HLSLPROGRAM
            #pragma vertex Vertex
            #pragma fragment Fragment

            #include "Packages/com.unity.render-pipelines.universal/ShaderLibrary/Core.hlsl"

            CBUFFER_START(UnityPerMaterial)
                half4 _BaseColor;
            CBUFFER_END

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

            Varyings Vertex(Attributes input)
            {
                Varyings output;
                output.positionCS = TransformObjectToHClip(input.positionOS.xyz);
                output.uv = input.uv;
                return output;
            }

            half4 Fragment(Varyings input) : SV_Target
            {
                // A check pattern makes the quad easy to find in a screenshot.
                float2 cell = floor(input.uv * 4.0);
                half check = fmod(cell.x + cell.y, 2.0) < 1.0 ? 1.0 : 0.35;
                return half4(_BaseColor.rgb * check, 1.0);
            }
            ENDHLSL
        }
    }
}
