// Builds the AssetBundle of the sky mod in batch mode:
// Unity.exe -batchmode -quit -projectPath <this folder> -executeMethod BuildBundles.Build -logFile <log>
using System;
using System.Collections.Generic;
using System.IO;
using UnityEditor;
using UnityEngine;
using UnityEngine.Rendering;
using UnityEngine.Rendering.Universal;

public static class BuildBundles
{
    const string k_BundleName = "moa_sky";
    const string k_PipelinePath = "Assets/Settings/MoaSkyPipeline.asset";
    const string k_RendererPath = "Assets/Settings/MoaSkyRenderer.asset";
    const string k_OutputFolder = "Bundles/StandaloneWindows64";
    // The mod embeds the bundle from this file.
    const string k_ModCopy = "../../data/moa_sky.bundle";

    // A material of the bundle: the mod loads it by its name. The textures go into the material.
    struct MaterialSpec
    {
        public string name;
        public string shaderPath;
        public string[] textureProperties;
        public Func<Texture>[] textures;
        // True for a texture that the build makes. It is not in git, so the build takes it out of the material again.
        public bool[] made;
    }

    static readonly MaterialSpec[] k_Materials =
    {
        new MaterialSpec { name = "MoaSkyTest", shaderPath = "Assets/Shaders/MoaSkyTest.shader" },
        new MaterialSpec
        {
            name = "MoaSkybox",
            shaderPath = "Assets/Shaders/MoaSkybox.shader",
            textureProperties = new[] { "_MoaStarIndex" },
            textures = new Func<Texture>[] { StarIndexBaker.Bake },
            made = new[] { true },
        },
        new MaterialSpec
        {
            name = "MoaMoon",
            shaderPath = "Assets/Shaders/MoaMoon.shader",
            textureProperties = new[] { "_MainTex" },
            textures = new Func<Texture>[] { PrepareMoonMap },
            made = new[] { false },
        },
    };

    public static void Build()
    {
        try
        {
            PrepareProject();
            var assets = new List<string>();
            foreach (var spec in k_Materials)
            {
                assets.Add(spec.shaderPath);
                var material = PrepareMaterial(spec);
                assets.Add(AssetDatabase.GetAssetPath(material));
                for (int i = 0; spec.textureProperties != null && i < spec.textureProperties.Length; ++i)
                {
                    var texture = spec.textures[i]();
                    if (texture == null)
                    {
                        throw new InvalidOperationException("No texture for " + spec.name + "." + spec.textureProperties[i]);
                    }
                    material.SetTexture(spec.textureProperties[i], texture);
                    assets.Add(AssetDatabase.GetAssetPath(texture));
                }
                EditorUtility.SetDirty(material);
                AssetDatabase.SaveAssets();
            }
            BuildBundle(assets.ToArray());
            ClearMadeTextures();
            EditorApplication.Exit(0);
        }
        catch (Exception exception)
        {
            Debug.LogError("BuildBundles: " + exception);
            EditorApplication.Exit(1);
        }
    }

    // The game uses URP with Direct3D 11 or Direct3D 12. The shader programs in the bundle are for the
    // graphics APIs of the player settings.
    static void PrepareProject()
    {
        PlayerSettings.SetUseDefaultGraphicsAPIs(BuildTarget.StandaloneWindows64, false);
        PlayerSettings.SetGraphicsAPIs(BuildTarget.StandaloneWindows64,
            new[] { GraphicsDeviceType.Direct3D11, GraphicsDeviceType.Direct3D12, GraphicsDeviceType.Vulkan });
        PlayerSettings.colorSpace = ColorSpace.Linear;

        Directory.CreateDirectory("Assets/Settings");
        var pipeline = AssetDatabase.LoadAssetAtPath<UniversalRenderPipelineAsset>(k_PipelinePath);
        if (pipeline == null)
        {
            var renderer = ScriptableObject.CreateInstance<UniversalRendererData>();
            AssetDatabase.CreateAsset(renderer, k_RendererPath);
            pipeline = UniversalRenderPipelineAsset.Create(renderer);
            AssetDatabase.CreateAsset(pipeline, k_PipelinePath);
        }
        GraphicsSettings.defaultRenderPipeline = pipeline;
        QualitySettings.renderPipeline = pipeline;
        AssetDatabase.SaveAssets();
    }

