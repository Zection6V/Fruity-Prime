import FruityPrime.Launcher
import QtQuick

// The menus' root: the front screen or the pause menu under a stack of
// screens, as StartScreen and InGameMenu keep one. Only the top of the stack
// exists; a closed menu leaves nothing in the scene graph.
Item {
    id: root
    // [{ url, props }], the last one showing.
    property var stack: []
    readonly property bool stacked: stack.length > 0

    // The captures hold everything still and switch the phone curve on.
    property bool still: ShellHost.backdropSuspended
    property bool phone: Qt.platform.os === "android" || Qt.platform.os === "ios"
    // Qt on Android hands this view a surface in physical pixels (the device
    // pixel ratio is 1), so a layout authored in the desktop's 1/96-inch
    // units comes out at a fraction of its size on a 440 dpi phone. Everything
    // below is laid out in a stage of the size the screen has in 1/160-inch dp
    // and scaled up to fill the view; where Qt already reports a ratio, or off
    // Android, the factor is 1 and the stage is the view.
    readonly property real uiScale: {
        if (Qt.platform.os !== "android" || Screen.devicePixelRatio > 1.01)
            return 1
        const dpi = Screen.pixelDensity * 25.4
        return dpi > 0 ? Math.min(4, Math.max(1, dpi / 160)) : 1
    }
    Item {
        id: stage
        width: root.width / root.uiScale
        height: root.height / root.uiScale
        scale: root.uiScale
        transformOrigin: Item.TopLeft
    }
    // What the base screen is asked to show (the end panel's tab).
    property var baseProps: ({})
    Binding { target: Theme; property: "still"; value: root.still }
    Binding { target: Theme; property: "phone"; value: root.phone }
    Binding { target: Theme; property: "em"; value: Theme.emFor(stage.width, stage.height) }

    function push(url, props) {
        stack = stack.concat([{ url: url, props: props || {} }])
    }
    function pop() {
        stack = stack.slice(0, stack.length - 1)
        if (!stacked && ShellHost.page === "pause" && !base.item)
            ShellHost.resume()
    }
    function reset() {
        stack = []
    }
    // Replace everything with one screen: the captures' way in.
    function only(url, props) {
        stack = [{ url: url, props: props || {} }]
    }

    // StartScreen's ways out of the front screen.
    function openPlay() {
        if (!ShellHost.gameFilesReady) {
            openSetup()
            return
        }
        push("PlayPage.qml", { face: 0 })
    }
    function openSettings(overGame) {
        push("SettingsPage.qml", { overGame: !!overGame })
    }
    function openSetup() {
        push("SetupPage.qml", {})
    }
    function openCreateServer() {
        push("CreateServerPage.qml", {})
    }
    function openLobby() {
        push("LobbyPage.qml", {})
    }
    // The lobby closed: back to the screen under it, told why.
    function lobbyClosed(reason) {
        stack = stack.slice(0, stack.length - 1)
        if (stack.length > 0 && reason.length > 0) {
            const last = stack[stack.length - 1]
            stack = stack.slice(0, stack.length - 1).concat([{ url: last.url, props: Object.assign({}, last.props, { endedReason: reason }) }])
        }
    }
    function openVote() {
        const why = ShellHost.whyNotVoting()
        if (why.length > 0) {
            ShellHost.systemMessage(why)
            ShellHost.resume()
            return
        }
        push("PlayPage.qml", { face: 4, overGame: true })
    }
    function askToQuit() {
        push("ConfirmPage.qml", { question: "Quit " + ShellHost.brand + "?", yesAction: () => ShellHost.quit() })
    }

    Connections {
        target: ShellHost
        function onBackRequested() {
            if (root.stacked) root.pop()
            else ShellHost.quit()
        }
        function onPageChanged() {
            root.reset()
            if (ShellHost.page === "front" && !ShellHost.gameFilesReady)
                root.openSetup()
        }
        function onLobbyOpened() { root.openLobby() }
        function onKeyboardDriving() { Theme.keyboardDriving = true }
        function onScreenRequested(url, props) {
            if (url.length > 0) {
                root.only(url, props)
            } else {
                root.reset()
                root.baseProps = props
            }
        }
    }

    Loader {
        id: base
        parent: stage
        anchors.fill: parent
        visible: !root.stacked
        enabled: visible
        focus: !root.stacked
        sourceComponent: ShellHost.page === "front" ? start
                       : ShellHost.page === "pause" ? pause
                       : ShellHost.page === "end" ? end
                       : null
        onLoaded: if (!root.stacked) item.forceActiveFocus()
    }
    Component {
        id: start
        StartPage {
            focus: true
            onPlay: root.openPlay()
            onSettings: root.openSettings(false)
            onQuit: root.askToQuit()
        }
    }
    Component {
        id: pause
        PausePage {
            focus: true
            onSettings: root.openSettings(true)
            onVoteMap: root.openVote()
        }
    }
    Component { id: end; EndPanel { focus: true; hunterTab: !!root.baseProps.hunterTab } }

    Loader {
        id: top
        parent: stage
        anchors.fill: parent
        focus: root.stacked
        readonly property var entry: root.stacked ? root.stack[root.stack.length - 1] : null
        onEntryChanged: {
            if (entry) {
                const props = Object.assign({ nav: root }, entry.props)
                setSource(entry.url, props)
            } else {
                source = ""
                if (base.item)
                    base.item.forceActiveFocus()
            }
        }
        onLoaded: item.forceActiveFocus()
    }

    ControllerKeyboard {
        id: controllerKeyboard
        parent: stage
        Component.onCompleted: Theme.keyboard = controllerKeyboard
    }

    // Until the host says the front screen is ready, something opaque is
    // drawn: a host that is still starting, or that failed, must never look
    // like a black window. Plain primitives only -- no images, no settings,
    // no game files -- so it draws whatever else has not loaded yet.
    Rectangle {
        id: boot
        parent: stage
        anchors.fill: parent
        visible: ShellHost.startupState !== "FrontReady"
        color: "#10141c"
        z: 1000
        // Swallow input meant for the pages underneath.
        MouseArea { anchors.fill: parent; enabled: boot.visible }
        Column {
            anchors.centerIn: parent
            width: Math.min(parent.width - 48, 640)
            spacing: 16
            Text {
                width: parent.width
                horizontalAlignment: Text.AlignHCenter
                color: "#e8ecf4"
                font.pixelSize: Math.max(18, Math.round(boot.height / 24))
                text: ShellHost.startupState === "Failed"
                    ? ShellHost.brand + " could not start"
                    : "Starting " + ShellHost.brand + "…"
            }
            Text {
                width: parent.width
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.Wrap
                color: "#a8b0c0"
                font.pixelSize: Math.max(12, Math.round(boot.height / 40))
                visible: text.length > 0
                text: ShellHost.startupState === "Failed" ? ShellHost.startupError : ""
            }
        }
    }
}
