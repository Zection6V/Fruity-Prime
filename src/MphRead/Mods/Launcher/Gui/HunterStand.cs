#if MPHREAD_AVALONIA
using System;
using System.Collections.Generic;
using Avalonia;
using Avalonia.Controls;
using Avalonia.Input;
using Avalonia.Media;
using Avalonia.Threading;

namespace MphRead.Mods.Launcher.Gui
{
    /// <summary>
    /// A hunter on a turntable, for the two screens that ask which one you are
    /// taking in: the server panel and the results.
    ///
    /// <para>
    /// <b>This is not the model.</b> The hunters are
    /// <c>startgame/choice*_Model.bin</c>, an NDS display-list format only the
    /// engine reads, and the engine is not running while the launcher's screens
    /// are. So each is built here out of boxes: enough silhouette to tell seven
    /// of them apart at nine ems -- Samus's shoulder cannon, Kanden's head
    /// tendril in three segments, Trace's sniper arm, Noxus's cape, Weavel's
    /// thin waist -- and the same slot a real render drops into the day the
    /// match-end camera can hand one over.
    /// </para>
    ///
    /// <para>
    /// A painter's-algorithm renderer: project the eight corners of each box,
    /// sort the quads back to front, fill them flat against one light. Eighty-
    /// four boxes is five hundred quads, which is nothing next to the room
    /// behind it -- and unlike the room, it is only drawn while it is turning.
    /// </para>
    /// </summary>
    internal sealed class HunterStand : Control
    {
        private readonly record struct Box(
            float X, float Y, float Z, float W, float H, float D, Color Colour);

        private readonly record struct Hunter(Color Tint, Box[] Boxes);

        private static Color Rgb(uint hex) => Color.FromRgb(
            (byte)(hex >> 16), (byte)(hex >> 8), (byte)hex);

