// Makes the star index texture of the sky mod from the star table (Data/stars_bsc5.bin, see Data/make_stars.py).
//
// The sky is a cube of cells: 6 faces of 32 x 32 cells. A cell coordinate is atan(u) / (pi / 4) on the face, so the
// cells have almost the same size (2.8 degrees). Each cell has a list of the stars that are in the cell or less
// than k_MarginDegrees from it, brightest first. The shader (MoaStars.hlsl) finds the cell of a pixel and reads
// only that list. The two files must use the same cell rules.
//
// The texture is 256 texels wide, RGBA 32-bit float, no mips, point filter:
//   rows 0 to 23: one texel for each cell (face * 1024 + row * 32 + column): x = first entry, y = count of entries.
//   rows 24 and more: the entries: xyz = the J2000 direction of the star, w = the packed magnitude and colour:
//   floor((V + 2) * 100) + clamp((B-V + 0.5) / 2.5, 0, 1) * 0.99.
using System;
using System.Collections.Generic;
using System.IO;
using UnityEditor;
using UnityEngine;

public static class StarIndexBaker
{
    public const int k_CellsPerFace = 32;
    public const int k_Width = 256;
    public const int k_CellCount = 6 * k_CellsPerFace * k_CellsPerFace;
    public const int k_TableRows = k_CellCount / k_Width;
    const double k_MarginDegrees = 0.75;
    const float k_DefaultColourIndex = 0.5f;
    const string k_StarTable = "Data/stars_bsc5.bin";
    const string k_TexturePath = "Assets/Generated/MoaStarIndex.asset";

    struct Star
    {
        public Vector3 direction;
        public float magnitude;
        public float colourIndex;
        public float packed;
    }

    public static Texture2D Bake()
    {
        var stars = ReadStars(k_StarTable);
        var cells = new List<int>[k_CellCount];
        for (int i = 0; i < k_CellCount; ++i)
        {
            cells[i] = new List<int>();
        }
        // The stars are brightest first in the table, so each list is also brightest first.
        var touched = new HashSet<int>();
        for (int index = 0; index < stars.Count; ++index)
        {
            touched.Clear();
            foreach (var direction in Neighbourhood(stars[index].direction))
            {
                touched.Add(CellOf(direction));
            }
            foreach (var cell in touched)
            {
                cells[cell].Add(index);
            }
        }

        int entries = 0;
        int most = 0;
        foreach (var list in cells)
        {
            entries += list.Count;
            most = Math.Max(most, list.Count);
        }
        int entryRows = (entries + k_Width - 1) / k_Width;
        int height = k_TableRows + entryRows;
        var data = new float[k_Width * height * 4];
        int next = 0;
        for (int cell = 0; cell < k_CellCount; ++cell)
        {
            data[cell * 4 + 0] = next;
            data[cell * 4 + 1] = cells[cell].Count;
            foreach (var index in cells[cell])
            {
                int texel = k_TableRows * k_Width + next;
                var star = stars[index];
                data[texel * 4 + 0] = star.direction.x;
                data[texel * 4 + 1] = star.direction.y;
                data[texel * 4 + 2] = star.direction.z;
                data[texel * 4 + 3] = star.packed;
                ++next;
            }
        }

        var texture = new Texture2D(k_Width, height, TextureFormat.RGBAFloat, false, true)
        {
            name = "MoaStarIndex",
            filterMode = FilterMode.Point,
            wrapMode = TextureWrapMode.Clamp,
            anisoLevel = 0,
        };
        texture.SetPixelData(data, 0);
        texture.Apply(false, false);
        Directory.CreateDirectory(Path.GetDirectoryName(k_TexturePath));
        AssetDatabase.DeleteAsset(k_TexturePath);
        AssetDatabase.CreateAsset(texture, k_TexturePath);
        AssetDatabase.SaveAssets();
        Debug.Log("StarIndexBaker: " + stars.Count + " stars, " + entries + " entries, at most " + most +
                  " in one cell, texture " + k_Width + " x " + height);
        return AssetDatabase.LoadAssetAtPath<Texture2D>(k_TexturePath);
    }

