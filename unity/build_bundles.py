#!/usr/bin/env python3
"""Build the AssetBundle of the sky mod with Unity 6000.5.7f1 in batch mode.

Usage: python unity/build_bundles.py [--unity PATH]
The log goes to D:/masters_of_albion/runs/moa_sky-bundle.log (or --log).
"""

import argparse
import pathlib
import subprocess
import sys

k_UnityVersion = "6000.5.7f1"
k_DefaultUnity = pathlib.Path(r"C:\Program Files\Unity\Hub\Editor") / k_UnityVersion / "Editor" / "Unity.exe"
k_Project = pathlib.Path(__file__).resolve().parent


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--unity", type=pathlib.Path, default=k_DefaultUnity)
    parser.add_argument("--log", type=pathlib.Path, default=pathlib.Path(r"D:\masters_of_albion\runs\moa_sky-bundle.log"))
    args = parser.parse_args()
    if not args.unity.is_file():
        print(f"Unity {k_UnityVersion} is not at {args.unity}", file=sys.stderr)
        return 2
    command = [str(args.unity), "-batchmode", "-quit", "-nographics", "-projectPath", str(k_Project),
               "-buildTarget", "StandaloneWindows64", "-executeMethod", "BuildBundles.Build",
               "-logFile", str(args.log)]
    result = subprocess.run(command, check=False)
    print(f"Unity returned {result.returncode}; the log is {args.log}")
    return result.returncode


if __name__ == "__main__":
    sys.exit(main())