        private static readonly Dictionary<string, Hunter> _hunters = new()
        {
            ["Samus"] = new Hunter(Rgb(0xb4632c), new Box[]
            {
                new(0f, 46f, 0f, 26f, 26f, 20f, Rgb(0xc4702f)),
                new(0f, 66f, 0f, 15f, 13f, 15f, Rgb(0xd8862f)),
                new(0f, 70f, 7f, 11f, 6f, 5f, Rgb(0x2f6f8f)),
                new(-17f, 50f, 0f, 10f, 14f, 14f, Rgb(0xc4702f)),
                new(17f, 50f, 0f, 12f, 15f, 15f, Rgb(0xe08b34)),
                new(17f, 36f, 2f, 11f, 16f, 13f, Rgb(0xd8862f)),
                new(-15f, 34f, 0f, 8f, 16f, 8f, Rgb(0xa85a28)),
                new(-7f, 18f, 0f, 11f, 26f, 11f, Rgb(0x8e4d24)),
                new(7f, 18f, 0f, 11f, 26f, 11f, Rgb(0x8e4d24)),
                new(-7f, 3f, 2f, 12f, 7f, 14f, Rgb(0x6f3d1e)),
                new(7f, 3f, 2f, 12f, 7f, 14f, Rgb(0x6f3d1e))
            }),
            ["Kanden"] = new Hunter(Rgb(0x4c8f5a), new Box[]
            {
                new(0f, 48f, 0f, 22f, 28f, 17f, Rgb(0x4c8f5a)),
                new(0f, 68f, 0f, 13f, 12f, 13f, Rgb(0x5da76b)),
                new(0f, 71f, 6f, 10f, 5f, 4f, Rgb(0xc9d94a)),
                new(0f, 78f, -6f, 8f, 7f, 10f, Rgb(0x3f7a4c)),
                new(0f, 84f, -14f, 7f, 6f, 10f, Rgb(0x3f7a4c)),
                new(0f, 86f, -23f, 6f, 5f, 9f, Rgb(0x357044)),
                new(-15f, 50f, 0f, 8f, 13f, 11f, Rgb(0x4c8f5a)),
                new(15f, 50f, 0f, 8f, 13f, 11f, Rgb(0x4c8f5a)),
                new(-14f, 33f, 0f, 7f, 18f, 7f, Rgb(0x3f7a4c)),
                new(14f, 33f, 0f, 7f, 18f, 7f, Rgb(0x3f7a4c)),
                new(-6f, 17f, 0f, 9f, 26f, 9f, Rgb(0x357044)),
                new(6f, 17f, 0f, 9f, 26f, 9f, Rgb(0x357044)),
                new(-6f, 3f, 2f, 10f, 7f, 13f, Rgb(0x2a5a37)),
                new(6f, 3f, 2f, 10f, 7f, 13f, Rgb(0x2a5a37))
            }),
            ["Trace"] = new Hunter(Rgb(0x9a8a4e), new Box[]
            {
                new(0f, 52f, 0f, 19f, 30f, 15f, Rgb(0x9a8a4e)),
                new(0f, 73f, 0f, 12f, 12f, 13f, Rgb(0xab9a58)),
                new(0f, 76f, 6f, 9f, 4f, 4f, Rgb(0xc94a4a)),
                new(-14f, 55f, 0f, 7f, 12f, 10f, Rgb(0x9a8a4e)),
                new(14f, 55f, 0f, 7f, 12f, 10f, Rgb(0x9a8a4e)),
                new(-13f, 38f, 0f, 6f, 20f, 6f, Rgb(0x8a7b45)),
                new(13f, 36f, 6f, 6f, 22f, 20f, Rgb(0x7a6d3d)),
                new(-6f, 18f, 0f, 9f, 30f, 9f, Rgb(0x8a7b45)),
                new(6f, 18f, 0f, 9f, 30f, 9f, Rgb(0x8a7b45)),
                new(-6f, 3f, 2f, 10f, 7f, 13f, Rgb(0x6b6033)),
                new(6f, 3f, 2f, 10f, 7f, 13f, Rgb(0x6b6033))
            }),
            ["Sylux"] = new Hunter(Rgb(0x3f7fa8), new Box[]
            {
                new(0f, 48f, 0f, 21f, 27f, 16f, Rgb(0x3f7fa8)),
                new(0f, 67f, 0f, 13f, 12f, 13f, Rgb(0x4a8fbc)),
                new(0f, 70f, 6f, 10f, 5f, 4f, Rgb(0x6fe0c8)),
                new(-16f, 51f, 0f, 9f, 13f, 12f, Rgb(0x3f7fa8)),
                new(16f, 51f, 0f, 9f, 13f, 12f, Rgb(0x3f7fa8)),
                new(-15f, 35f, 3f, 8f, 17f, 14f, Rgb(0x356f94)),
                new(15f, 35f, 3f, 8f, 17f, 14f, Rgb(0x356f94)),
                new(-6f, 17f, 0f, 10f, 26f, 10f, Rgb(0x2d6182)),
                new(6f, 17f, 0f, 10f, 26f, 10f, Rgb(0x2d6182)),
                new(-6f, 3f, 2f, 11f, 7f, 13f, Rgb(0x24506c)),
                new(6f, 3f, 2f, 11f, 7f, 13f, Rgb(0x24506c))
            }),
            ["Noxus"] = new Hunter(Rgb(0x6f86b8), new Box[]
            {
                new(0f, 42f, 0f, 24f, 24f, 18f, Rgb(0x8c9fc9)),
                new(0f, 60f, 0f, 14f, 12f, 14f, Rgb(0xa8b8d8)),
                new(0f, 63f, 6f, 10f, 5f, 4f, Rgb(0x3f5f8f)),
                new(0f, 46f, -13f, 30f, 26f, 6f, Rgb(0x6f86b8)),
                new(-17f, 44f, 0f, 9f, 12f, 12f, Rgb(0x8c9fc9)),
                new(17f, 44f, 0f, 9f, 12f, 12f, Rgb(0x8c9fc9)),
                new(-15f, 30f, 0f, 8f, 14f, 8f, Rgb(0x7a8cbe)),
                new(15f, 30f, 0f, 8f, 14f, 8f, Rgb(0x7a8cbe)),
                new(-7f, 15f, 0f, 11f, 22f, 11f, Rgb(0x6f86b8)),
                new(7f, 15f, 0f, 11f, 22f, 11f, Rgb(0x6f86b8)),
                new(-7f, 3f, 2f, 12f, 7f, 14f, Rgb(0x5a6f9e)),
                new(7f, 3f, 2f, 12f, 7f, 14f, Rgb(0x5a6f9e))
            }),
            ["Spire"] = new Hunter(Rgb(0xa85f2e), new Box[]
            {
                new(0f, 50f, 0f, 30f, 30f, 22f, Rgb(0xa85f2e)),
                new(0f, 71f, 0f, 16f, 14f, 15f, Rgb(0xbb6f35)),
                new(0f, 74f, 7f, 11f, 5f, 4f, Rgb(0xe0a43c)),
                new(-21f, 54f, 0f, 13f, 16f, 16f, Rgb(0x96542a)),
                new(21f, 54f, 0f, 13f, 16f, 16f, Rgb(0x96542a)),
                new(-20f, 36f, 0f, 11f, 18f, 11f, Rgb(0x8a4d26)),
                new(20f, 36f, 0f, 11f, 18f, 11f, Rgb(0x8a4d26)),
                new(-9f, 17f, 0f, 13f, 26f, 13f, Rgb(0x7c4522)),
                new(9f, 17f, 0f, 13f, 26f, 13f, Rgb(0x7c4522)),
                new(-9f, 3f, 2f, 14f, 7f, 15f, Rgb(0x63371b)),
                new(9f, 3f, 2f, 14f, 7f, 15f, Rgb(0x63371b))
            }),
            ["Weavel"] = new Hunter(Rgb(0x8a7f96), new Box[]
            {
                new(0f, 56f, 0f, 24f, 20f, 17f, Rgb(0x8a7f96)),
                new(0f, 44f, 0f, 10f, 10f, 9f, Rgb(0x5f5769)),
                new(0f, 71f, 0f, 13f, 12f, 13f, Rgb(0x9a8ea8)),
                new(0f, 74f, 6f, 10f, 5f, 4f, Rgb(0xc94a4a)),
                new(-17f, 58f, 0f, 10f, 13f, 12f, Rgb(0x8a7f96)),
                new(17f, 58f, 0f, 10f, 13f, 12f, Rgb(0x8a7f96)),
                new(-16f, 42f, 0f, 8f, 18f, 8f, Rgb(0x776c85)),
                new(16f, 42f, 2f, 9f, 18f, 13f, Rgb(0x776c85)),
                new(-7f, 32f, 0f, 11f, 16f, 11f, Rgb(0x6b6178)),
                new(7f, 32f, 0f, 11f, 16f, 11f, Rgb(0x6b6178)),
                new(-7f, 15f, 2f, 10f, 20f, 12f, Rgb(0x5f5769)),
                new(7f, 15f, 2f, 10f, 20f, 12f, Rgb(0x5f5769)),
                new(-7f, 3f, 4f, 12f, 7f, 16f, Rgb(0x4d4659)),
                new(7f, 3f, 4f, 12f, 7f, 16f, Rgb(0x4d4659))
            })
        };

