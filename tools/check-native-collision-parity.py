"""Run the current C# collision methods against native code on identical inputs.

Requires dotnet, the managed project's OpenTK.Mathematics package and a C++20
compiler. Generated oracle/build products stay in a temporary directory. No ROM
or desktop is needed. The oracle method bodies are extracted without rewriting
from the current managed source, so a future C# change is tested automatically.
"""
import argparse
import math
from pathlib import Path
import random
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
METHODS = [
    "CheckSphereOverlapVolume", "CheckCylinderOverlapVolume", "CheckCylindersOverlap",
    "CheckCylinderOverlapVolumeHelper", "CheckVolumesOverlap",
    "CheckCylinderOverlapSphere", "CheckCylinderIntersectPlane", "CheckCylinderBetweenPoints",
]


def method(source, name):
    import re
    match = re.search(r"(?:public|private) static bool " + name + r"\(", source)
    if not match:
        raise RuntimeError(f"Managed method disappeared: {name}")
    start = source.index("{", match.start())
    end, depth = start + 1, 1
    while depth:
        depth += (source[end] == "{") - (source[end] == "}")
        end += 1
    return source[match.start():end]


ORACLE = r'''
using System;
using System.Globalization;
using OpenTK.Mathematics;
enum VolumeType { Box, Cylinder, Sphere }
enum CollisionFlags : ushort { None }
class CollisionVolume {
    public VolumeType Type;
    public Vector3 BoxPosition, BoxVector1, BoxVector2, BoxVector3;
    public float BoxDot1, BoxDot2, BoxDot3;
    public Vector3 CylinderPosition, CylinderVector, SpherePosition;
    public float CylinderRadius, CylinderDot, SphereRadius;
}
struct CollisionResult {
    public byte Field0;
    public CollisionFlags Flags;
    public Vector4 Plane;
    public float Field14;
    public Vector3 Position;
    public float Distance;
    public object? EntityCollision;
    public Vector3 EdgePoint1, EdgePoint2;
}
class Program {
    // METHODS
    static CollisionVolume Volume(int kind, Vector3 p, Vector3 d, float r, float h) => new() {
        Type = (VolumeType)kind, BoxPosition = p, CylinderPosition = p, SpherePosition = p,
        BoxVector1 = Vector3.UnitX, BoxVector2 = Vector3.UnitY, BoxVector3 = Vector3.UnitZ,
        BoxDot1 = r, BoxDot2 = h, BoxDot3 = 2*r,
        CylinderVector = d, CylinderRadius = r, SphereRadius = r, CylinderDot = h
    };
    static void Main() {
        CultureInfo.CurrentCulture = CultureInfo.InvariantCulture;
        string? line;
        while ((line = Console.ReadLine()) != null) {
            var x = Array.ConvertAll(line.Split(' '), float.Parse);
            int op = (int)x[0];
            Vector3 a = new(x[3], x[4], x[5]), b = new(x[6], x[7], x[8]);
            Vector3 c = new(x[9], x[10], x[11]), d = new(x[12], x[13], x[14]);
            float radius = x[15], height = x[16];
            var one = Volume((int)x[1], a, d, radius, height);
            var two = Volume((int)x[2], b, d, radius * 0.5f, height);
            var r = new CollisionResult { Field0=7, Flags=(CollisionFlags)0x2138,
                Plane=new(11,12,13,14), Field14=15, Position=new(16,17,18), Distance=19,
                EntityCollision=new object(), EdgePoint1=new(20,21,22), EdgePoint2=new(23,24,25) };
            bool hit = op switch {
                0 => CheckSphereOverlapVolume(one,b,radius*0.5f,ref r),
                1 => CheckCylinderOverlapVolume(one,b,c,radius*0.5f,ref r),
                2 => CheckCylindersOverlap(a,b,c,d,height,radius,ref r),
                3 => CheckVolumesOverlap(one,two,ref r),
                4 => CheckCylinderOverlapSphere(a,b,c,radius,ref r),
                5 => CheckCylinderIntersectPlane(a,b,new(d,height),ref r),
                6 => CheckCylinderBetweenPoints(a,b,c,height,radius,ref r),
                _ => throw new Exception("unknown operation")
            };
            float[] output = { hit?1:0, r.Field0, (ushort)r.Flags,
                r.Plane.X, r.Plane.Y, r.Plane.Z, r.Plane.W, r.Field14,
                r.Position.X,r.Position.Y,r.Position.Z,r.Distance,r.EntityCollision!=null?1:0,
                r.EdgePoint1.X,r.EdgePoint1.Y,r.EdgePoint1.Z,r.EdgePoint2.X,r.EdgePoint2.Y,r.EdgePoint2.Z };
            Console.WriteLine(string.Join(" ", Array.ConvertAll(output,v=>v.ToString("R"))));
        }
    }
}
'''


