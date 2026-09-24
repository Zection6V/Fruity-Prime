using System;
using System.Collections.Generic;
using MphRead.Entities;
using OpenTK.Graphics.OpenGL;
using OpenTK.Mathematics;
using OpenTK.Windowing.Common;
using OpenTK.Windowing.Desktop;

namespace MphRead.Mods.MapGen
{
    /// <summary>
    /// Stands one hunter on one spot, jumps, morphs, and reports where the
    /// floor stopped working.
    ///
    /// Written for a report that no sweep could have found: on DUST2, jumping
    /// and then morphing at one particular place drops Weavel through the
    /// collision. The tour in <c>-maptest</c> morphs and jumps constantly and
    /// has never seen it, because the fault needs the two in a specific order
    /// at a specific height, and the tour never happens to be standing there
    /// when it does them.
    ///
    /// So this asks the narrow question instead: at this coordinate, for every
    /// delay between the jump and the morph, does the player end up below the
    /// ground he started on? It runs the real engine -- the real
    /// PlayerEntity, the real CheckCollision -- with nobody watching, so the
    /// answer is the game's and not a model's.
    ///
    /// Usage: -altprobe "DUST2" -at 1.17053,-2.73169,-41.3657 [-hunter Weavel]
    /// </summary>
    public sealed class AltFormProbe : GameWindow
    {
        private readonly string _room;
        private readonly Vector3 _start;
        private readonly Hunter _hunter;

        // One trial per delay between the jump and the morph. Zero is
        // "morph on the frame the jump is asked for"; the jump itself takes
        // about half a second to come back down, so this covers the whole
        // arc and a little past it.
        private static int MaxDelay => TraceDelay ?? 40;
        private const int Settle = 40;
        private const int Observe = 150;

        /// <summary>
        /// The probe drives this slot, not slot 0. Slot 0 is the main player
        /// and its controls are refilled from the keyboard on every frame, so
        /// anything written into them is gone before the engine reads it --
        /// which is why the tour's own reports have always said "moved 7/8".
        /// </summary>
        private const int Slot = 1;

        /// <summary>
        /// Print every frame of one trial instead of a table of all of them.
        /// Set by <c>-delay N</c>: a table says a fault is there, a trace says
        /// which frame it happens on.
        /// </summary>
        public static int? TraceDelay { get; set; }

        private int _delay;
        private int _frame;
        private int _resetFrames;
        private bool _armed;
        private float _rest;
        private float _lowest;
        private float _highest;
        private float _final;
        private readonly List<(int Delay, float Lowest, float Highest, float Final, bool Fell)> _trials = new();

        /// <summary>How far below the starting height counts as through the floor.</summary>
        private const float Through = 1.2f;

        private AltFormProbe(string room, Vector3 start, Hunter hunter)
            : base(new GameWindowSettings() { UpdateFrequency = 60 }, new NativeWindowSettings()
            {
                ClientSize = new Vector2i(320, 180),
                Title = "MphRead alt form probe",
                Profile = ContextProfile.Compatability,
                Flags = ContextFlags.Default,
                APIVersion = new Version(3, 2),
                StartVisible = false
            })
        {
            _room = room;
            _start = start;
            _hunter = hunter;
            _delay = TraceDelay ?? 0;
            Network.MapAudit.ForceEveryone = true;
            PlayerEntity.MaxPlayers = Math.Max(PlayerEntity.MaxPlayers, Slot + 1);
            Scene = new Scene(Size, KeyboardState, MouseState, _ => { }, () =>
            {
                Console.WriteLine($"    (the scene asked to close after trial {_delay})");
                Close();
            });
            // Slot 0 exists only to be the main player and stand still.
            Scene.AddPlayer(Hunter.Samus, recolor: 0, team: -1);
            Scene.AddPlayer(hunter, recolor: 0, team: -1);
            for (int i = 0; i < PlayerEntity.Players.Count; i++)
            {
                // No AI anywhere. PlayerAi writes the same controls this
                // probe does and wins, which reads as the probe's own inputs
                // being ignored and the player morphing on his own.
                PlayerEntity.Players[i].IsBot = false;
                PlayerEntity.Players[i].BotLevel = 0;
                if (i > Slot)
                {
                    PlayerEntity.Players[i].LoadFlags &= ~LoadFlags.Active;
                }
            }
            PlayerEntity.PlayerCount = Slot + 1;
            PlayerEntity.MainPlayerIndex = 0;
            Scene.AddRoom(room, GameMode.Battle, playerCount: Network.NetLaunch.RoomPlayerCount);
        }

