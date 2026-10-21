// Renders previews of the shaders of the sky mod to PNG files, with made cubemaps in the place of the cubemaps of the
// game. It is a tool for the look of the shaders only; the in-game check is the true test.
// Unity.exe -batchmode -quit -projectPath <this folder> -executeMethod SkyPreview.Render -logFile <log>
// (no -nographics: the preview needs the graphics card). The files go to the folder in the environment variable
// MOA_SKY_PREVIEW, or to Bundles/Preview.
using System;
using System.IO;
using UnityEditor;
using UnityEngine;
using UnityEngine.Rendering;

public static class SkyPreview
{
    const int k_Width = 1280;
    const int k_Height = 720;
    // London: the default place of the moon settings.
    const double k_Latitude = 51.5;
    const double k_Longitude = -0.13;

    struct View
    {
        public string name;
        public float pitch;
        public float heading;
        public float fieldOfView;
        public float blend;
        public float blend2;
        public bool stars;
        public float aurora;
        public double siderealTime;
        public float moonElongation;
        // The alignment theme: off keeps the painted night cubemap; the theme goes from -1 (evil) to +1 (good).
        public bool themeOn;
        public float theme;
    }

    // The light of the aurora for a theme, as AuroraLightForTheme of the mod: none below 0.1, then 10 % to 60 %.
    static float AuroraLight(float theme)
    {
        if (theme < 0.1f)
        {
            return 0.0f;
        }
        return 0.1f + 0.5f * (Mathf.Min(theme, 1.0f) - 0.1f) / 0.9f;
    }

    static void AddThemeViews(System.Collections.Generic.List<View> views, string name, float theme)
    {
        views.Add(new View { name = name + "-night-north", pitch = 22, heading = 0, fieldOfView = 70, blend = 0, blend2 = 1, stars = true, aurora = 1, siderealTime = 300, moonElongation = 70, themeOn = true, theme = theme });
        views.Add(new View { name = name + "-night-south", pitch = 25, heading = 180, fieldOfView = 60, blend = 0, blend2 = 1, stars = true, aurora = 1, siderealTime = 300, moonElongation = 70, themeOn = true, theme = theme });
        views.Add(new View { name = name + "-twilight", pitch = 15, heading = 180, fieldOfView = 60, blend = 1, blend2 = 0.45f, stars = true, aurora = 1, siderealTime = 300, moonElongation = 70, themeOn = true, theme = theme });
        views.Add(new View { name = name + "-sunset", pitch = 15, heading = 180, fieldOfView = 60, blend = 1, blend2 = 0.1f, stars = true, aurora = 1, siderealTime = 300, moonElongation = 70, themeOn = true, theme = theme });
        views.Add(new View { name = name + "-moon", pitch = 30, heading = 150, fieldOfView = 8, blend = 0, blend2 = 1, stars = true, siderealTime = 300, moonElongation = 140, themeOn = true, theme = theme });
    }

