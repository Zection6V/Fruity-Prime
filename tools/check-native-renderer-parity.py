"""Compare current Renderer.cs projection/frustum code against native code.

Only the oracle's GL uniform upload is a no-op. The projection, plane math and
bounds-index selection are unmodified methods from the current managed source.
"""
import importlib.util
import math
from pathlib import Path
import random
import re
import tempfile

spec = importlib.util.spec_from_file_location("model_parity", Path(__file__).with_name("check-native-model-parity.py"))
common = importlib.util.module_from_spec(spec)
spec.loader.exec_module(common)
ROOT, run = common.ROOT, common.run

ORACLE = r'''
using System;
using System.Globalization;
using System.Diagnostics;
using OpenTK.Mathematics;
class FrustumPlane { public int XIndex1,XIndex2,YIndex1,YIndex2,ZIndex1,ZIndex2; public Vector4 Plane; }
class FrustumInfo { public int Index,Count; public FrustumPlane[] Planes=new FrustumPlane[10]; }
class CameraInfo { public Vector3 Position; }
class NodeRef {}
class PlayerEntity { public static PlayerEntity Main=new(); public CameraInfo CameraInfo=new(); public Vector3 FacingVector; public void Reposition(Vector3 p,Vector3 f,NodeRef n) {} }
class Room { public void LoadRoom(bool resume) {} }
class MovieSettings { public Vector3? AfterPosition=null,AfterFacing=null;public int MovieId=0; }
enum FadeType : byte { None,FadeInBlack,FadeOutBlack,FadeInWhite,FadeOutWhite,FadeOutInBlack,FadeOutInWhite }
enum AfterFade { None,Exit,LoadRoom,PlayMovie,StopMovie,EnterShip }
class ShaderLocations { public int ProjectionMatrix=0; }
static class GL { public static void UniformMatrix4(int location, bool transpose, ref Matrix4 matrix) {} public static void ClearColor(Color4 c) {} }
static class Vectors { public static Vector4 AddW(this Vector4 v,float w) { v.W+=w;return v; } }
class Scene {
    public Vector2i Size;
    public Matrix4 _viewMatrix, _perspectiveMatrix;
    public float _nearClip,_farClip,_cameraFov;
    public bool _useClip;
    private ShaderLocations _shaderLocations=new();
    public FrustumInfo FrustumInfo=new();
    public FadeType _fadeType; public float _fadeColor,_fadeStart,_fadeLength,_fadePercent,_fadeDelay;
    public bool _fadeIn,_fadeEnded; public AfterFade _afterFade;
    public float _globalElapsedTime,_frameTime;
    Color4 _clearColor; Room _room=new(); MovieSettings _movieSettings=new();
    void QuitGame(bool enteringShip) {} void PlayMovie(int id) {} void StopMovie() {}
    NodeRef GetNodeRefByName(string name) => new();
    public void FadeStep(float dt) { _frameTime=dt;_globalElapsedTime+=dt;UpdateFade(); }
    // METHODS
    public void Update() => UpdateProjection();
}
class Program {
    static void Main() {
        CultureInfo.CurrentCulture=CultureInfo.InvariantCulture;
        string? line;
        while ((line=Console.ReadLine())!=null) {
            try {
                if (line.StartsWith("F ")) {
                    var f=Array.ConvertAll(line[2..].Split(' '),float.Parse);
                    var fade=new Scene();
                    fade.SetFade((FadeType)f[0],f[1],true,(AfterFade)f[3],f[2]);
                    for (int i=0;i<(int)f[5];i++) fade.FadeStep(f[4]);
                    float opacity=fade._fadeIn?1-fade._fadePercent:fade._fadePercent;
                    float[] output={(int)fade._fadeType,fade._fadePercent,fade._fadeColor,opacity,fade._fadeEnded?1:0,(int)fade._afterFade,fade._fadeType!=FadeType.None?1:0};
                    Console.WriteLine(string.Join(" ",Array.ConvertAll(output,v=>v.ToString("R"))));
                    continue;
                }
                var x=Array.ConvertAll(line.Split(' '),float.Parse);
                var s=new Scene { Size=new((int)x[0],(int)x[1]),_cameraFov=x[2],_nearClip=x[3],_farClip=x[4],_useClip=x[5]!=0 };
                s._viewMatrix=new Matrix4(x[6],x[7],x[8],x[9],x[10],x[11],x[12],x[13],x[14],x[15],x[16],x[17],x[18],x[19],x[20],x[21]);
                PlayerEntity.Main.CameraInfo.Position=new(x[22],x[23],x[24]);
                s.Update();
                var values=new System.Collections.Generic.List<float>();
                for (int r=0;r<4;r++) for (int c=0;c<4;c++) values.Add(s._perspectiveMatrix[r,c]);
                for (int i=0;i<s.FrustumInfo.Count;i++) {
                    var p=s.FrustumInfo.Planes[i];
                    values.AddRange(new float[]{p.Plane.X,p.Plane.Y,p.Plane.Z,p.Plane.W,p.XIndex1,p.XIndex2,p.YIndex1,p.YIndex2,p.ZIndex1,p.ZIndex2});
                }
                Console.WriteLine(string.Join(" ",values.ConvertAll(v=>v.ToString("R"))));
            } catch(Exception) { Console.WriteLine("ERR"); }
        }
    }
}
'''

