import QtQuick
import Quickshell
import Quickshell.Io

QtObject {
    id: userConfig

    readonly property string userConfigPath: configHome() + "/tide-island/userconfig.json"
    property string defaultWallpaperPath: ""
    property string defaultTlpSudoPassword: ""

    property string wallpaperPath: userConfigString("wallpaperPath", defaultWallpaperPath)
    property real workspaceOverviewWindowRadius: userConfigReal("workspaceOverviewWindowRadius", 12)
    property string iconFontFamily: userConfigString("iconFontFamily", "JetBrainsMono Nerd Font")
    property string textFontFamily: userConfigString("textFontFamily", "Inter Display")
    property string heroFontFamily: userConfigString("heroFontFamily", "Inter Display")
    property string timeFontFamily: userConfigString("timeFontFamily", "Inter Display")
    // Plain-text sudo password for TLP mode switches.
    // Leave empty to use only cached sudo.
    property string tlpSudoPassword: userConfigString("tlpSudoPassword", defaultTlpSudoPassword)

    property var _userConfigFile: FileView {
        id: userConfigFile

        path: userConfig.userConfigPath
        blockLoading: true
        preload: true
        watchChanges: true
        printErrors: false
    }

    function envString(name) {
        const value = Quickshell.env(name);
        return value === undefined || value === null ? "" : String(value);
    }

    function configHome() {
        const xdgConfigHome = envString("XDG_CONFIG_HOME");
        if (xdgConfigHome.length > 0)
            return xdgConfigHome;

        const home = envString("HOME");
        return home.length > 0 ? home + "/.config" : ".";
    }

    function userConfigData() {
        const text = userConfigFile.text();
        if (!text || text.trim().length === 0)
            return ({});

        try {
            const parsed = JSON.parse(text);
            return parsed && typeof parsed === "object" ? parsed : ({});
        } catch (error) {
            console.warn("Tide Island user config is not valid JSON:", error);
            return ({});
        }
    }

    function userConfigString(key, fallback) {
        const value = userConfigData()[key];
        return typeof value === "string" && value.length > 0 ? value : fallback;
    }

    function userConfigReal(key, fallback) {
        const value = userConfigData()[key];
        return typeof value === "number" && isFinite(value) ? value : fallback;
    }

    function userConfigInt(key, fallback) {
        const value = userConfigData()[key];
        return typeof value === "number" && isFinite(value) ? Math.round(value) : fallback;
    }

    function userConfigArray(key, fallback) {
        const value = userConfigData()[key];
        return Array.isArray(value) ? value : fallback;
    }

    function userConfigObject(key, fallback) {
        const value = userConfigData()[key];
        return value && typeof value === "object" && !Array.isArray(value) ? value : fallback;
    }

    // Set these to `0` if you want to disable the in-overview key handling.
    property int overviewCloseKey: userConfigInt("overviewCloseKey", 16777216)
    property int overviewPreviousWorkspaceKey: userConfigInt("overviewPreviousWorkspaceKey", 16777234)
    property int overviewNextWorkspaceKey: userConfigInt("overviewNextWorkspaceKey", 16777236)

    // This registers a Hyprland global shortcut action for the workspace overview.
    property string overviewGlobalShortcutAppid: userConfigString("overviewGlobalShortcutAppid", "quickshell")
    property string overviewGlobalShortcutName: userConfigString("overviewGlobalShortcutName", "dynamic-island-overview")

    // Mouse buttons in this file use simple numbers:
    // 1 = left click, 2 = middle click, 3 = right click.
    // These fields are meant to use the simple numbers above, not Qt's raw enum values.

    // Workspace overview mouse bindings.
    property int workspaceOverviewWorkspaceActivateButton: userConfigInt("workspaceOverviewWorkspaceActivateButton", 1)
    property int workspaceOverviewWindowDragButton: userConfigInt("workspaceOverviewWindowDragButton", 1)
    property int workspaceOverviewWindowFocusButton: userConfigInt("workspaceOverviewWindowFocusButton", 1)
    property int workspaceOverviewWindowCloseButton: userConfigInt("workspaceOverviewWindowCloseButton", 3)

    // Dynamic Island mouse bindings.
    // Supported click actions:
    // "none", "toggleExpandedPlayer", "openExpandedPlayer", "closeExpandedPlayer",
    // "toggleControlCenter", "openControlCenter", "closeControlCenter",
    // "toggleOverview", "openOverview", "closeOverview",
    // "toggleLyrics", "showLyrics", "showTime", "restoreRestingCapsule"
    property int dynamicIslandSwipeButton: userConfigInt("dynamicIslandSwipeButton", 1)
    property int dynamicIslandPrimaryButton: userConfigInt("dynamicIslandPrimaryButton", 1)
    property string dynamicIslandPrimaryAction: userConfigString("dynamicIslandPrimaryAction", "toggleExpandedPlayer")
    property int dynamicIslandSecondaryButton: userConfigInt("dynamicIslandSecondaryButton", 3)
    property string dynamicIslandSecondaryAction: userConfigString("dynamicIslandSecondaryAction", "toggleControlCenter")
    // Supported built-in left swipe items:
    // "time", "date", "battery", "volume", "brightness", "workspace", "cpu", "ram", "cava"
    property var dynamicIslandLeftSwipeItems: userConfigArray("dynamicIslandLeftSwipeItems", ["cava", "battery"])

    property var controlCenterIcons: userConfigObject("controlCenterIcons", ({
        "charging": "",
        "brightness": "󰃟",
        "volume": "󰕾"
    }))

    property var statusIcons: userConfigObject("statusIcons", ({
        "default": "🎧",
        "notification": "",
        "volume": "󰕾",
        "mute": "󰝟",
        "brightnessLow": "󰃞",
        "brightnessMedium": "󰃟",
        "brightnessHigh": "󰃠",
        "charging": "",
        "discharging": "",
        "cpu": "󰍛",
        "ram": "󰘚",
        "capsLockOn": "",
        "capsLockOff": "",
        "bluetooth": "󰋋"
    }))

    function mouseButton(button) {
        switch (button) {
        case 1:
            return Qt.LeftButton;
        case 2:
            return Qt.MiddleButton;
        case 3:
            return Qt.RightButton;
        default:
            return typeof button === "number" ? button : Qt.NoButton;
        }
    }

    function mouseButtonsMask(buttons) {
        if (buttons === undefined || buttons === null)
            return Qt.NoButton;

        if (Array.isArray(buttons)) {
            let mask = Qt.NoButton;
            for (let index = 0; index < buttons.length; index++)
                mask |= mouseButton(buttons[index]);
            return mask;
        }

        return mouseButton(buttons);
    }
}