    public static void Render()
    {
        try
        {
            var folder = Environment.GetEnvironmentVariable("MOA_SKY_PREVIEW");
            if (string.IsNullOrEmpty(folder))
            {
                folder = "Bundles/Preview";
            }
            Directory.CreateDirectory(folder);
            // A copy, so that the material in the project does not change.
            var skybox = new Material(AssetDatabase.LoadAssetAtPath<Material>("Assets/Materials/MoaSkybox.mat"));
            skybox.SetTexture("_MoaStarIndex", StarIndexBaker.Bake());
            skybox.SetTexture("_Tex", MakeCubemap(Day));
            skybox.SetTexture("_OverlayTex", MakeCubemap(Sunrise));
            skybox.SetTexture("_OverlayTex2", MakeCubemap(Night));
            RenderSettings.skybox = skybox;
            var moon = AssetDatabase.LoadAssetAtPath<Material>("Assets/Materials/MoaMoon.mat");

            var themed = new System.Collections.Generic.List<View>();
            AddThemeViews(themed, "neutral", 0.0f);
            AddThemeViews(themed, "good-0.1", 0.1f);
            AddThemeViews(themed, "good-0.5", 0.5f);
            AddThemeViews(themed, "good-1.0", 1.0f);
            AddThemeViews(themed, "evil-0.5", -0.5f);
            AddThemeViews(themed, "evil-1.0", -1.0f);
            var painted = new[]
            {
                new View { name = "night-south", pitch = 25, heading = 180, fieldOfView = 60, blend = 0, blend2 = 1, stars = true, siderealTime = 300, moonElongation = 70 },
                new View { name = "night-north", pitch = 30, heading = 0, fieldOfView = 60, blend = 0, blend2 = 1, stars = true, siderealTime = 300, moonElongation = 70 },
                new View { name = "night-zenith", pitch = 80, heading = 180, fieldOfView = 70, blend = 0, blend2 = 1, stars = true, siderealTime = 300, moonElongation = 70 },
                new View { name = "aurora-north", pitch = 20, heading = 0, fieldOfView = 70, blend = 0, blend2 = 1, stars = true, aurora = 1, siderealTime = 300, moonElongation = 70 },
                new View { name = "aurora-up", pitch = 55, heading = 0, fieldOfView = 70, blend = 0, blend2 = 1, stars = true, aurora = 1, siderealTime = 300, moonElongation = 70 },
                new View { name = "twilight", pitch = 25, heading = 180, fieldOfView = 60, blend = 1, blend2 = 0.45f, stars = true, siderealTime = 300, moonElongation = 70 },
                new View { name = "moon-crescent", pitch = 30, heading = 150, fieldOfView = 8, blend = 0, blend2 = 1, stars = true, siderealTime = 300, moonElongation = 40 },
                new View { name = "moon-quarter", pitch = 30, heading = 150, fieldOfView = 8, blend = 0, blend2 = 1, stars = true, siderealTime = 300, moonElongation = 90 },
                new View { name = "moon-gibbous", pitch = 30, heading = 150, fieldOfView = 8, blend = 0, blend2 = 1, stars = true, siderealTime = 300, moonElongation = 140 },
                new View { name = "moon-full", pitch = 30, heading = 150, fieldOfView = 8, blend = 0, blend2 = 1, stars = true, siderealTime = 300, moonElongation = 178 },
                new View { name = "effects-off-night", pitch = 25, heading = 180, fieldOfView = 60, blend = 0, blend2 = 1, stars = false, siderealTime = 300, moonElongation = 70 },
            };
            themed.AddRange(painted);
            // MOA_SKY_PREVIEW_ONLY=evil draws only the views of the evil night.
            bool evilOnly = Environment.GetEnvironmentVariable("MOA_SKY_PREVIEW_ONLY") == "evil";
            var views = evilOnly ? new View[0] : themed.ToArray();

            var cameraObject = new GameObject("PreviewCamera");
            var camera = cameraObject.AddComponent<Camera>();
            camera.clearFlags = CameraClearFlags.Skybox;
            camera.nearClipPlane = 1.0f;
            camera.farClipPlane = 10000.0f;
            var target = new RenderTexture(k_Width, k_Height, 24, RenderTextureFormat.ARGBHalf);
            camera.targetTexture = target;

            var quad = new GameObject("MoonQuad");
            quad.AddComponent<MeshFilter>().sharedMesh = MakeMoonQuad();
            quad.AddComponent<MeshRenderer>().sharedMaterial = moon;

            foreach (var view in views)
            {
                skybox.SetFloat("_Blend", view.blend);
                skybox.SetFloat("_Blend2", view.blend2);
                skybox.SetFloat("_Rotation", 0);
                SetGlobals(view);
                cameraObject.transform.rotation = Quaternion.Euler(-view.pitch, view.heading, 0);
                camera.fieldOfView = view.fieldOfView;
                PlaceMoon(quad.transform);
                camera.Render();
                Save(target, Path.Combine(folder, "sky-preview-" + view.name + ".png"));
            }
            int evilCount = RenderEvil(folder, skybox, cameraObject, camera, target, quad);
            Debug.Log("SkyPreview: wrote " + (views.Length + evilCount) + " files to " + folder);
            EditorApplication.Exit(0);
        }
        catch (Exception exception)
        {
            Debug.LogError("SkyPreview: " + exception);
            EditorApplication.Exit(1);
        }
    }

    // The moon of the preview: altitude 30 degrees, azimuth 150 degrees (north on +z, east on +x).
    static Vector3 MoonDirection()
    {
        return Local(30, 150);
    }

    static Vector3 Local(double altitude, double azimuth)
    {
        double a = altitude * Math.PI / 180.0;
        double z = azimuth * Math.PI / 180.0;
        return new Vector3((float)(Math.Cos(a) * Math.Sin(z)), (float)Math.Sin(a), (float)(Math.Cos(a) * Math.Cos(z)));
    }

