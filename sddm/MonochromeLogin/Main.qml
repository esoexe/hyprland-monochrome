import QtQuick 2.0

Item {
    id: root
    property var screenGeometry: screenModel.geometry(screenModel.primary)
    width: screenGeometry.width > 0 ? screenGeometry.width : 1920
    height: screenGeometry.height > 0 ? screenGeometry.height : 1080
    focus: true

    property bool busy: false
    property bool hasPreviousSession: sessionModel.lastIndex >= 0
    property int sessionIndex: hasPreviousSession ? sessionModel.lastIndex : 0
    property string statusText: ""
    property real panelScale: 0.96

    function attemptLogin() {
        if (busy || usernameInput.text.length === 0 || passwordInput.text.length === 0)
            return
        busy = true
        statusText = "Signing in"
        sddm.login(usernameInput.text, passwordInput.text, sessionIndex)
    }

    Component.onCompleted: {
        if (userModel.lastUser && userModel.lastUser.length > 0)
            usernameInput.text = userModel.lastUser
        entrance.start()
        if (usernameInput.text.length > 0)
            passwordInput.forceActiveFocus()
        else
            usernameInput.forceActiveFocus()
    }

    AnimatedImage {
        id: wallpaper
        anchors.fill: parent
        source: "assets/orbital.gif"
        fillMode: Image.PreserveAspectCrop
        playing: true
        cache: true
        asynchronous: true
        opacity: 0.46
    }

    Rectangle {
        anchors.fill: parent
        gradient: Gradient {
            GradientStop { position: 0.0; color: "#050505" }
            GradientStop { position: 0.48; color: "#090909" }
            GradientStop { position: 1.0; color: "#030303" }
        }
        opacity: 0.72
    }

    Item {
        id: orbitField
        width: Math.min(root.width * 0.72, 1100)
        height: width
        x: root.width * 0.22 - width / 2
        y: root.height / 2 - height / 2
        opacity: 0.38

        Repeater {
            model: 4
            Rectangle {
                property real sizeFactor: 0.40 + index * 0.19
                width: orbitField.width * sizeFactor
                height: width
                radius: width / 2
                anchors.centerIn: parent
                color: "transparent"
                border.width: index === 0 ? 2 : 1
                border.color: index % 2 === 0 ? "#e8e8e8" : "#827f7f"
                opacity: 0.28 + index * 0.08
                rotation: index * 21

                Rectangle {
                    width: 7
                    height: 7
                    radius: 4
                    color: "#ffffff"
                    x: parent.width * 0.5 - width / 2
                    y: 2
                    opacity: 0.74
                }

                RotationAnimation on rotation {
                    from: index * 21
                    to: index * 21 + (index % 2 === 0 ? 360 : -360)
                    duration: 32000 + index * 11000
                    loops: Animation.Infinite
                }
            }
        }
    }

    Rectangle {
        width: 2
        height: Math.min(root.height * 0.58, 620)
        anchors.left: parent.left
        anchors.leftMargin: root.width * 0.09
        anchors.verticalCenter: parent.verticalCenter
        color: "#ffffff"
        opacity: 0.15
    }

    Column {
        x: root.width > 920 ? root.width * 0.115 : 32
        y: root.height * 0.31
        width: root.width > 920 ? Math.min(root.width * 0.40, 620) : root.width - 64
        spacing: 18
        opacity: root.width > 920 ? 0.92 : 0.0
        visible: root.width > 920

        Text {
            text: "HYPRLAND  /  MONOCHROME"
            color: "#a6a2a2"
            font.family: "monospace"
            font.pixelSize: 13
            font.letterSpacing: 3
        }
        Text {
            text: "A quieter\nway to work."
            color: "#ffffff"
            font.family: "sans-serif"
            font.pixelSize: Math.max(42, Math.min(68, root.width * 0.044))
            font.weight: Font.DemiBold
            lineHeight: 1.08
        }
        Text {
            width: Math.min(parent.width, 470)
            text: "A focused Wayland session, shaped around calm motion, clear space, and tools that stay out of your way."
            color: "#aaa6a6"
            font.family: "sans-serif"
            font.pixelSize: 16
            wrapMode: Text.WordWrap
            lineHeight: 1.35
        }
        Text {
            id: clock
            text: Qt.formatDateTime(new Date(), "HH:mm")
            color: "#d7d4d4"
            font.family: "monospace"
            font.pixelSize: 14
            font.letterSpacing: 2
        }
    }

    Rectangle {
        id: panel
        width: Math.min(420, root.width - 40)
        height: 492
        x: root.width > 920 ? root.width * 0.69 - width / 2 : (root.width - width) / 2
        y: (root.height - height) / 2
        radius: 18
        color: "#0b0b0b"
        opacity: 0.95
        border.width: 1
        border.color: "#555151"
        scale: root.panelScale

        Behavior on scale {
            NumberAnimation { duration: 900; easing.type: Easing.OutCubic }
        }

        Rectangle {
            anchors.fill: parent
            anchors.margins: 1
            radius: 17
            color: "transparent"
            border.width: 1
            border.color: "#ffffff"
            opacity: 0.04
        }

        Column {
            anchors.fill: parent
            anchors.margins: 34
            spacing: 0

            Row {
                width: parent.width
                height: 34
                spacing: 10

                Rectangle {
                    width: 9
                    height: 9
                    radius: 5
                    anchors.verticalCenter: parent.verticalCenter
                    color: "#ffffff"
                    opacity: pulseDot.opacity

                    SequentialAnimation on opacity {
                        id: pulseDot
                        loops: Animation.Infinite
                        NumberAnimation { from: 0.45; to: 1.0; duration: 1100; easing.type: Easing.InOutSine }
                        NumberAnimation { from: 1.0; to: 0.45; duration: 1100; easing.type: Easing.InOutSine }
                    }
                }

                Text {
                    text: "SECURE SESSION"
                    color: "#b8b4b4"
                    font.family: "monospace"
                    font.pixelSize: 11
                    font.letterSpacing: 2
                    anchors.verticalCenter: parent.verticalCenter
                }

                Item { width: 1; height: 1 }

                Text {
                    text: "v5.3"
                    color: "#777272"
                    font.family: "monospace"
                    font.pixelSize: 11
                    anchors.verticalCenter: parent.verticalCenter
                }
            }

            Text {
                text: "Welcome back"
                color: "#ffffff"
                font.family: "sans-serif"
                font.pixelSize: 31
                font.weight: Font.DemiBold
                anchors.left: parent.left
                anchors.leftMargin: 0
                anchors.topMargin: 24
            }

            Text {
                text: "Sign in to continue to your desktop."
                color: "#918d8d"
                font.family: "sans-serif"
                font.pixelSize: 13
                anchors.topMargin: 5
            }

            Item { width: 1; height: 20 }

            Text {
                text: "USER NAME"
                color: "#aaa6a6"
                font.family: "monospace"
                font.pixelSize: 10
                font.letterSpacing: 1.5
            }

            Rectangle {
                width: parent.width
                height: 48
                radius: 8
                color: "#111010"
                border.width: usernameInput.activeFocus ? 1 : 1
                border.color: usernameInput.activeFocus ? "#ffffff" : "#353131"

                TextInput {
                    id: usernameInput
                    anchors.fill: parent
                    anchors.leftMargin: 13
                    anchors.rightMargin: 13
                    verticalAlignment: TextInput.AlignVCenter
                    color: "#f5f3f3"
                    selectionColor: "#bcb8b8"
                    selectedTextColor: "#090909"
                    font.family: "monospace"
                    font.pixelSize: 14
                    activeFocusOnTab: true
                    selectByMouse: true
                    text: ""
                    KeyNavigation.tab: passwordInput
                }

                Text {
                    anchors.left: parent.left
                    anchors.leftMargin: 13
                    anchors.verticalCenter: parent.verticalCenter
                    text: "Linux user"
                    color: "#686464"
                    font.family: "monospace"
                    font.pixelSize: 13
                    visible: usernameInput.text.length === 0 && !usernameInput.activeFocus
                }
            }

            Item { width: 1; height: 14 }

            Text {
                text: "PASSWORD"
                color: "#aaa6a6"
                font.family: "monospace"
                font.pixelSize: 10
                font.letterSpacing: 1.5
            }

            Rectangle {
                width: parent.width
                height: 48
                radius: 8
                color: "#111010"
                border.width: 1
                border.color: passwordInput.activeFocus ? "#ffffff" : "#353131"

                TextInput {
                    id: passwordInput
                    anchors.fill: parent
                    anchors.leftMargin: 13
                    anchors.rightMargin: 13
                    verticalAlignment: TextInput.AlignVCenter
                    color: "#f5f3f3"
                    selectionColor: "#bcb8b8"
                    selectedTextColor: "#090909"
                    font.family: "monospace"
                    font.pixelSize: 14
                    echoMode: TextInput.Password
                    activeFocusOnTab: true
                    selectByMouse: true
                    KeyNavigation.tab: loginButton
                    Keys.onReturnPressed: root.attemptLogin()
                    Keys.onEnterPressed: root.attemptLogin()
                }

                Text {
                    anchors.left: parent.left
                    anchors.leftMargin: 13
                    anchors.verticalCenter: parent.verticalCenter
                    text: "Password"
                    color: "#686464"
                    font.family: "monospace"
                    font.pixelSize: 13
                    visible: passwordInput.text.length === 0 && !passwordInput.activeFocus
                }
            }

            Item { width: 1; height: 13 }

            Rectangle {
                id: loginButton
                width: parent.width
                height: 50
                radius: 8
                color: busy ? "#c9c6c6" : "#f4f3f3"
                border.width: 1
                border.color: "#ffffff"
                KeyNavigation.tab: usernameInput
                Keys.onReturnPressed: root.attemptLogin()
                Keys.onEnterPressed: root.attemptLogin()

                Text {
                    anchors.centerIn: parent
                    text: root.busy ? "SIGNING IN" : "SIGN IN"
                    color: "#080808"
                    font.family: "monospace"
                    font.pixelSize: 12
                    font.weight: Font.DemiBold
                    font.letterSpacing: 2
                }

                MouseArea {
                    anchors.fill: parent
                    enabled: !root.busy
                    hoverEnabled: true
                    onEntered: loginButton.color = "#ffffff"
                    onExited: loginButton.color = root.busy ? "#c9c6c6" : "#f4f3f3"
                    onClicked: root.attemptLogin()
                }
            }

            Item { width: 1; height: 12 }

            Text {
                id: statusMessage
                width: parent.width
                height: 20
                text: root.statusText
                horizontalAlignment: Text.AlignHCenter
                color: root.statusText === "Incorrect password or user." ? "#f0b5b5" : "#969292"
                font.family: "monospace"
                font.pixelSize: 11
            }

            Item { width: 1; height: 2 }

            Row {
                width: parent.width
                height: 24
                spacing: 12
                anchors.horizontalCenter: parent.horizontalCenter

                Text {
                    text: "SESSION  /  LAST USED"
                    color: "#777272"
                    font.family: "monospace"
                    font.pixelSize: 9
                    font.letterSpacing: 1
                    anchors.verticalCenter: parent.verticalCenter
                }
                Text {
                    text: root.hasPreviousSession ? "Last used" : "System default"
                    color: "#c8c4c4"
                    font.family: "monospace"
                    font.pixelSize: 10
                    anchors.verticalCenter: parent.verticalCenter
                }
            }

            Item { width: 1; height: 10 }

            Rectangle {
                width: parent.width
                height: 1
                color: "#302d2d"
            }

            Row {
                width: parent.width
                height: 38
                spacing: 18
                anchors.horizontalCenter: parent.horizontalCenter

                Text {
                    text: sddm.canReboot ? "RESTART" : ""
                    color: restartMouse.containsMouse ? "#ffffff" : "#878282"
                    font.family: "monospace"
                    font.pixelSize: 10
                    font.letterSpacing: 1
                    verticalAlignment: Text.AlignVCenter
                    MouseArea {
                        id: restartMouse
                        anchors.fill: parent
                        hoverEnabled: true
                        enabled: sddm.canReboot
                        onClicked: sddm.reboot()
                    }
                }

                Text {
                    text: sddm.canPowerOff ? "POWER OFF" : ""
                    color: powerMouse.containsMouse ? "#ffffff" : "#878282"
                    font.family: "monospace"
                    font.pixelSize: 10
                    font.letterSpacing: 1
                    verticalAlignment: Text.AlignVCenter
                    MouseArea {
                        id: powerMouse
                        anchors.fill: parent
                        hoverEnabled: true
                        enabled: sddm.canPowerOff
                        onClicked: sddm.powerOff()
                    }
                }
            }
        }
    }

    Connections {
        target: sddm
        onLoginFailed: {
            root.busy = false
            root.statusText = "Incorrect password or user."
            passwordInput.text = ""
            passwordInput.forceActiveFocus()
        }
        onLoginSucceeded: {
            root.busy = false
            root.statusText = ""
        }
    }

    Timer {
        interval: 30000
        running: true
        repeat: true
        onTriggered: clock.text = Qt.formatDateTime(new Date(), "HH:mm")
    }

    NumberAnimation {
        id: entrance
        target: root
        property: "panelScale"
        from: 0.96
        to: 1.0
        duration: 720
        easing.type: Easing.OutCubic
    }
}