    static List<Star> ReadStars(string path)
    {
        var bytes = File.ReadAllBytes(path);
        if (bytes.Length < 16 || System.Text.Encoding.ASCII.GetString(bytes, 0, 8) != "MOASTARS" ||
            BitConverter.ToUInt32(bytes, 8) != 1)
        {
            throw new InvalidDataException(path + " is not a star table of version 1");
        }
        int count = (int)BitConverter.ToUInt32(bytes, 12);
        if (bytes.Length != 16 + 12 * count)
        {
            throw new InvalidDataException(path + " has a wrong size");
        }
        var stars = new List<Star>(count);
        for (int i = 0; i < count; ++i)
        {
            int offset = 16 + 12 * i;
            double rightAscension = BitConverter.ToSingle(bytes, offset) * Math.PI / 180.0;
            double declination = BitConverter.ToSingle(bytes, offset + 4) * Math.PI / 180.0;
            float magnitude = BitConverter.ToInt16(bytes, offset + 8) / 100.0f;
            short colour = BitConverter.ToInt16(bytes, offset + 10);
            float colourIndex = colour == short.MinValue ? k_DefaultColourIndex : colour / 1000.0f;
            var direction = new Vector3((float)(Math.Cos(declination) * Math.Cos(rightAscension)),
                (float)(Math.Cos(declination) * Math.Sin(rightAscension)), (float)Math.Sin(declination));
            float packed = Mathf.Floor((magnitude + 2.0f) * 100.0f) +
                           Mathf.Clamp01((colourIndex + 0.5f) / 2.5f) * 0.99f;
            stars.Add(new Star { direction = direction, magnitude = magnitude, colourIndex = colourIndex, packed = packed });
        }
        return stars;
    }

    // The star direction and points on two circles around it, at the margin and at half the margin. The cells are
    // much larger than the margin, so these points find each cell that the circle touches.
    static IEnumerable<Vector3> Neighbourhood(Vector3 centre)
    {
        yield return centre;
        var any = Mathf.Abs(centre.z) < 0.9f ? Vector3.forward : Vector3.right;
        var a = Vector3.Normalize(Vector3.Cross(centre, any));
        var b = Vector3.Cross(centre, a);
        foreach (var radius in new[] { k_MarginDegrees, k_MarginDegrees * 0.5 })
        {
            double r = radius * Math.PI / 180.0;
            int points = radius == k_MarginDegrees ? 24 : 12;
            for (int i = 0; i < points; ++i)
            {
                double angle = 2.0 * Math.PI * i / points;
                var offset = a * (float)(Math.Sin(r) * Math.Cos(angle)) + b * (float)(Math.Sin(r) * Math.Sin(angle));
                yield return Vector3.Normalize(centre * (float)Math.Cos(r) + offset);
            }
        }
    }

    // The same rule as MoaStarCell in MoaStars.hlsl.
    public static int CellOf(Vector3 d)
    {
        float ax = Mathf.Abs(d.x);
        float ay = Mathf.Abs(d.y);
        float az = Mathf.Abs(d.z);
        int face;
        float u;
        float v;
        if (ax >= ay && ax >= az)
        {
            face = d.x > 0.0f ? 0 : 1;
            u = d.y / ax;
            v = d.z / ax;
        }
        else if (ay >= az)
        {
            face = d.y > 0.0f ? 2 : 3;
            u = d.x / ay;
            v = d.z / ay;
        }
        else
        {
            face = d.z > 0.0f ? 4 : 5;
            u = d.x / az;
            v = d.y / az;
        }
        int column = CellCoordinate(u);
        int row = CellCoordinate(v);
        return face * k_CellsPerFace * k_CellsPerFace + row * k_CellsPerFace + column;
    }

    static int CellCoordinate(float t)
    {
        float warped = Mathf.Atan(t) * (4.0f / Mathf.PI);
        return Mathf.Clamp((int)Mathf.Floor((warped + 1.0f) * 0.5f * k_CellsPerFace), 0, k_CellsPerFace - 1);
    }
}