    static void PlaceMoon(Transform quad)
    {
        var direction = MoonDirection();
        quad.position = direction * 3000.0f;
        quad.rotation = Quaternion.LookRotation(-direction, Vector3.up);
        quad.localScale = Vector3.one * 60.0f;
    }

    // A quad with its front face on local +z, the right of the texture on local -x and the top on local +y, as the
    // moon quad of the game.
    static Mesh MakeMoonQuad()
    {
        var mesh = new Mesh();
        mesh.vertices = new[] { new Vector3(0.5f, -0.5f, 0), new Vector3(-0.5f, -0.5f, 0), new Vector3(-0.5f, 0.5f, 0), new Vector3(0.5f, 0.5f, 0) };
        // The texture coordinates of one frame of the strip of eight phases, as the mesh of the game.
        mesh.uv = new[] { new Vector2(0, 0), new Vector2(0.125f, 0), new Vector2(0.125f, 1), new Vector2(0, 1) };
        mesh.triangles = new[] { 0, 1, 2, 0, 2, 3 };
        mesh.RecalculateBounds();
        return mesh;
    }

    static void SetGlobals(View view)
    {
        Shader.SetGlobalVector("_MoaEvilMoon", Vector4.zero);
        Shader.SetGlobalVector("_MoaSkyParams", new Vector4(1, view.stars ? 4 : 0, view.stars ? 0 : 1, view.stars ? 0.5f : 0));
        Shader.SetGlobalVector("_MoaStarParams", new Vector4(view.stars ? 1 : 0, 6.5f, 0.03f, 0.08f));
        // The painted views have no theme: the aurora shows at its first light, as the mode "always".
        float auroraLight = view.themeOn ? AuroraLight(view.theme) : 1.0f;
        Shader.SetGlobalVector("_MoaThemeParams", new Vector4(view.themeOn ? 1 : 0, view.themeOn ? view.theme : 0, auroraLight, 0));
        Shader.SetGlobalVector("_MoaThemeStyle", new Vector4(1, 1, 1, 0));
        if (!view.stars && view.aurora == 0 && !view.themeOn)
        {
            Shader.SetGlobalVector("_MoaSkyParams", Vector4.zero);
        }
        // World to local: north +z, east +x, up +y. Local to equatorial by the sidereal time and the latitude.
        var worldToLocal = new double[3, 3] { { 0, 0, 1 }, { 1, 0, 0 }, { 0, 1, 0 } };
        var equatorialToLocal = EquatorialToLocal(view.siderealTime, k_Latitude);
        var worldToCelestial = Multiply(Transpose(equatorialToLocal), worldToLocal);
        SetRows("_MoaCelestial", worldToCelestial);

        // The geomagnetic axes, and the band of the aurora a little to the north (the mode "always").
        var pole = Unit(80.8, -72.7);
        var earthNorth = new double[] { 0, 0, 1 };
        var x = Normalize(Subtract(earthNorth, Scale(pole, Dot(earthNorth, pole))));
        var y = Cross(pole, x);
        var earthToMag = new double[3, 3] { { x[0], x[1], x[2] }, { y[0], y[1], y[2] }, { pole[0], pole[1], pole[2] } };
        var localToEarth = Transpose(EquatorialToLocal(k_Longitude, k_Latitude));
        var worldToMag = Multiply(Multiply(earthToMag, localToEarth), worldToLocal);
        SetRows("_MoaGeomagnetic", worldToMag);
        var here = Unit(k_Latitude, k_Longitude);
        double magneticLatitude = Math.Asin(Dot(here, pole)) * 180.0 / Math.PI;
        Shader.SetGlobalVector("_MoaAuroraParams", new Vector4(view.aurora, (float)magneticLatitude + 1.5f, 6, 3));

        // The sun: at the elongation of the moon from the moon, below the horizon.
        var moon = MoonDirection();
        var below = Vector3.Normalize(Vector3.ProjectOnPlane(Vector3.down + Vector3.right * 0.3f, moon));
        double e = view.moonElongation * Math.PI / 180.0;
        var sun = (moon * (float)Math.Cos(e) + below * (float)Math.Sin(e)).normalized;
        double phaseAngle = Math.PI - e;
        float earthshine = (float)(0.02 * (1 - Math.Cos(phaseAngle)) / 2);
        float lit = (float)((1 + Math.Cos(phaseAngle)) / 2);
        Shader.SetGlobalVector("_MoaSunWorld", new Vector4(sun.x, sun.y, sun.z, earthshine));
        Shader.SetGlobalVector("_MoaMoonWorld", new Vector4(moon.x, moon.y, moon.z, 1));
        Shader.SetGlobalVector("_MoaMoonParams", new Vector4(lit, 0.33f, 0.15f, 1));
    }