        public Scene Scene { get; }

        protected override void OnLoad()
        {
            Scene.Size = ClientSize;
            Scene.OnLoad();
            base.OnLoad();
            GL.Viewport(0, 0, ClientSize.X, ClientSize.Y);
            Scene.OnResize();
        }

        protected override void OnRenderFrame(FrameEventArgs args)
        {
            GameState.ApplyPause();
            Scene.OnSimulationFrame();
            Scene.OnDrawFrame();
            if (!Scene.OnRenderFrame())
            {
                return;
            }
            Step();
            SwapBuffers();
            Scene.AfterRenderFrame();
            base.OnRenderFrame(args);
            if (_delay > MaxDelay)
            {
                Close();
            }
        }

        private void Step()
        {
            // No clock. The sweep is forty-odd trials long and the match it
            // runs inside would otherwise finish, roll its results screen and
            // quit the process somewhere in the middle of it -- which reads
            // as the probe stopping after one trial for no reason.
            GameState.MatchTime = -1;
            GameState.ForceEndGame = false;
            PlayerEntity player = PlayerEntity.Players[Slot];
            if (player.Health == 0 || !player.LoadFlags.TestFlag(LoadFlags.Active)
                || !player.LoadFlags.TestFlag(LoadFlags.Spawned))
            {
                return;
            }
            PlayerControls c = player.Controls;
            for (int i = 0; i < c.All.Length; i++)
            {
                c.All[i].IsDown = false;
                c.All[i].IsPressed = false;
                c.All[i].IsReleased = false;
            }
            if (!_armed)
            {
                // Put him on the spot, in biped form, at rest. A trial that
                // starts in the wrong form measures nothing, so unmorph first
                // and only then place him.
                // A reset that cannot finish is the one way this probe can
                // hang, and it hangs silently: a trial can leave the player
                // wedged where the engine refuses to let him stand up, and
                // then nothing the reset asks for ever happens. Say so and
                // stop instead, with what has been measured so far intact.
                if (++_resetFrames > 600)
                {
                    Console.WriteLine($"    (gave up resetting before trial {_delay}:"
                        + $" {(player.IsAltForm ? "alt" : player.IsMorphing ? "morphing" : "unmorphing")}"
                        + $" at y {player.Position.Y:F3}"
                        + $"{(player.Flags1.TestFlag(PlayerFlags1.NoUnmorph) ? ", cannot unmorph" : "")})");
                    _delay = MaxDelay + 1;
                    return;
                }
                if (player.IsMorphing || player.IsUnmorphing)
                {
                    // Let the animation finish. Asking again in the middle of
                    // one turns it round, and a reset that re-asks on a timer
                    // never converges.
                    _frame++;
                    return;
                }
                if (player.IsAltForm)
                {
                    // Put him back on the spot before asking. A trial can
                    // leave him wedged where he cannot stand up -- NoUnmorph
                    // is exactly that state -- and then pressing morph for
                    // ever gets no answer at all.
                    if (_frame % 60 == 0)
                    {
                        Place(player);
                    }
                    Press(player, c.Morph, _frame % 60 == 20);
                    Finish(player, c);
                    _frame++;
                    return;
                }
                // A quarter of a unit clear of the surface, and PrevPosition
                // moved with it. Teleport writes Position alone, and the
                // sweep's starting point is PrevPosition -- so a player put
                // back on top of a box he had fallen below starts every test
                // from under it, every face between the two is discarded as
                // "behind the starting point", and he falls straight through
                // again. That is the same rule this probe is here to measure,
                // so leaving it in the reset would have the probe confirming
                // itself.
                Place(player);
                _resetFrames = 0;
                _armed = true;
                _frame = 0;
                _lowest = _start.Y;
                _highest = _start.Y;
                _final = _start.Y;
                return;
            }
            _frame++;
            if (_frame < Settle)
            {
                // Let him come to rest on whatever he was placed above.
                return;
            }
            int t = _frame - Settle;
            if (t == 0)
            {
                // Where he actually came to rest, which is what "fell
                // through" has to be measured against. The coordinate this
                // probe was given is a place to put him, not a promise that
                // it is a floor -- on a spot picked off a headroom sweep
                // rather than out of a bug report he may legitimately settle
                // some way below it, and calling that a fall makes every
                // trial read YES and says nothing.
                _rest = player.Position.Y;
                _lowest = _rest;
                _highest = _rest;
            }
            bool jump = t >= 0 && t < 3;
            bool morph = t >= _delay && t < _delay + 3;
            Press(player, c.Jump, jump);
            Press(player, c.Morph, morph);
            Finish(player, c);
            _lowest = MathF.Min(_lowest, player.Position.Y);
            _highest = MathF.Max(_highest, player.Position.Y);
            _final = player.Position.Y;
            if (TraceDelay == _delay && t <= _delay + 50)
            {
                string form = player.IsAltForm ? "alt  " : player.IsMorphing ? "morph"
                    : player.IsUnmorphing ? "unmrp" : "biped";
                Console.WriteLine($"    {t,4}  {(jump ? "J" : " ")}{(morph ? "M" : " ")}  {form}"
                    + $"  y {player.Position.Y,8:F4}  prevY {player.PrevPosition.Y,8:F4}"
                    + $"  vy {player.Speed.Y,8:F4}"
                    + $"  {(player.Flags1.TestFlag(PlayerFlags1.Standing) ? "standing" : "        ")}"
                    + $"  {(player.Flags1.TestFlag(PlayerFlags1.NoUnmorph) ? "nounmorph" : "")}");
            }
            if (t >= _delay + Observe)
            {
                bool fell = _rest - _lowest > Through;
                _trials.Add((_delay, _lowest, _highest, _final, fell));
                if (TraceDelay == null)
                {
                    // Printed as it happens, not only in the summary: a run
                    // that ends early still has to show what it measured.
                    Console.WriteLine($"    {_delay,5}  {_highest,7:F3}   {_lowest,8:F3}"
                        + $"   {_final,7:F3}   {(fell ? "YES" : "")}");
                }
                _delay++;
                _armed = false;
                _frame = 0;
            }
        }