        /// <summary>The seven, in the order the pickers step through them.</summary>
        public static readonly string[] Names =
        {
            "Samus", "Kanden", "Trace", "Sylux", "Noxus", "Spire", "Weavel"
        };

        public static Color TintOf(string name) =>
            _hunters.TryGetValue(name, out Hunter h) ? h.Tint : GuiTheme.Accent;

        /// <summary>The six faces of a unit cube, as corner indices.</summary>
        private static readonly int[][] _faces =
        {
            new[] { 0, 1, 2, 3 }, new[] { 5, 4, 7, 6 }, new[] { 4, 0, 3, 7 },
            new[] { 1, 5, 6, 2 }, new[] { 4, 5, 1, 0 }, new[] { 3, 2, 6, 7 }
        };

        private static readonly double[][] _normals =
        {
            new[] { 0d, 0, 1 }, new[] { 0d, 0, -1 }, new[] { -1d, 0, 0 },
            new[] { 1d, 0, 0 }, new[] { 0d, 1, 0 }, new[] { 0d, -1, 0 }
        };

        private static readonly double[] _light = Normalise(-0.45, 0.78, 0.44);

        private static double[] Normalise(double x, double y, double z)
        {
            double m = Math.Sqrt(x * x + y * y + z * z);
            return new[] { x / m, y / m, z / m };
        }

        /// <summary>
        /// Which hunter. Not <c>Name</c>: every Avalonia control already has
        /// one of those and it is the thing XAML looks a control up by, so
        /// shadowing it here would be a bug that only shows on a machine that
        /// happens to go looking.
        /// </summary>
        /// <summary>
        /// Which of the hunter's four suits this is wearing, 0-3.
        ///
        /// The colours are not written down anywhere and must not be: a suit
        /// is <c>pal_01</c> to <c>pal_04</c> on that hunter's own model, and
        /// what those palettes hold is a fact about the player's extracted
        /// files rather than something this repository carries. See
        /// <see cref="Mods.HunterSuits"/>, which samples them.
        ///
        /// The boxes keep their own light and shade: each is recoloured by
        /// the ratio it already stands at against the hunter's base tint, so
        /// swapping a suit swaps the palette and not the modelling -- which
        /// is what a recolor is.
        /// </summary>
        public int Suit
        {
            get => _suit;
            set
            {
                int clamped = Math.Clamp(value, 0, 3);
                if (_suit == clamped)
                {
                    return;
                }
                _suit = clamped;
                InvalidateVisual();
            }
        }

