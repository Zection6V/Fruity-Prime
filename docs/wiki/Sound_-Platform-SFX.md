### Movement SFX

There are 45 structs of 4 shorts at `0x0217194C` describing platform movement SFX.

| ID | Platform | Start SFX 1 | Start SFX 2 | Stop SFX | Destroyed SFX |
:- | :- | :- | :- | :- | :-
00 | platform | `ELEVATOR_START` | N/A | `ELEVATOR_STOP` | N/A
01 | platform | `ELEVATOR_START` | N/A | `ELEVATOR_STOP` | N/A
02 | N/A | N/A | N/A | N/A | N/A
03 | Elevator | `ELEVATOR_START` | N/A | `ELEVATOR_STOP` | N/A
04 | smasher | `PISTON1`<sup>D</sup> | N/A | N/A | N/A
05 | Platform_Unit4_C1 | `ELEVATOR_START` | N/A | `ELEVATOR_STOP` | N/A
06 | pillar | N/A | N/A | N/A | N/A
07 | Door_Unit4_RM1 | `ARMORED_DOOR_SCR`<sup>S</sup> | `ARMORED_DOOR_SCR`<sup>S</sup> | N/A | N/A
08 | SyluxShip | N/A | N/A | N/A | N/A
09 | pistonmp7 | `STONE_PLATFORM2` | N/A | `STONE_PLATFORM3` | N/A
10 | unit3_brain | N/A | N/A | N/A | N/A
11 | unit4_mover1 | `STONE_PLATFORM2` | N/A | `STONE_PLATFORM3` | N/A
12 | unit4_mover2 | `STONE_PLATFORM2` | `STONE_PLATFORM` | `STONE_PLATFORM3` | N/A
13 | ElectroField1 | N/A | N/A | N/A | N/A
14 | Unit3_platform1 | `ELEVATOR_START` | N/A | `ELEVATOR_STOP` | N/A
15 | unit3_pipe1 | N/A | N/A | N/A | N/A
16 | unit3_pipe2 | N/A | N/A | N/A | N/A
17 | cylinderbase | N/A | N/A | N/A | N/A
18 | unit3_platform | `ELEVATOR_START` | N/A | `ELEVATOR_STOP` | N/A
19 | unit3_platform2 | `ELEVATOR_START` | N/A | `ELEVATOR_STOP` | N/A
20 | unit3_jar | N/A | N/A | N/A | N/A
21 | SyluxTurret | N/A | N/A | N/A | N/A
22 | unit3_jartop | N/A | N/A | N/A | N/A
23 | SamusShip | N/A | N/A | N/A | N/A
24 | unit1_land_plat1 | `STONE_PLATFORM2` | N/A | N/A | N/A
25 | unit1_land_plat2 | `STONE_PLATFORM2` | N/A | N/A | N/A
26 | unit1_land_plat3 | `STONE_PLATFORM2` | N/A | N/A | N/A
27 | unit1_land_plat4 | `STONE_PLATFORM2` | N/A | N/A | N/A
28 | unit1_land_plat5 | `STONE_PLATFORM2` | N/A | N/A | N/A
29 | unit2_c4_plat | `ELEVATOR_START` | N/A | `ELEVATOR_STOP` | N/A
30 | unit2_land_elev | `ELEVATOR_START` | N/A | `ELEVATOR_STOP` | N/A
31 | unit4_platform1 | N/A | N/A | N/A | N/A
32 | Crate01 | N/A | N/A | N/A | `BOX_BREAK1_SCR`<sup>S</sup>
33 | unit1_mover1 | N/A | N/A | N/A | N/A
34 | unit1_mover2 | `ELEVATOR2_START`<sup>E</sup> | N/A | `ELEVATOR2_STOP` | N/A
35 | unit2_mover1 | `ELEVATOR_START` | N/A | `ELEVATOR_STOP` | N/A
36 | unit4_mover3 | `STONE_PLATFORM2` | N/A | `STONE_PLATFORM3` | N/A
37 | unit4_mover4 | `STONE_PLATFORM2` | N/A | `STONE_PLATFORM3` | N/A
38 | unit3_mover1 | `ELEVATOR_START` | N/A | `ELEVATOR_STOP` | N/A
39 | unit2_c1_mover | N/A | N/A | N/A | `BOX_BREAK1_SCR`<sup>S</sup>
40 | unit3_mover2 | `ELEVATOR_START` | N/A | `ELEVATOR_STOP` | N/A
41 | piston_gorealand | `ELEVATOR_START` | N/A | `ELEVATOR_STOP` | N/A
42 | unit4_tp2_artifact_wo | N/A | N/A | N/A | N/A
43 | unit4_tp1_artifact_wo | N/A | N/A | N/A | N/A
44 | SamusShip | N/A | N/A | N/A | N/A

<sup>D</sup> Indicates SFX data from `DGNFILES.DAT`.

<sup>S</sup> Indicates SFX script data from `SFXSCRIPTFILES.DAT`.

<sup>E</sup> Indicates data referenced by an environment SFX index.

### Beam SFX

There are 4 structs at `0x0217194C` containing SFX information, indexed by the platform's beam ID. Platforms with a beam ID of -1 do not use this information, and while many platforms have a beam of ID of 0, other flags are set which mean it is not used. TODO: SyluxShip or SyluxTurret should actually use this ID to spawn missiles. Also, confirm that no platforms have a beam ID of 1.

| ID | Beam Type | SFX Name |
:- | :- | :-
0 | Missile | `MISSILE`
1 | Power Beam | N/A
2 | Energy Beam | `ELECTRIC_BARRIER`<sup>E</sup>
3 | Arc Welder | `ELECTRO_WAVE2`<sup>E</sup>

<sup>E</sup> Indicates data referenced by an environment SFX index.
