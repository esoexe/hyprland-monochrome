// ═══════════════════════════════════════════════════════════════════════════
//  OpenCode Design System — Firefox / LibreWolf / Zen user.js overrides
//  Enables `chrome/userChrome.css` + `userContent.css`, Wayland acceleration,
//  dark prefers-color-scheme, and compact OpenCode chrome density.
// ═══════════════════════════════════════════════════════════════════════════

user_pref("toolkit.legacyUserProfileCustomizations.stylesheets", true);
user_pref("browser.uidensity", 1);
user_pref("layout.css.prefers-color-scheme.content-override", 0);
user_pref("ui.systemUsesDarkTheme", 1);
user_pref("browser.theme.content-theme", 0);
user_pref("browser.theme.toolbar-theme", 0);
user_pref("extensions.pocket.enabled", false);
user_pref("browser.newtabpage.activity-stream.showSponsored", false);
user_pref("browser.newtabpage.activity-stream.showSponsoredTopSites", false);
user_pref("widget.use-xdg-desktop-portal.file-picker", 1);
