### Environment SFX

An array of 10 existing SFX IDs can be found at `0x02120A3C`. When one is referenced to begin playing, one of 10 corresponding structs at `0x02123A60` is updated.

| Index | Name |
:- | :-
0 | `ELECTRO_WAVE2`
1 | `ELECTRICITY`
2 | `ELECTRIC_BARRIER`
3 | `ENERGY_BALL`
4 | `BLUE_FLAME`
5 | `CYLINDER_BOSS_ATTACK`
6 | `CYLINDER_BOSS_SPIN`
7 | `BUBBLES`
8 | `ELEVATOR2_START`
9 | `GOREA_ATTACK3_LOOP`

### Effect SFX

Several particle effects have corresponding SFX. When an object begins creating one of those effects, it also plays the SFX. This metadata is located at `0x02123090`. Only entries with an associated SFX are listed here.

| Effect ID | Effect Name | SFX Name |
:- | :- | :-
010 | `ballDeath` | `UNIT4_RM1_EXPLOSION_SCR`<sup>S</sup>
088 | `jetFlameBlue` | `BLUE_FLAME`<sup>E</sup>
089 | `lavaBurstLarge` | `LAVA_PLUME_SCR`<sup>S</sup>
097 | `lavaBurstExtraLarge` | `LAVA_PLUME_SCR`<sup>S</sup>
106 | `sparks` | `ELECTRICITY`<sup>E</sup>
127 | `bubblesRising` | `BUBBLES`<sup>E</sup>
186 | `sphereTricity` | `ENERGY_BALL`<sup>E</sup>
199 | `sphereTricitySmall` | `ELECTRIC_BARRIER`<sup>E</sup>

<sup>S</sup> Indicates SFX script data from `SFXSCRIPTFILES.DAT`.

<sup>E</sup> Indicates data referenced by an environment SFX index.