    static double[,] EquatorialToLocal(double siderealTime, double latitude)
    {
        double t = siderealTime * Math.PI / 180.0;
        double f = latitude * Math.PI / 180.0;
        return new double[3, 3]
        {
            { -Math.Sin(f) * Math.Cos(t), -Math.Sin(f) * Math.Sin(t), Math.Cos(f) },
            { -Math.Sin(t), Math.Cos(t), 0 },
            { Math.Cos(f) * Math.Cos(t), Math.Cos(f) * Math.Sin(t), Math.Sin(f) },
        };
    }

    static void SetRows(string prefix, double[,] m)
    {
        for (int row = 0; row < 3; ++row)
        {
            Shader.SetGlobalVector(prefix + row, new Vector4((float)m[row, 0], (float)m[row, 1], (float)m[row, 2], 0));
        }
    }

    static double[] Unit(double latitude, double longitude)
    {
        double f = latitude * Math.PI / 180.0;
        double l = longitude * Math.PI / 180.0;
        return new[] { Math.Cos(f) * Math.Cos(l), Math.Cos(f) * Math.Sin(l), Math.Sin(f) };
    }

    static double Dot(double[] a, double[] b) => a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
    static double[] Scale(double[] a, double s) => new[] { a[0] * s, a[1] * s, a[2] * s };
    static double[] Subtract(double[] a, double[] b) => new[] { a[0] - b[0], a[1] - b[1], a[2] - b[2] };
    static double[] Normalize(double[] a) => Scale(a, 1.0 / Math.Sqrt(Dot(a, a)));
    static double[] Cross(double[] a, double[] b) =>
        new[] { a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0] };

    static double[,] Transpose(double[,] m)
    {
        var r = new double[3, 3];
        for (int i = 0; i < 3; ++i)
            for (int j = 0; j < 3; ++j)
                r[i, j] = m[j, i];
        return r;
    }

    static double[,] Multiply(double[,] a, double[,] b)
    {
        var r = new double[3, 3];
        for (int i = 0; i < 3; ++i)
            for (int j = 0; j < 3; ++j)
                for (int k = 0; k < 3; ++k)
                    r[i, j] += a[i, k] * b[k, j];
        return r;
    }

    static Color Day(Vector3 d)
    {
        return d.y < 0 ? new Color(0.25f, 0.22f, 0.25f, 1) : Color.Lerp(new Color(0.55f, 0.65f, 0.8f, 1), new Color(0.2f, 0.35f, 0.7f, 1), d.y);
    }

    static Color Sunrise(Vector3 d)
    {
        return d.y < 0 ? new Color(0.12f, 0.05f, 0.04f, 1) : Color.Lerp(new Color(0.9f, 0.45f, 0.3f, 1), new Color(0.25f, 0.2f, 0.45f, 1), d.y);
    }

    // A dark blue night with a ring of grey clouds over the horizon, as in the night cubemap of the game.
    static Color Night(Vector3 d)
    {
        if (d.y < 0)
        {
            return new Color(0.012f, 0.016f, 0.024f, 1);
        }
        float azimuth = Mathf.Atan2(d.x, d.z);
        float clouds = Mathf.Clamp01(1.0f - d.y / 0.3f) * Mathf.Clamp01(0.5f + 0.5f * Mathf.Sin(azimuth * 5.0f) + 0.3f * Mathf.Sin(azimuth * 13.0f + 1.0f));
        var sky = new Color(0.008f, 0.012f, 0.026f, 1);
        return Color.Lerp(sky, new Color(0.11f, 0.12f, 0.14f, 1), clouds);
    }

    static Cubemap MakeCubemap(Func<Vector3, Color> colour)
    {
        const int size = 128;
        var cubemap = new Cubemap(size, TextureFormat.RGBAHalf, true);
        foreach (CubemapFace face in Enum.GetValues(typeof(CubemapFace)))
        {
            if (face == CubemapFace.Unknown)
            {
                continue;
            }
            var pixels = new Color[size * size];
            for (int y = 0; y < size; ++y)
            {
                for (int x = 0; x < size; ++x)
                {
                    float u = (x + 0.5f) / size * 2 - 1;
                    float v = (y + 0.5f) / size * 2 - 1;
                    pixels[y * size + x] = colour(FaceDirection(face, u, -v));
                }
            }
            cubemap.SetPixels(pixels, face);
        }
        cubemap.Apply(true);
        return cubemap;
    }

    static Vector3 FaceDirection(CubemapFace face, float u, float v)
    {
        switch (face)
        {
        case CubemapFace.PositiveX: return new Vector3(1, v, -u).normalized;
        case CubemapFace.NegativeX: return new Vector3(-1, v, u).normalized;
        case CubemapFace.PositiveY: return new Vector3(u, 1, -v).normalized;
        case CubemapFace.NegativeY: return new Vector3(u, -1, v).normalized;
        case CubemapFace.PositiveZ: return new Vector3(u, v, 1).normalized;
        default: return new Vector3(-u, v, -1).normalized;
        }
    }

    static void Save(RenderTexture target, string path)
    {
        var previous = RenderTexture.active;
        RenderTexture.active = target;
        var image = new Texture2D(k_Width, k_Height, TextureFormat.RGBAFloat, false, true);
        image.ReadPixels(new Rect(0, 0, k_Width, k_Height), 0, 0);
        image.Apply();
        RenderTexture.active = previous;
        // Linear light to sRGB, with no tone mapping, as the game.
        var pixels = image.GetPixels();
        for (int i = 0; i < pixels.Length; ++i)
        {
            pixels[i] = new Color(Mathf.LinearToGammaSpace(Mathf.Clamp01(pixels[i].r)), Mathf.LinearToGammaSpace(Mathf.Clamp01(pixels[i].g)),
                Mathf.LinearToGammaSpace(Mathf.Clamp01(pixels[i].b)), 1);
        }
        var output = new Texture2D(k_Width, k_Height, TextureFormat.RGB24, false);
        output.SetPixels(pixels);
        output.Apply();
        File.WriteAllBytes(path, output.EncodeToPNG());
    }

    // The views of the evil night: the blood moon. Each view sets the value that the mod sets for one evil value.
    static int RenderEvil(string folder, Material skybox, GameObject cameraObject, Camera camera, RenderTexture target,
        GameObject moon)
    {
        int count = 0;
        Action<string, View, Action> shot = (name, view, values) =>
        {
            skybox.SetFloat("_Blend", view.blend);
            skybox.SetFloat("_Blend2", view.blend2);
            skybox.SetFloat("_Rotation", 0);
            SetGlobals(view);
            values();
            cameraObject.transform.rotation = Quaternion.Euler(-view.pitch, view.heading, 0);
            camera.fieldOfView = view.fieldOfView;
            PlaceMoon(moon.transform);
            camera.Render();
            Save(target, Path.Combine(folder, "evil-preview-" + name + ".png"));
            ++count;
        };
        Func<float, View> night = theme => new View
        {
            pitch = 25, heading = 180, fieldOfView = 60, blend = 0, blend2 = 1, stars = true, siderealTime = 300,
            moonElongation = 140, themeOn = true, theme = theme,
        };
        Action none = () => { };

        shot("neutral-night", night(0), none);
        shot("evil-1.0-night", night(-1), none);

        // The blood moon at the evil values 0.5 and 1 (smoothstep 0.5 is 0.5).
        Func<float, View> moonView = theme => new View
        {
            pitch = 30, heading = 150, fieldOfView = 8, blend = 0, blend2 = 1, stars = true, siderealTime = 300,
            moonElongation = 140, themeOn = true, theme = theme,
        };
        shot("moon-neutral", moonView(0), none);
        shot("moon-blood-0.5", moonView(-0.5f), () => Shader.SetGlobalVector("_MoaEvilMoon", new Vector4(0.5f, 1.15f, 0.3f, 0)));
        shot("moon-blood-1.0", moonView(-1), () => Shader.SetGlobalVector("_MoaEvilMoon", new Vector4(1, 1.3f, 0.6f, 0)));
        shot("moon-blood-1.0-crescent", new View { pitch = 30, heading = 150, fieldOfView = 8, blend = 0, blend2 = 1, stars = true, siderealTime = 300, moonElongation = 50, themeOn = true, theme = -1 },
            () => Shader.SetGlobalVector("_MoaEvilMoon", new Vector4(1, 1.3f, 0.6f, 0)));

        // Sunset: the mod sets no evil value before the night weight starts (the night blend below 0.5).
        shot("sunset-evil-1.0", new View { pitch = 15, heading = 180, fieldOfView = 60, blend = 1, blend2 = 0.1f, stars = true, siderealTime = 300, moonElongation = 70, themeOn = true, theme = -1 }, none);
        return count;
    }
}