    static Material PrepareMaterial(MaterialSpec spec)
    {
        var shader = AssetDatabase.LoadAssetAtPath<Shader>(spec.shaderPath);
        if (shader == null)
        {
            throw new InvalidOperationException("The shader " + spec.shaderPath + " is missing");
        }
        if (ShaderUtil.ShaderHasError(shader))
        {
            foreach (var message in ShaderUtil.GetShaderMessages(shader))
            {
                Debug.LogError("BuildBundles: " + spec.shaderPath + ":" + message.line + ": " + message.message);
            }
            throw new InvalidOperationException("The shader " + spec.shaderPath + " has errors");
        }
        Directory.CreateDirectory("Assets/Materials");
        var path = "Assets/Materials/" + spec.name + ".mat";
        var material = AssetDatabase.LoadAssetAtPath<Material>(path);
        if (material == null)
        {
            material = new Material(shader) { name = spec.name };
            AssetDatabase.CreateAsset(material, path);
        }
        material.shader = shader;
        EditorUtility.SetDirty(material);
        AssetDatabase.SaveAssets();
        return material;
    }

    // The made textures are not in git. Take them out of the materials again, so that the materials in git stay
    // the same after each build.
    static void ClearMadeTextures()
    {
        foreach (var spec in k_Materials)
        {
            if (spec.textureProperties == null)
            {
                continue;
            }
            var material = AssetDatabase.LoadAssetAtPath<Material>("Assets/Materials/" + spec.name + ".mat");
            for (int i = 0; i < spec.textureProperties.Length; ++i)
            {
                if (spec.made[i])
                {
                    material.SetTexture(spec.textureProperties[i], null);
                }
            }
            EditorUtility.SetDirty(material);
        }
        AssetDatabase.SaveAssets();
    }

    // The colour map of the moon: NASA SVS CGI Moon Kit (LRO LROC WAC), 1024 x 512, longitude 0 in the middle.
    static Texture PrepareMoonMap()
    {
        const string path = "Assets/Textures/MoonAlbedo.jpg";
        var importer = (TextureImporter)AssetImporter.GetAtPath(path);
        if (importer == null)
        {
            throw new InvalidOperationException("The moon map " + path + " is missing");
        }
        importer.textureType = TextureImporterType.Default;
        importer.sRGBTexture = true;
        importer.mipmapEnabled = true;
        importer.wrapModeU = TextureWrapMode.Repeat;
        importer.wrapModeV = TextureWrapMode.Clamp;
        importer.filterMode = FilterMode.Trilinear;
        importer.anisoLevel = 4;
        importer.maxTextureSize = 1024;
        importer.npotScale = TextureImporterNPOTScale.None;
        importer.textureCompression = TextureImporterCompression.Compressed;
        importer.SaveAndReimport();
        return AssetDatabase.LoadAssetAtPath<Texture2D>(path);
    }

    static void BuildBundle(string[] assets)
    {
        Directory.CreateDirectory(k_OutputFolder);
        var build = new AssetBundleBuild
        {
            assetBundleName = k_BundleName,
            assetNames = assets,
        };
        var manifest = BuildPipeline.BuildAssetBundles(k_OutputFolder, new[] { build },
            BuildAssetBundleOptions.ChunkBasedCompression | BuildAssetBundleOptions.StrictMode,
            BuildTarget.StandaloneWindows64);
        if (manifest == null)
        {
            throw new InvalidOperationException("BuildAssetBundles failed");
        }
        var output = Path.Combine(k_OutputFolder, k_BundleName);
        Directory.CreateDirectory(Path.GetDirectoryName(k_ModCopy));
        File.Copy(output, k_ModCopy, true);
        Debug.Log("BuildBundles: wrote " + output + " (" + new FileInfo(output).Length + " bytes) and " + k_ModCopy);
    }
}
