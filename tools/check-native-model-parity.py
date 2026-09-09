"""Differentially execute current Formats/Model.cs math and playback methods.

The oracle uses unmodified method bodies and the project's OpenTK version.
Its scaffolding contains only input/output data; no substitute math. Temporary
builds need dotnet and a C++20 compiler, but no ROM or renderer.
"""
import argparse
import math
from pathlib import Path
import random
import re
import shutil
import tempfile

import importlib.util
spec = importlib.util.spec_from_file_location("collision_parity", Path(__file__).with_name(
    "check-native-collision-parity.py"))
common = importlib.util.module_from_spec(spec)
spec.loader.exec_module(common)
ROOT, run = common.ROOT, common.run


def methods(source, name):
    result = []
    for match in re.finditer(r"(?:public|private) (?:static )?\w+ " + name + r"\(", source):
        end = source.index("{", match.start()) + 1
        depth = 1
        while depth:
            depth += (source[end] == "{") - (source[end] == "}")
            end += 1
        result.append(source[match.start():end])
    if not result:
        raise RuntimeError(f"Missing managed method: {name}")
    return "\n".join(result)


ORACLE = r'''
using System;
using System.Collections.Generic;
using System.Globalization;
using OpenTK.Mathematics;
[Flags] enum AnimFlags : ushort { None=0, PingPong=1, Reverse=2, Paused=4, NoLoop=8, Ended=16 }
static class Flags { public static bool TestFlag(this AnimFlags a, AnimFlags b) => (a & b) != 0; }
class Channel { public int Slot; public NodeAnimationGroup? Group; }
class AnimationInfo {
    public int[] Frame=new int[2], Step={1,1}, FrameCount=new int[2];
    public AnimFlags[] Flags=new AnimFlags[2];
    public Channel Node=new(), Material=new(), Texture=new(), Texcoord=new();
    public int NodeFrame => Frame[Node.Slot];
}
class Playback {
    public AnimationInfo AnimInfo=new();
    // PLAYBACK_METHODS
}
// ANIMATION_TYPES
class NodeAnimationGroup {
    public int FrameCount=1;
    public List<float> Scales=new(), Rotations=new(), Translations=new();
    public Dictionary<string,NodeAnimation> Animations=new();
}
class TexcoordAnimationGroup {
    public int FrameCount=1;
    public List<float> Scales=new(), Rotations=new(), Translations=new();
}
class Node {
    public string Name="";
    public int ParentIndex=-1, ChildIndex=-1, NextIndex=-1;
    public bool AnimIgnoreChild, AnimIgnoreParent;
    public Matrix4 Transform=Matrix4.Identity, Animation=Matrix4.Identity;
    public Matrix4? BeforeTransform, AfterTransform;
}
class Model {
    public List<Node> Nodes=new();
    // MODEL_METHODS
    public Matrix4 NodeMatrix(NodeAnimationGroup g, NodeAnimation a, Vector3 s) => AnimateNode(g,a,s,0);
    public Matrix4 StaticMatrix(Vector3 s, Vector3 a, Vector3 p) => ComputeNodeTransforms(s,a,p);
}
class Program {
    static void Print(Matrix4 m) {
        var a=new List<float>();
        for (int r=0;r<4;r++) for (int c=0;c<4;c++) a.Add(m[r,c]);
        Console.WriteLine(string.Join(" ",a.ConvertAll(v=>v.ToString("R"))));
    }
    static void Main() {
        CultureInfo.CurrentCulture=CultureInfo.InvariantCulture;
        string? line;
        while ((line=Console.ReadLine())!=null) {
            try {
                char op=line[0];
                var x=Array.ConvertAll(line[2..].Split(' '),float.Parse);
                var model=new Model();
                if (op=='F') {
                    var p=new Playback(); int slot=(int)x[0];
                    p.AnimInfo.Frame[slot]=(int)x[1]; p.AnimInfo.Step[slot]=(int)x[2];
                    p.AnimInfo.FrameCount[slot]=(int)x[3]; p.AnimInfo.Flags[slot]=(AnimFlags)x[4];
                    p.AnimInfo.Node.Slot=p.AnimInfo.Material.Slot=p.AnimInfo.Texture.Slot=p.AnimInfo.Texcoord.Slot=slot;
                    for (int i=0;i<(int)x[5];i++) p.UpdateAnimFrames();
                    Console.WriteLine($"{p.AnimInfo.Frame[slot]} {(int)p.AnimInfo.Flags[slot]}");
                } else if (op=='I') {
                    var values=new List<float>(x[7..]);
                    Console.WriteLine(model.InterpolateAnimation(values,(int)x[0],(int)x[1],(int)x[2],(int)x[3],(int)x[4],x[5]!=0).ToString("R"));
                } else if (op=='N') {
                    var g=new NodeAnimationGroup { Scales=new(x[0..3]),Rotations=new(x[3..6]),Translations=new(x[6..9]) };
                    var a=new NodeAnimation();
                    // NODE_CONSTANTS
                    Print(model.NodeMatrix(g,a,new(x[9],x[10],x[11])));
                } else if (op=='U') {
                    var g=new TexcoordAnimationGroup { Scales=new(x[0..2]),Rotations=new(x[2..3]),Translations=new(x[3..5]) };
                    var a=new TexcoordAnimation();
                    // UV_CONSTANTS
                    Print(model.AnimateTexcoords(g,a,0));
                } else if (op=='T' || op=='B') {
                    var m=model.StaticMatrix(new(x[0],x[1],x[2]),new(x[3],x[4],x[5]),new(x[6],x[7],x[8]));
                    Print(op=='T'?m:m.ClearRotation());
                } else if (op=='H') {
                    var root=new Node { Name="a",ChildIndex=1,AnimIgnoreChild=x[1]!=0,Transform=Matrix4.CreateTranslation(2,0,0) };
                    var child=new Node { Name="b",ParentIndex=0,Animation=Matrix4.CreateTranslation(99,0,0) };
                    var t=Matrix4.CreateTranslation(0,x[3],0);
                    if (x[2]==1) root.BeforeTransform=t;
                    if (x[2]==2) root.AfterTransform=t;
                    model.Nodes.Add(root); model.Nodes.Add(child);
                    var info=new AnimationInfo(); var parent=Matrix4.CreateTranslation(0,0,3);
                    if (x[0]==0) model.AnimateNodes(0,true,parent,Vector3.One,info);
                    else model.AnimateNodes2(0,true,parent,Vector3.One,info);
                    Print(root.Animation); Print(child.Animation);
                } else throw new Exception("Unknown test");
            } catch (Exception) { Console.WriteLine("ERR"); }
        }
    }
}
'''


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--cxx", default=shutil.which("g++") or "C:/mingw64/bin/g++.exe")
    args = parser.parse_args()
    source = (ROOT / "src/MphRead/Formats/Model.cs").read_text(encoding="utf-8-sig")
    names = ["InterpolateAnimation", "AnimateNode", "AnimateTexcoords", "ComputeNodeTransforms", "AnimateNodes", "AnimateNodes2"]
    oracle = ORACLE.replace("// PLAYBACK_METHODS", methods(source,"UpdateAnimFrames"))
    oracle = oracle.replace("// MODEL_METHODS", "\n".join(methods(source,n) for n in names))
    declarations = []
    for cls, name, marker in (("NodeAnimation","AnimateNode","NODE"), ("TexcoordAnimation","AnimateTexcoords","UV")):
        fields = sorted(set(re.findall(r"animation\.(\w+)", methods(source,name))))
        declarations.append(f"class {cls} {{ public int " + ",".join(fields) + "; }")
        assignments = []
        for field in fields:
            value = 1 if "Length" in field else 0
            if "Index" in field:
                value = {"Y":1,"Z":2}.get(field[-1],0) if marker == "NODE" else int(field[-1]=='T')
            assignments.append(f"a.{field}={value};")
        oracle = oracle.replace(f"// {marker}_CONSTANTS", " ".join(assignments))
    oracle = oracle.replace("// ANIMATION_TYPES", "\n".join(declarations))
    project = (ROOT / "src/MphRead/MphRead.csproj").read_text(encoding="utf-8-sig")
    version = re.search(r'Include="OpenTK" Version="([^"]+)"',project)[1]
    with tempfile.TemporaryDirectory(prefix="fruity-model-parity-") as temp:
        temp=Path(temp)
        (temp/"Oracle.csproj").write_text('<Project Sdk="Microsoft.NET.Sdk"><PropertyGroup><OutputType>Exe</OutputType><TargetFramework>net9.0</TargetFramework><Nullable>enable</Nullable></PropertyGroup><ItemGroup><PackageReference Include="OpenTK.Mathematics" Version="'+version+'" /></ItemGroup></Project>',encoding="utf-8")
        (temp/"Program.cs").write_text(oracle,encoding="utf-8")
        run(["dotnet","build",temp/"Oracle.csproj","-c","Release","--nologo"])
        cpp=ROOT/"src/MphRead.Native"
        exe=temp/"probe.exe"
        run([args.cxx,"-std=c++20","-O2","-ffp-contract=off","-static-libgcc","-static-libstdc++","-I",cpp/"include",cpp/"Testing/model_differential_probe.cpp",cpp/"Formats/Model.cpp",cpp/"Formats/model_instance.cpp","-o",exe])
        cases=[]
        for flags in range(32):
            for count in (0,1,2,3,15):
                for frame in (-1,0,1,count-1,count):
                    for step in (-1,0,1,2,4):
                        cases.append(['F',flags%2,frame,step,count,flags,5])
        rng=random.Random(20260909)
        for i in range(2000):
            count=rng.randrange(0,25); length=rng.randrange(0,25)
            values=[rng.randrange(-4096,4097)/1024 for _ in range(40)]
            cases.append(['I',rng.randrange(-2,8),rng.randrange(-2,27),rng.choice([0,1,2,3,4,8,64,66]),length,count,i%2,40,*values])
        for i in range(500):
            scale=[rng.randrange(-16,17)/4 for _ in range(3)]
            angles=[rng.randrange(-300,301)/100 for _ in range(3)]
            pos=[rng.randrange(-16,17)/4 for _ in range(3)]
            cases += [['N',*scale,*angles,*pos,1,2,4], ['T',*scale,*angles,*pos], ['B',*scale,*angles,*pos], ['U',*scale[:2],angles[0],*pos[:2]]]
        for mode in (0,1):
            for ignore in (0,1):
                for attachment in (0,1,2):
                    cases.append(['H',mode,ignore,attachment,5])
        inputs=''.join(' '.join(map(str,c))+'\n' for c in cases)
        managed=run(['dotnet',temp/'bin/Release/net9.0/Oracle.dll'],input=inputs).splitlines()
        native=run([exe],input=inputs).splitlines()
        expected=sum(2 if c[0]=='H' else 1 for c in cases)
        assert len(managed)==len(native)==expected, (len(managed),len(native),expected)
        mismatch=[]; index=0
        for case in cases:
            for _ in range(2 if case[0]=='H' else 1):
                a,b=managed[index],native[index]; index+=1
                if a=='ERR' or b=='ERR':
                    if a!=b: mismatch.append((case,a,b))
                    continue
                x,y=list(map(float,a.split())),list(map(float,b.split()))
                if len(x)!=len(y) or any(not ((math.isnan(v) and math.isnan(w)) or v==w or math.isclose(v,w,rel_tol=2e-5,abs_tol=2e-5)) for v,w in zip(x,y)):
                    mismatch.append((case,a,b))
        if mismatch: raise AssertionError(f'{len(mismatch)} mismatches; first 5: {mismatch[:5]}')
        print(f'PASS: {len(cases)} C#/C++ Model cases: playback, interpolation, node/UV matrices, billboards, both hierarchy traversal variants.')


if __name__=='__main__': main()

