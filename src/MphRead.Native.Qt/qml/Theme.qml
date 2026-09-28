pragma Singleton
import QtQuick

// GuiTheme and Deck, value for value: the numbers are what is kept in step
// with the Avalonia launcher, not the types.
QtObject {
    // GuiTheme's palette.
    readonly property color ink: Qt.rgba(10 / 255, 12 / 255, 16 / 255, 1)
    readonly property color panel: Qt.rgba(18 / 255, 21 / 255, 28 / 255, 1)
    readonly property color panelLight: Qt.rgba(26 / 255, 31 / 255, 41 / 255, 1)
    readonly property color edge: Qt.rgba(38 / 255, 46 / 255, 60 / 255, 1)
    readonly property color panelDeep: Qt.rgba(14 / 255, 17 / 255, 24 / 255, 1)
    readonly property color text: Qt.rgba(230 / 255, 234 / 255, 242 / 255, 1)
    readonly property color textDim: Qt.rgba(138 / 255, 147 / 255, 166 / 255, 1)
    readonly property color accent: Qt.rgba(1, 179 / 255, 71 / 255, 1)
    readonly property color warm: accent
    readonly property color good: "#5f9e72"
    readonly property color warn: "#c08a3e"
    readonly property color bad: "#a85454"
    readonly property color glass: Qt.rgba(18 / 255, 21 / 255, 28 / 255, 220 / 255)
    readonly property color glassLight: Qt.rgba(26 / 255, 31 / 255, 41 / 255, 220 / 255)
    readonly property color scrim: Qt.rgba(10 / 255, 12 / 255, 16 / 255, 196 / 255)

    // Deck faces: a fill and the solid edge under it.
    readonly property var blue: ({ fill: "#2b4e6b", lip: "#16293a" })
    readonly property var brass: ({ fill: "#7a6130", lip: "#3f3118" })
    readonly property var rust: ({ fill: "#6b3636", lip: "#381b1b" })
    readonly property var moss: ({ fill: "#2c5a4e", lip: "#15302a" })
    readonly property var slate: ({ fill: "#232a36", lip: "#12161e" })
    readonly property var step: ({ fill: "#2a3140", lip: "#151a23" })

    // The faces. Pixelify Sans at 400/600/700 are three files of one family.
    readonly property string pixel: pixelRegular.name
    readonly property string mono: monoRegular.name
    readonly property string title: heyNovember.name
    readonly property string prose: roboto.name
    readonly property FontLoader pixelRegular: FontLoader { source: "fonts/PixelifySans-Regular.ttf" }
    readonly property FontLoader pixelSemi: FontLoader { source: "fonts/PixelifySans-SemiBold.ttf" }
    readonly property FontLoader pixelBold: FontLoader { source: "fonts/PixelifySans-Bold.ttf" }
    readonly property FontLoader monoRegular: FontLoader { source: "fonts/JetBrainsMono-Regular.ttf" }
    readonly property FontLoader monoBold: FontLoader { source: "fonts/JetBrainsMono-Bold.ttf" }
    readonly property FontLoader heyNovember: FontLoader { source: "fonts/heyNovember.ttf" }
    readonly property FontLoader roboto: FontLoader { source: "fonts/Roboto-Bold.ttf" }

    // Deck.EmFor on the desktop; the phone curve joins with the mobile heads.
    property bool phone: false
    function emFor(width, height) {
        if (width <= 0)
            return 9
        if (phone)
            return Math.min(13, Math.max(9, width * (width > height ? 0.0155 : 0.0315)))
        return Math.min(15, Math.max(9, width * 0.0115))
    }
    // GuiTheme.PixelSize: the nearest even point, floor of nine.
    function pixelSize(wanted) {
        return Math.max(9, roundEven(wanted / 2) * 2)
    }
    // Banker's rounding, as .NET's Math.Round.
    function roundEven(v) {
        const f = Math.floor(v)
        const d = v - f
        if (d > 0.5) return f + 1
        if (d < 0.5) return f
        return f % 2 === 0 ? f : f + 1
    }
    // UiLayout.Void: the ground's near-black at an alpha.
    function voidAt(alpha) {
        return Qt.rgba(5 / 255, 7 / 255, 10 / 255, roundEven(Math.max(0, Math.min(1, alpha)) * 255) / 255)
    }
    // DeckPaint: CSS brightness() and saturate().
    function brightness(c, k) {
        return Qt.rgba(Math.min(1, c.r * k), Math.min(1, c.g * k), Math.min(1, c.b * k), c.a)
    }
    function saturate(c, s) {
        const l = c.r * 0.2126 + c.g * 0.7152 + c.b * 0.0722
        const f = v => Math.max(0, Math.min(1, l + (v - l) * s))
        return Qt.rgba(f(c.r), f(c.g), f(c.b), c.a)
    }
    // The state behind :focus-visible: the keyboard, not a pointer, is driving.
    property bool keyboardDriving: false
    // Deck.Spring, cubic-bezier(.18, 1.55, .35, 1).
    readonly property var spring: [0.18, 1.55, 0.35, 1, 1, 1]

    // The stand-in play page's own names, until PlayScreen replaces it.
    readonly property color field: step.fill
    readonly property color line: edge
    readonly property color muted: textDim
    readonly property color accentInk: ink
    readonly property int radius: 8
    readonly property int gap: 12
    readonly property string font: pixel
}