        private int _suit;

        /// <summary>The suit's sampled colour, or the hunter's own when there is none.</summary>
        private Color SuitTint(Color baseTint)
        {
            try
            {
                if (!Enum.TryParse(Name2, ignoreCase: true, out MphRead.Hunter which))
                {
                    return baseTint;
                }
                ColorRgba sampled = Mods.HunterSuits.Color(which, _suit);
                return Color.FromRgb(sampled.Red, sampled.Green, sampled.Blue);
            }
            catch (Exception)
            {
                // A model that will not load is the hunter's own colour, not a
                // dead screen. HunterSuits says the same.
                return baseTint;
            }
        }

        /// <summary>
        /// One box's colour under the chosen suit: the ratio it sits at
        /// against the hunter's base tint, applied to the suit's.
        /// </summary>
        private Color Wear(Color box, Color baseTint, Color suit)
        {
            if (suit == baseTint)
            {
                return box;
            }
            static byte Mix(byte value, byte from, byte to)
            {
                double ratio = from == 0 ? 1 : value / (double)from;
                return (byte)Math.Clamp(to * ratio, 0, 255);
            }
            return Color.FromRgb(
                Mix(box.R, baseTint.R, suit.R),
                Mix(box.G, baseTint.G, suit.G),
                Mix(box.B, baseTint.B, suit.B));
        }

        public string Name2
        {
            get => _who;
            set
            {
                _who = value;
                InvalidateVisual();
            }
        }

        private string _who = "Samus";

        /// <summary>Turns on its own until somebody takes hold of it.</summary>
        private double _spin;
        private bool _dragging;
        private double _dragX, _dragBase;
        private readonly System.Diagnostics.Stopwatch _clock =
            System.Diagnostics.Stopwatch.StartNew();
        private TimeSpan _lastFrame;

        private readonly DispatcherTimer _turn;

        public HunterStand()
        {
            Focusable = false;
            Cursor = new Cursor(StandardCursorType.SizeWestEast);
            Avalonia.Media.RenderOptions.SetEdgeMode(this, EdgeMode.Antialias);
            // Driven by a timer that runs only while this is on screen, never
            // by re-invalidating from Render: that spins the render loop at
            // whatever rate it will go, forever, in the game window as well as
            // here -- and it never lets a capture settle on a frame.
            _turn = new DispatcherTimer(TimeSpan.FromMilliseconds(33),
                DispatcherPriority.Background, (_, _) => Beat());
        }

        protected override void OnAttachedToVisualTree(VisualTreeAttachmentEventArgs e)
        {
            base.OnAttachedToVisualTree(e);
            _turn.Start();
        }

