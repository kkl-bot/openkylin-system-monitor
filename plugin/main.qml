// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick 2.15
import QtQuick.Layouts 1.15
import QtQuick.Controls 2.15 as Controls
import org.ukui.quick.widgets 1.0
import org.ukui.quick.items 1.0
import org.ukui.quick.platform 1.0
import org.ukui.kylinmonitor 1.0

WidgetItem {
    id: root
    objectName: "kylinMonitorWidget"
    Layout.fillWidth: true
    Layout.fillHeight: true
    Accessible.name: qsTr("系统监测")
    Accessible.description: backend.tooltip

    readonly property bool previewMode: typeof widgetPreviewMode !== "undefined" && widgetPreviewMode
    readonly property int instanceId: typeof Widget !== "undefined" ? Widget.instanceId : -1
    readonly property var desktopShell: parent && parent.peonyShell !== undefined ? parent.peonyShell : null
    readonly property var instanceConfig: typeof Widget !== "undefined" ? Widget.config : null
    readonly property var sizeVariants: [
        { id: "small", w: 136, h: 136, grid: [2, 2] },
        { id: "medium", w: 296, h: 136, grid: [4, 2] },
        { id: "large", w: 296, h: 296, grid: [4, 4] }
    ]
    readonly property string sizeMode: width > 180 ? (height > 180 ? "large" : "medium") : "small"
    readonly property bool canRemove: !previewMode && instanceId >= 0 && desktopShell !== null
    property int availabilityRevision: 0
    readonly property bool darkMode: Theme.isDarkTheme
    readonly property color surfaceColor: background.color

    function setting(key, fallback) {
        if (previewMode || !instanceConfig || instanceConfig[key] === undefined || instanceConfig[key] === null)
            return fallback
        return instanceConfig[key]
    }
    function writeSetting(key, value) {
        if (!previewMode && instanceId >= 0 && instanceConfig)
            instanceConfig[key] = value
    }
    function canSwitchToVariant(id) {
        // The revision invalidates action bindings whenever the shared menu becomes active.
        var revision = availabilityRevision
        if (previewMode || !desktopShell || sizeMode === id) return false
        for (var i = 0; i < sizeVariants.length; ++i) {
            var variant = sizeVariants[i]
            if (variant.id === id)
                return desktopShell.canResizeWidgetTo(root, variant.w, variant.h)
        }
        return false
    }
    function switchToVariant(id) {
        if (!canSwitchToVariant(id)) return false
        for (var i = 0; i < sizeVariants.length; ++i) {
            var variant = sizeVariants[i]
            if (variant.id !== id) continue
            // Peony validates both screen bounds and collisions before this point.
            writeSetting("variantId", id)
            writeSetting("width", variant.w)
            writeSetting("height", variant.h)
            width = variant.w
            height = variant.h
            ++availabilityRevision
            return true
        }
        return false
    }
    function requestVariant(id) {
        Qt.callLater(function() { root.switchToVariant(id); root.syncChecks() })
    }
    function syncChecks() {
        smallAction.checked = Qt.binding(function() { return root.sizeMode === "small" })
        mediumAction.checked = Qt.binding(function() { return root.sizeMode === "medium" })
        largeAction.checked = Qt.binding(function() { return root.sizeMode === "large" })
        pauseAction.checked = Qt.binding(function() { return backend.paused })
        fastAction.checked = Qt.binding(function() { return backend.interval === 500 })
        normalAction.checked = Qt.binding(function() { return backend.interval === 1000 })
        slowAction.checked = Qt.binding(function() { return backend.interval === 2000 })
        relaxedAction.checked = Qt.binding(function() { return backend.interval === 5000 })
    }
    function setInterval(value) { writeSetting("interval", value); Qt.callLater(syncChecks) }
    function togglePause() { writeSetting("paused", !backend.paused); Qt.callLater(syncChecks) }
    function cycleGpu() {
        var devices = backend.gpus
        if (devices.length < 2) return
        var selected = -1
        for (var i = 0; i < devices.length; ++i)
            if (devices[i].id === backend.selectedGpu) selected = i
        // Empty or disconnected selection currently displays the first available GPU.
        writeSetting("selectedGpu", devices[(selected < 0 ? 1 : selected + 1) % devices.length].id)
    }
    function removeInstance() {
        if (canRemove) desktopShell.removeWidgetInstance(instanceId)
    }
    function showManager() {
        if (!previewMode && desktopShell) desktopShell.showWidgetManager()
    }

    MonitorBackend {
        id: backend
        objectName: "monitorBackend"
        active: root.visible
        paused: root.setting("paused", false) === true
        interval: Number(root.setting("interval", 1000))
        selectedGpu: String(root.setting("selectedGpu", ""))
    }
    StyleBackground {
        id: background
        anchors.fill: parent
        radius: 12
        paletteRole: Theme.Window
        useStyleTransparency: false
        border.width: root.activeFocus ? 2 : 1
        borderColor: root.activeFocus ? Theme.Highlight : Theme.Mid
        borderAlpha: root.activeFocus ? 1 : 0.35
    }
    ResourceBars {
        anchors.fill: parent
        anchors.margins: root.sizeMode === "small" ? 4 : 8
        resources: backend.resources
        dark: root.darkMode
        paused: backend.paused
        interactive: !root.previewMode
        onDoubleClicked: { root.forceActiveFocus(); backend.openDetails() }
    }
    MouseArea {
        id: hoverArea
        anchors.fill: parent
        acceptedButtons: Qt.NoButton
        hoverEnabled: true
        enabled: !root.previewMode
        Controls.ToolTip.visible: containsMouse
        Controls.ToolTip.delay: 800
        Controls.ToolTip.text: backend.tooltip
    }
    Keys.onReturnPressed: if (!previewMode) backend.openDetails()
    Keys.onSpacePressed: if (!previewMode) togglePause()

    Connections {
        target: typeof Widget !== "undefined" && Widget.container ? Widget.container : null
        function onActiveChanged() {
            ++root.availabilityRevision
            root.syncChecks()
        }
    }
    Component.onCompleted: {
        if (!previewMode) writeSetting("settingsVersion", 1)
        Qt.callLater(root.syncChecks)
    }
    // The host owns popup creation, positioning, dragging, and instance removal.
    // No custom Menu/Popup or window is created inside the shared QML engine.
    Widget.tooltip: backend.tooltip
    Widget.actions: [
        Action { text: qsTr("更多插件"); iconName: "preferences-desktop-apps-symbolic"; enabled: !root.previewMode && root.desktopShell !== null; onTriggered: root.showManager() },
        Action { text: qsTr("查看详细信息"); iconName: "utilities-system-monitor"; enabled: !root.previewMode; onTriggered: backend.openDetails() },
        Action { id: pauseAction; text: qsTr("暂停采样"); checkable: true; checked: backend.paused; enabled: !root.previewMode; onTriggered: root.togglePause() },
        Action { text: qsTr("切换 GPU"); iconName: "video-display"; enabled: !root.previewMode && backend.gpus.length > 1; onTriggered: root.cycleGpu() },
        Action { isSeparator: true },
        Action { id: smallAction; text: qsTr("小尺寸"); iconName: "small-symbolic"; checkable: true; checked: root.sizeMode === "small"; enabled: !root.previewMode && (checked || root.canSwitchToVariant("small")); onTriggered: root.requestVariant("small") },
        Action { id: mediumAction; text: qsTr("中尺寸"); iconName: "medium-symbolic"; checkable: true; checked: root.sizeMode === "medium"; enabled: !root.previewMode && (checked || root.canSwitchToVariant("medium")); onTriggered: root.requestVariant("medium") },
        Action { id: largeAction; text: qsTr("大尺寸"); iconName: "large-symbolic"; checkable: true; checked: root.sizeMode === "large"; enabled: !root.previewMode && (checked || root.canSwitchToVariant("large")); onTriggered: root.requestVariant("large") },
        Action { isSeparator: true },
        Action { id: fastAction; text: qsTr("刷新：0.5 秒"); checkable: true; checked: backend.interval === 500; enabled: !root.previewMode; onTriggered: root.setInterval(500) },
        Action { id: normalAction; text: qsTr("刷新：1 秒"); checkable: true; checked: backend.interval === 1000; enabled: !root.previewMode; onTriggered: root.setInterval(1000) },
        Action { id: slowAction; text: qsTr("刷新：2 秒"); checkable: true; checked: backend.interval === 2000; enabled: !root.previewMode; onTriggered: root.setInterval(2000) },
        Action { id: relaxedAction; text: qsTr("刷新：5 秒"); checkable: true; checked: backend.interval === 5000; enabled: !root.previewMode; onTriggered: root.setInterval(5000) },
        Action { isSeparator: true },
        Action { text: qsTr("移除本插件"); iconName: "zoom-out-symbolic"; enabled: root.canRemove; onTriggered: root.removeInstance() }
    ]
}
