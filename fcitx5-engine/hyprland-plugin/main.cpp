// xtype-inline — Hyprland plugin for XType's popup ghost style.
//
// Hyprland places input-method popups *below* the text caret (see
// CInputPopup::updateBox). With XType's popup ghost style that popup holds the
// suggestion, so moving it onto the caret's own line — starting right after the
// caret and vertically centred on it — makes it read as inline ghost text.
//
// Every consumer of the popup position (rendering, damage, pointer hit-tests)
// goes through CInputPopup::globalBox(), so hooking that one function is
// enough. isVecInPopup() is hooked too so the popup is click-through and never
// steals clicks from the text underneath it.
//
// The caret comes from the app (text-input cursor rectangle). When the app
// reports none, or one that can't be right (no height, outside its own
// surface), the popup is hidden instead of drawn somewhere unrelated.
//
// Debug: with ~/.local/share/xtype/.debug_verbose present when the plugin
// loads, every change of caret / surface / popup box is appended to
// ~/.local/share/xtype/hyprland-inline.log.
#include <hyprland/src/plugins/PluginAPI.hpp>
#include <hyprland/src/plugins/HookSystem.hpp>
#include <hyprland/src/managers/input/InputManager.hpp>
#include <hyprland/src/managers/input/InputMethodPopup.hpp>
#include <hyprland/src/managers/input/TextInput.hpp>
#include <hyprland/src/desktop/view/WLSurface.hpp>

#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <format>
#include <stdexcept>
#include <string>

inline HANDLE         PHANDLE           = nullptr;
static CFunctionHook* g_globalBoxHook   = nullptr;
static CFunctionHook* g_isVecInPopupHook = nullptr;

using GlobalBoxFn    = CBox (*)(CInputPopup*);
using IsVecInPopupFn = bool (*)(CInputPopup*, const Vector2D&);

static FILE*       g_debugLog = nullptr;   // open only in debug mode
static std::string g_lastLogged;

static void debugLog(const std::string& line) {
    if (!g_debugLog || line == g_lastLogged)
        return;   // globalBox runs every frame; log changes only
    g_lastLogged = line;
    std::fprintf(g_debugLog, "%s\n", line.c_str());
    std::fflush(g_debugLog);
}

static std::string str(const CBox& b) {
    return std::format("{:.0f},{:.0f} {:.0f}x{:.0f}", b.x, b.y, b.w, b.h);
}

// A box far outside every monitor: the popup is still mapped but not visible.
static CBox offscreen(CBox box) {
    box.x = -100000;
    box.y = -100000;
    return box;
}

static CBox hkGlobalBox(CInputPopup* self) {
    CBox box = reinterpret_cast<GlobalBoxFn>(g_globalBoxHook->m_original)(self);

    const auto TI = g_pInputManager->m_relay.getFocusedTextInput();
    if (!TI)
        return box;   // not attached to a text input: leave Hyprland's placement
    if (!TI->hasCursorRectangle()) {
        if (g_debugLog) debugLog("hide: app reports no caret");
        return offscreen(box);
    }

    const auto SURF = Desktop::View::CWLSurface::fromResource(TI->focusedSurface());
    if (!SURF)
        return box;
    const auto PARENT = SURF->getSurfaceBoxGlobal();
    if (!PARENT)
        return box;

    const CBox CARET = TI->cursorBox();   // surface-local
    const bool plausible = CARET.h > 0 && CARET.x >= 0 && CARET.y >= 0 &&
                           CARET.x <= PARENT->w && CARET.y + CARET.h <= PARENT->h;
    if (!plausible) {
        if (g_debugLog) debugLog("hide: caret " + str(CARET) + " outside surface " + str(*PARENT));
        return offscreen(box);
    }

    box.x = PARENT->x + CARET.x + CARET.w;
    box.y = PARENT->y + CARET.y + (CARET.h - box.h) / 2.0;
    if (g_debugLog) debugLog("caret " + str(CARET) + "  surface " + str(*PARENT) + "  popup " + str(box));
    return box;
}

static bool hkIsVecInPopup(CInputPopup*, const Vector2D&) {
    return false;
}

static void* findMember(const std::string& name, const std::string& qualified) {
    for (const auto& m : HyprlandAPI::findFunctionsByName(PHANDLE, name))
        if (m.demangled.find(qualified) != std::string::npos)
            return m.address;
    return nullptr;
}

APICALL EXPORT std::string PLUGIN_API_VERSION() {
    return HYPRLAND_API_VERSION;
}

APICALL EXPORT PLUGIN_DESCRIPTION_INFO PLUGIN_INIT(HANDLE handle) {
    PHANDLE = handle;

    if (std::string(__hyprland_api_get_hash()) != __hyprland_api_get_client_hash()) {
        HyprlandAPI::addNotification(PHANDLE, "[xtype-inline] Version mismatch: rebuild against the running Hyprland",
                                     CHyprColor{1.0, 0.2, 0.2, 1.0}, 5000);
        throw std::runtime_error("[xtype-inline] version mismatch");
    }

    void* globalBox    = findMember("globalBox", "CInputPopup::globalBox");
    void* isVecInPopup = findMember("isVecInPopup", "CInputPopup::isVecInPopup");
    if (!globalBox || !isVecInPopup) {
        HyprlandAPI::addNotification(PHANDLE, "[xtype-inline] CInputPopup functions not found; plugin inactive",
                                     CHyprColor{1.0, 0.2, 0.2, 1.0}, 5000);
        throw std::runtime_error("[xtype-inline] symbols not found");
    }

    g_globalBoxHook    = HyprlandAPI::createFunctionHook(PHANDLE, globalBox, reinterpret_cast<void*>(&hkGlobalBox));
    g_isVecInPopupHook = HyprlandAPI::createFunctionHook(PHANDLE, isVecInPopup, reinterpret_cast<void*>(&hkIsVecInPopup));
    if (!g_globalBoxHook->hook() || !g_isVecInPopupHook->hook())
        throw std::runtime_error("[xtype-inline] failed to hook");

    if (const char* home = std::getenv("HOME")) {
        const std::filesystem::path dir = std::filesystem::path(home) / ".local/share/xtype";
        std::error_code ec;
        if (std::filesystem::exists(dir / ".debug_verbose", ec))
            g_debugLog = std::fopen((dir / "hyprland-inline.log").c_str(), "a");
    }

    HyprlandAPI::addNotification(PHANDLE, "[xtype-inline] loaded: input-method popups now sit on the caret line",
                                 CHyprColor{0.52, 0.44, 0.9, 1.0}, 3000);
    return {"xtype-inline", "Places input-method popups on the caret line (XType popup suggestions)", "XType", "0.1"};
}

APICALL EXPORT void PLUGIN_EXIT() {
    // Hooks created through HyprlandAPI are removed automatically on unload.
    if (g_debugLog) {
        std::fclose(g_debugLog);
        g_debugLog = nullptr;
    }
}