        /// <summary>
        /// Once every thirty-three milliseconds while this is in the tree:
        /// tell the engine where the box is, and redraw the boxes if the
        /// engine is not the one filling it.
        ///
        /// <b>The heartbeat, not <c>Render</c>, is what publishes.</b> A
        /// control that is not visible is never rendered, so a rectangle
        /// published from the drawing would go on being true after the drawer
        /// had slid shut -- and the model would be left painted over a panel
        /// that is no longer there. A timer can ask whether it is visible;
        /// a draw that never happens cannot.
        /// </summary>
        private void Beat()
        {
#if MPHREAD_SHELL
            if (!IsEffectivelyVisible)
            {
                Mods.Render.LauncherHunter.Wanted = false;
                return;
            }
            Publish();
#endif
            InvalidateVisual();
        }

#if MPHREAD_SHELL
        /// <summary>
        /// Where this box is in the window, and whether the engine drew a
        /// hunter in it.
        ///
        /// The rectangle has to be published every frame rather than once:
        /// this stand lives in a drawer that slides in, so it is somewhere
        /// different on each of the frames that matter most.
        ///
        /// Two coordinate systems meet here. A control's bounds are in the
        /// surface's points, and the surface is laid out at a scale and then
        /// rasterised at another (see UiSurface); the engine wants fractions
        /// of the window. Going through the top level's own transform is what
        /// keeps those two from having to know about each other.
        /// </summary>
        private bool Publish()
        {
            UiSurface? surface = UiSurface.Current;
            if (surface == null)
            {
                return false;
            }
            // Both corners translated, rather than one corner and a scale.
            // The layout transform that scales the screens lives *inside* the
            // top level, so a point translated into it has already been
            // scaled -- multiplying by the factor as well put the rectangle a
            // third of a window off the right-hand edge, where the scissor
            // clipped it away to nothing.
            Point origin = this.TranslatePoint(new Point(0, 0), surface.Root) ?? new Point(0, 0);
            Point far = this.TranslatePoint(new Point(Bounds.Width, Bounds.Height), surface.Root)
                ?? origin;
            double windowWidth = surface.WindowWidth;
            double windowHeight = surface.WindowHeight;
            if (windowWidth <= 0 || windowHeight <= 0 || far.X <= origin.X || far.Y <= origin.Y)
            {
                return false;
            }
            double left = origin.X;
            double top = origin.Y;
            Mods.Render.LauncherHunter.Wanted = true;
            Mods.Render.LauncherHunter.Hunter =
                Enum.TryParse(_who, ignoreCase: true, out MphRead.Hunter which)
                    ? which : MphRead.Hunter.Samus;
            Mods.Render.LauncherHunter.Suit = _suit;
            Mods.Render.LauncherHunter.Left = (float)(left / windowWidth);
            Mods.Render.LauncherHunter.Top = (float)(top / windowHeight);
            Mods.Render.LauncherHunter.Right = (float)(far.X / windowWidth);
            Mods.Render.LauncherHunter.Bottom = (float)(far.Y / windowHeight);
            // And the scene's own copy, for the frame that *has* a match in
            // it: the results screen's pass reads these and used to be handed
            // them by the HUD panel this replaced. LauncherHunter copies the
            // same four across on the frame with no match, where there is no
            // scene to have read them.
            Scene.PreviewWanted = true;
            Scene.PreviewLeft = Mods.Render.LauncherHunter.Left;
            Scene.PreviewTop = Mods.Render.LauncherHunter.Top;
            Scene.PreviewRight = Mods.Render.LauncherHunter.Right;
            Scene.PreviewBottom = Mods.Render.LauncherHunter.Bottom;
            return Scene.PreviewDrawnLastFrame;
        }
#endif

        protected override void OnDetachedFromVisualTree(VisualTreeAttachmentEventArgs e)
        {
            _turn.Stop();
#if MPHREAD_SHELL
            // The stand has gone; nothing should be drawn under where it was.
            Mods.Render.LauncherHunter.Reset();
#endif
            base.OnDetachedFromVisualTree(e);
        }

        protected override void OnPointerPressed(PointerPressedEventArgs e)
        {
            _dragging = true;
            _dragX = e.GetPosition(this).X;
            _dragBase = _spin;
            e.Pointer.Capture(this);
            e.Handled = true;
            base.OnPointerPressed(e);
        }

        protected override void OnPointerMoved(PointerEventArgs e)
        {
            if (_dragging)
            {
                // A model that turns and cannot be stopped is a screensaver
                // rather than a picker.
                _spin = _dragBase + (e.GetPosition(this).X - _dragX) * 0.014;
                InvalidateVisual();
            }
            base.OnPointerMoved(e);
        }

        protected override void OnPointerReleased(PointerReleasedEventArgs e)
        {
            _dragging = false;
            base.OnPointerReleased(e);
        }

        protected override void OnPointerCaptureLost(PointerCaptureLostEventArgs e)
        {
            _dragging = false;
            base.OnPointerCaptureLost(e);
        }