        private void Place(PlayerEntity player)
        {
            player.Teleport(_start.AddY(0.25f), Vector3.UnitZ, Scene.GetNodeRefByPosition(_start));
            player.PrevPosition = player.Position;
            player.Speed = Vector3.Zero;
        }

        private static void Press(PlayerEntity player, Keybind bind, bool down)
        {
            bind.IsDown = down;
        }

        private bool[] _wasDown = Array.Empty<bool>();

        private void Finish(PlayerEntity player, PlayerControls c)
        {
            if (_wasDown.Length < c.All.Length)
            {
                _wasDown = new bool[c.All.Length];
            }
            bool any = false;
            for (int i = 0; i < c.All.Length; i++)
            {
                Keybind bind = c.All[i];
                // Edges, not levels. Holding morph down and calling every
                // frame of it a press toggles the form over and over: the
                // animation is forty frames long, so the presses queue up and
                // the player morphs and unmorphs long after the input stopped.
                bind.IsPressed = bind.IsDown && !_wasDown[i];
                bind.IsReleased = !bind.IsDown && _wasDown[i];
                _wasDown[i] = bind.IsDown;
                any |= bind.IsDown || bind.IsReleased;
            }
            if (any)
            {
                // Same reason the tour does it: a scripted player's binds are
                // written after the pass that decides whether anybody is
                // playing, so without this he counts as idle and the engine
                // refuses half of what he asks for.
                player.ModNoteInput();
            }
        }

        private int Report()
        {
            Console.WriteLine();
            Console.WriteLine($"{_room}: {_hunter} at ({_start.X:F3}, {_start.Y:F3}, {_start.Z:F3})");
            Console.WriteLine();
            Console.WriteLine("  jump, then morph N frames later");
            Console.WriteLine($"  came to rest at y {_rest:F3} before each jump");
            Console.WriteLine();
            Console.WriteLine("    delay   peak Y   lowest Y   final Y   fell through");
            int fell = 0;
            foreach ((int delay, float lowest, float highest, float final, bool through) in _trials)
            {
                if (through)
                {
                    fell++;
                }
                Console.WriteLine($"    {delay,5}  {highest,7:F3}   {lowest,8:F3}   {final,7:F3}   {(through ? "YES" : "")}");
            }
            Console.WriteLine();
            Console.WriteLine(fell == 0
                ? "  the floor held in every trial"
                : $"  the floor gave way in {fell} of {_trials.Count} trials");
            return fell == 0 ? 0 : 1;
        }

        public static int Run(string room, Vector3 start, Hunter hunter)
        {
            AltFormProbe? window = null;
            try
            {
                window = new AltFormProbe(room, start, hunter);
                window.Run();
                return window.Report();
            }
            catch (Exception ex)
            {
                Console.WriteLine($"ALTPROBE {room} | {ex.GetType().Name}: {ex.Message}");
                Console.WriteLine(ex.StackTrace);
                return 1;
            }
            finally
            {
                window?.Dispose();
            }
        }
    }
}