def run(command, **kwargs):
    result = subprocess.run([str(x) for x in command], text=True, encoding="utf-8",
                            capture_output=True, **kwargs)
    if result.returncode:
        raise RuntimeError(f"Command failed ({result.returncode}): {command}\n"
                           f"{result.stdout[-4000:]}\n{result.stderr[-4000:]}")
    return result.stdout


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--cxx", default=shutil.which("g++") or "C:/mingw64/bin/g++.exe")
    parser.add_argument("--cases", type=int, default=10000)
    args = parser.parse_args()
    source = (ROOT / "src/MphRead/Formats/CollisionDetection.cs").read_text(encoding="utf-8-sig")
    # Match the dependency version already used by the managed project.
    import re
    project = (ROOT / "src/MphRead/MphRead.csproj").read_text(encoding="utf-8-sig")
    version = re.search(r'Include="OpenTK" Version="([^"]+)"', project)[1]
    with tempfile.TemporaryDirectory(prefix="fruity-collision-parity-") as temp:
        temp = Path(temp)
        (temp / "Oracle.csproj").write_text(
            '<Project Sdk="Microsoft.NET.Sdk"><PropertyGroup><OutputType>Exe</OutputType>'
            '<TargetFramework>net9.0</TargetFramework><Nullable>enable</Nullable>'
            '</PropertyGroup><ItemGroup><PackageReference Include="OpenTK.Mathematics" '
            f'Version="{version}" /></ItemGroup></Project>', encoding="utf-8")
        (temp / "Program.cs").write_text(ORACLE.replace("// METHODS", "\n".join(
            method(source, name) for name in METHODS)), encoding="utf-8")
        run(["dotnet", "build", temp / "Oracle.csproj", "-c", "Release", "--nologo"])
        cpp = ROOT / "src/MphRead.Native"
        exe = temp / "probe.exe"
        run([args.cxx, "-std=c++20", "-O2", "-ffp-contract=off", "-static-libgcc",
             "-static-libstdc++", "-I", cpp / "include",
             cpp / "Testing/collision_differential_probe.cpp",
             cpp / "Formats/CollisionDetection.cpp", cpp / "Formats/Collision.cpp",
             "-o", exe])
        rng = random.Random(20260909)
        cases = []
        # Each operation/type pairing: coincident points, parallel segments,
        # zero radius, zero height, boundary contacts and opposite directions.
        for op in range(7):
            for k1 in range(3):
                for k2 in range(3):
                    for b in ([0,0,0], [0,2,0], [2,0,0], [0,-2,0]):
                        for radius, height in ((0,0), (1,0), (0,2), (1,2)):
                            cases.append([op,k1,k2,0,0,0,*b,0,0,0,0,1,0,radius,height])
        for i in range(args.cases):
            xyz = [rng.randrange(-16,17)/4 for _ in range(9)]
            direction = rng.choice(([1,0,0],[0,1,0],[0,0,1],[-1,0,0],[0,-1,0],[0,0,-1],[0.6,0.8,0]))
            cases.append([i%7,rng.randrange(3),rng.randrange(3),*xyz,*direction,
                          rng.randrange(0,17)/4,rng.randrange(0,17)/4])
        inputs = "".join(" ".join(map(str,c)) + "\n" for c in cases)
        managed = run(["dotnet", temp / "bin/Release/net9.0/Oracle.dll"], input=inputs).splitlines()
        native = run([exe], input=inputs).splitlines()
        if len(managed) != len(cases) or len(native) != len(cases):
            raise AssertionError("Probe result count differs from input count")
        mismatches = []
        for index, (left, right) in enumerate(zip(managed, native)):
            a, b = list(map(float,left.split())), list(map(float,right.split()))
            if len(a) != 19 or len(b) != 19:
                raise AssertionError("Probe result field count differs")
            for field, (x,y) in enumerate(zip(a,b)):
                equal = (math.isnan(x) and math.isnan(y)) or x == y
                if not equal and field not in (0,1,2,12):
                    equal = math.isclose(x,y,rel_tol=2e-5,abs_tol=2e-5)
                if not equal:
                    mismatches.append((index,field,x,y,cases[index]))
        if mismatches:
            raise AssertionError(f"{len(mismatches)} field mismatches; first 10: {mismatches[:10]}")
        print(f"PASS: {len(cases)} C#/C++ collision cases, 19 result fields each; "
              "7 public methods plus volume helper; NaN and unchanged fields checked.")


if __name__ == "__main__":
    main()