def main():
    source=(ROOT/'src/MphRead/Renderer.cs').read_text(encoding='utf-8-sig')
    oracle=ORACLE.replace('// METHODS','\n'.join(common.methods(source,name) for name in ('GetPerspectiveMatrix','UpdateProjection','SetBoundsIndices','SetFade','UpdateFade','EndFade')))
    project=(ROOT/'src/MphRead/MphRead.csproj').read_text(encoding='utf-8-sig')
    version=re.search(r'Include="OpenTK" Version="([^"]+)"',project)[1]
    with tempfile.TemporaryDirectory(prefix='fruity-renderer-parity-') as temp:
        temp=Path(temp)
        (temp/'Oracle.csproj').write_text('<Project Sdk="Microsoft.NET.Sdk"><PropertyGroup><OutputType>Exe</OutputType><TargetFramework>net9.0</TargetFramework><Nullable>enable</Nullable></PropertyGroup><ItemGroup><PackageReference Include="OpenTK.Mathematics" Version="'+version+'" /></ItemGroup></Project>',encoding='utf-8')
        (temp/'Program.cs').write_text(oracle,encoding='utf-8')
        run(['dotnet','build',temp/'Oracle.csproj','-c','Release','--nologo'])
        cpp=ROOT/'src/MphRead.Native'; exe=temp/'probe.exe'
        run(['C:/mingw64/bin/g++.exe','-std=c++20','-O2','-ffp-contract=off','-static-libgcc','-static-libstdc++','-I',cpp/'include',cpp/'Testing/renderer_differential_probe.cpp',cpp/'Renderer.cpp','-o',exe])
        rng=random.Random(20260909)
        identity=[1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1]
        cases=[]
        for i in range(3000):
            angle=rng.randrange(-314,315)/100
            c,s=math.cos(angle),math.sin(angle)
            view=[c,0,-s,0,0,1,0,0,s,0,c,0,1,2,3,1]
            cases.append([rng.choice([256,640,1280,1920,3440]),rng.choice([192,480,720,1080,1440]),rng.randrange(10,310)/100,0.0625,rng.randrange(1,1000),i%2,*view,*[rng.randrange(-1000,1001)/4 for _ in range(3)]])
        for fov in (-1,0,0.1,3.1415927,4):
            for near,far in ((0.0625,100),(0,100),(-1,100),(10,10),(100,10)):
                cases.append([256,192,fov,near,far,1,*identity,1,2,3])
        for type in range(7):
            for length in (0,0.1,0.5,1):
                for delay in (0,0.1,0.5):
                    for after in range(6):
                        for dt,steps in ((0,1),(0.25,1),(0.25,5),(1,2),(0.1,23)):
                            cases.append(['F',type,length,delay,after,dt,steps])
        data=''.join(' '.join(map(str,c))+'\n' for c in cases)
        managed=run(['dotnet',temp/'bin/Release/net9.0/Oracle.dll'],input=data).splitlines()
        native=run([exe],input=data).splitlines()
        assert len(managed)==len(native)==len(cases)
        mismatch=[]
        for case,a,b in zip(cases,managed,native):
            if a=='ERR' or b=='ERR':
                if a!=b: mismatch.append((case,a,b))
                continue
            x,y=list(map(float,a.split())),list(map(float,b.split()))
            count=7 if case[0]=='F' else 66
            if len(x)!=count or len(y)!=count or any(not (v==w or (math.isnan(v) and math.isnan(w)) or math.isclose(v,w,rel_tol=2e-5,abs_tol=2e-5)) for v,w in zip(x,y)):
                mismatch.append((case,a,b))
        if mismatch: raise AssertionError(f'{len(mismatch)} mismatches; first 2: {mismatch[:2]}')
        print(f'PASS: {len(cases)} C#/C++ Renderer cases: projection/frustum, bounds indices, invalid input rejection and fade timelines.')

if __name__=='__main__': main()