        public override void Render(DrawingContext context)
        {
            TimeSpan now = _clock.Elapsed;
            double dt = Math.Min(0.1, (now - _lastFrame).TotalSeconds);
            _lastFrame = now;
            if (!_dragging)
            {
                _spin += dt * 0.7;
            }

            double w = Bounds.Width, h = Bounds.Height;
            if (w <= 0 || h <= 0)
            {
                return;
            }
#if MPHREAD_SHELL
            // Did the engine manage the real model last frame? If it did,
            // draw nothing at all -- what goes in this box is painted over
            // the screens afterwards, by LauncherHunter. A model that will
            // not load, or the frames before it has, fall through to the
            // boxes below.
            if (Scene.PreviewDrawnLastFrame)
            {
                return;
            }
#endif
            Hunter hunter = _hunters.TryGetValue(_who, out Hunter found)
                ? found : _hunters["Samus"];

            // The well it stands in, tinted by the hunter: a hunter is a colour
            // before it is a name in a match.
            context.DrawRectangle(new SolidColorBrush(GuiTheme.Ink), null,
                new RoundedRect(new Rect(0, 0, w, h), 7));
            var glow = new RadialGradientBrush
            {
                Center = new RelativePoint(0.5, 0.42, RelativeUnit.Relative),
                GradientOrigin = new RelativePoint(0.5, 0.42, RelativeUnit.Relative),
                RadiusX = new RelativeScalar(0.7, RelativeUnit.Relative),
                RadiusY = new RelativeScalar(0.6, RelativeUnit.Relative),
                GradientStops =
                {
                    new GradientStop(Color.FromArgb(70, hunter.Tint.R, hunter.Tint.G, hunter.Tint.B), 0),
                    new GradientStop(Color.FromArgb(0, hunter.Tint.R, hunter.Tint.G, hunter.Tint.B), 1)
                }
            };
            context.DrawRectangle(glow, null, new RoundedRect(new Rect(0, 0, w, h), 7));

            double cos = Math.Cos(_spin), sin = Math.Sin(_spin);
            const double Pitch = 0.16;
            double cp = Math.Cos(Pitch), sp = Math.Sin(Pitch);
            const double Dist = 170;
            double focal = h * 1.02;
            double cx = w / 2, cy = h * 0.63;

            Color suit = SuitTint(hunter.Tint);
            var quads = new List<(double Z, Point[] P, Color C)>(hunter.Boxes.Length * 6);
            foreach (Box b in hunter.Boxes)
            {
                Color worn = Wear(b.Colour, hunter.Tint, suit);
                double hw = b.W / 2, hh = b.H / 2, hd = b.D / 2;
                var pts = new Point[8];
                var depth = new double[8];
                for (int i = 0; i < 8; i++)
                {
                    double lx = b.X + ((i & 1) != 0 ? hw : -hw);
                    double ly = b.Y + ((i & 4) != 0 ? hh : -hh);
                    double lz = b.Z + ((i & 2) != 0 ? hd : -hd);
                    double rx = lx * cos - lz * sin;
                    double rz0 = lx * sin + lz * cos;
                    double ry = ly * cp - rz0 * sp;
                    double rz = ly * sp + rz0 * cp;
                    double s = focal / (Dist + rz);
                    pts[i] = new Point(cx + rx * s, cy - ry * s);
                    depth[i] = rz;
                }
                for (int f = 0; f < 6; f++)
                {
                    int[] q = _faces[f];
                    double[] n = _normals[f];
                    double nx = n[0] * cos - n[2] * sin;
                    double nz = n[0] * sin + n[2] * cos;
                    double ny = n[1] * cp - nz * sp;
                    // No back-face cull: the painter's sort already hides them,
                    // and culling per box would drop the inside faces of a pose
                    // that is open.
                    double lit = Math.Max(0, nx * _light[0] + ny * _light[1] + nz * _light[2]);
                    quads.Add((
                        (depth[q[0]] + depth[q[1]] + depth[q[2]] + depth[q[3]]) / 4,
                        new[] { pts[q[0]], pts[q[1]], pts[q[2]], pts[q[3]] },
                        Shade(worn, 0.42 + lit * 0.78)));
                }
            }
            quads.Sort((a, b) => b.Z.CompareTo(a.Z));

            var edge = new Pen(new SolidColorBrush(Color.FromArgb(72, 0, 0, 0)), 0.7);
            foreach ((double _, Point[] p, Color c) in quads)
            {
                var geometry = new StreamGeometry();
                using (StreamGeometryContext g = geometry.Open())
                {
                    g.BeginFigure(p[0], true);
                    g.LineTo(p[1]);
                    g.LineTo(p[2]);
                    g.LineTo(p[3]);
                    g.EndFigure(true);
                }
                context.DrawGeometry(new SolidColorBrush(c), edge, geometry);
            }
        }

        private static Color Shade(Color c, double k)
        {
            return Color.FromRgb(
                (byte)Math.Clamp(c.R * k, 0, 255),
                (byte)Math.Clamp(c.G * k, 0, 255),
                (byte)Math.Clamp(c.B * k, 0, 255));
        }
    }
}
#endif
