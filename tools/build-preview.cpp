// ═══════════════════════════════════════════════════════════════════════════
//  tools/build-preview.cpp — Build self-contained docs/preview.html in C++20
//
//  Reads the live configuration (`binds.lua`, `user.lua`, `waybar/config.jsonc`,
//  `rofi/theme.rasi`, `kitty/theme.conf`, `starship.toml`, `packages.txt`,
//  `aur-packages.txt`, `docs/preview.svg`, and `wallpapers/`) and emits a
//  zero-external-asset HTML preview with inline base64 JPEG thumbnails and
//  the 60 FPS interactive Velocity & Liquid Glass desktop simulator.
//
//    ./bin/build-preview            # write docs/preview.html
//    ./bin/build-preview --check    # exit 1 if out of date (ignores date stamp)
// ═══════════════════════════════════════════════════════════════════════════

#include "../include/keybind_table.hpp"
#include "../include/mono_config.hpp"
#include "../include/mono_image.hpp"
#include "../wallpapers/generate.hpp"

#include <algorithm>
#include <ctime>
#include <filesystem>
#include <iostream>
#include <map>
#include <regex>
#include <set>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace fs = std::filesystem;
using mono_cfg::html_escape;

namespace {

std::string today_iso() {
    std::time_t now = std::time(nullptr);
    std::tm tm_val{};
    localtime_r(&now, &tm_val);
    char buf[32]{};
    std::strftime(buf, sizeof(buf), "%Y-%m-%d", &tm_val);
    return buf;
}

std::string swatch_html(const std::string& name, const std::string& value, const std::string& label = "") {
    return "<div class=\"sw\"><div class=\"chip\" style=\"background:" + value + "\"></div>"
           "<div class=\"meta\"><b>" + html_escape(name) + "</b><span>" + html_escape(value) +
           (label.empty() ? "" : (" \xc2\xb7 " + html_escape(label))) + "</span></div></div>";
}

std::pair<std::string, std::string> build_wallpaper_cards(const fs::path& repo) {
    fs::path wall_dir = repo / "wallpapers";
    std::string hero_uri;
    std::string items;

    for (const auto& d : mono_wall::DESIGNS) {
        std::string stem = d.stem;
        if (!fs::exists(wall_dir / (stem + ".jpg"))) continue;

        auto thumb = d.fn(420, 236);
        auto jpg = mono_img::encode_jpeg_mem(thumb, 82);
        std::string uri = "data:image/jpeg;base64," + mono_img::base64_encode(jpg);

        if (stem == "mono-11" || hero_uri.empty()) hero_uri = uri;

        std::string badge = fs::exists(wall_dir / (stem + "-16k.jpg"))
            ? "16K \xc2\xb7 132.7 MP"
            : (fs::exists(wall_dir / (stem + "-4k.jpg")) ? "4K \xc2\xb7 UHD" : "1080p \xc2\xb7 scalable");
        std::string active = (stem == "mono-11") ? " active" : "";

        items += "<button type=\"button\" class=\"wcard" + active + "\" data-uri=\"" + uri +
                 "\" data-title=\"" + html_escape(stem) + " \xe2\x80\x94 " + html_escape(d.title) +
                 "\" data-desc=\"" + html_escape(d.subtitle) + " (" + html_escape(badge) + ")\">"
                 "<img src=\"" + uri + "\" alt=\"" + html_escape(stem) + "\">"
                 "<div class=\"wmeta\"><b>" + html_escape(stem) + " \xc2\xb7 " + html_escape(d.title) +
                 "</b><span>" + html_escape(badge) + "</span></div></button>";
    }
    return {hero_uri, items};
}

std::string build_bar_html(const mono_cfg::JsonVal& config) {
    static const std::map<std::string, std::pair<std::string, std::string>> SAMPLE = {
        {"custom/launcher",     {"\xf3\xb0\xa3\x87", ""}},
        {"hyprland/workspaces", {"", ""}},
        {"hyprland/submap",     {"RESIZE", ""}},
        {"hyprland/window",     {"", "~/projects/monochrome \xe2\x80\x94 kitty"}},
        {"mpris",               {"\xf3\xb0\x8e\x88", "Midnight City \xe2\x80\x94 M83"}},
        {"custom/sysgraph",     {"\xe2\x96\x81\xe2\x96\x82\xe2\x96\x84\xe2\x96\x86\xe2\x96\x83\xe2\x96\x82", "14%"}},
        {"cpu",                 {"", "12%"}},
        {"memory",              {"", "42%"}},
        {"temperature",         {"", "48\xc2\xb0" "C"}},
        {"custom/updates",      {"\xf3\xb0\x9a\xb0", "7"}},
        {"network",             {"\xf3\xb0\x96\xa9", "wlan0"}},
        {"bluetooth",           {"\xf3\xb0\x82\xaf", ""}},
        {"pulseaudio",          {"\xf3\xb0\x95\xbe", "65%"}},
        {"battery",             {"\xf3\xb0\x81\xb9", "88%"}},
        {"custom/perf",         {"\xe2\x9a\xa1", "TURBO"}},
        {"custom/focus",        {"\xe2\x97\x8e", "24:58"}},
        {"custom/recording",    {"\xe2\x97\x8f", "00:42"}},
        {"tray",                {"", ""}},
        {"clock",               {"", "21:42"}},
        {"custom/settings",     {"\xe2\x9a\x99", ""}},
        {"custom/power",        {"\xe2\x8f\xbb", ""}},
    };

    auto render_group = [&](const std::vector<std::string>& mods) -> std::string {
        std::string chips;
        for (const auto& mod : mods) {
            if (mod == "hyprland/workspaces") {
                chips += "<span class=\"ws active\">1</span>"
                         "<span class=\"ws\">2</span><span class=\"ws\">3</span>"
                         "<span class=\"ws\">4</span><span class=\"ws\">5</span>";
                continue;
            }
            if (mod == "tray") {
                chips += "<span class=\"mod tray\"><span class=\"dot\"></span><span class=\"dot\"></span></span>";
                continue;
            }
            std::string icon, val;
            if (auto it = SAMPLE.find(mod); it != SAMPLE.end()) {
                icon = it->second.first;
                val = it->second.second;
            }
            std::string css = mod;
            std::replace(css.begin(), css.end(), '/', '-');
            chips += "<span class=\"mod " + css + "\">" + html_escape(icon) +
                     (val.empty() ? "" : ("<b>" + html_escape(val) + "</b>")) + "</span>";
        }
        return "<div class=\"group \">" + chips + "</div>";
    };

    auto left = config.get_string_array("modules-left");
    auto center = config.get_string_array("modules-center");
    auto right = config.get_string_array("modules-right");

    std::string body =
        "<div class=\"brow\"><span class=\"blabel\">left</span>" + render_group(left) + "</div>"
        "<div class=\"brow\"><span class=\"blabel\">centre</span>" + render_group(center) + "</div>"
        "<div class=\"brow\"><span class=\"blabel\">right</span>" + render_group(right) + "</div>";

    std::string names;
    for (size_t i = 0; i < right.size(); ++i) {
        if (i > 0) names += ", ";
        names += right[i];
    }

    return "<div class=\"bar\">" + body + "</div>\n"
           "<p class=\"caption\">The three floating groups of the bar, exactly as listed in\n"
           "<code>waybar/config.jsonc</code> \xe2\x80\x94 on screen they sit in one row, left \xc2\xb7 centre \xc2\xb7 right.\n"
           "Module values are sample data; the right group is ordered\n"
           "<code>" + html_escape(names) + "</code>.</p>";
}

std::vector<std::pair<int, std::string>> build_project_tree(const fs::path& repo) {
    std::vector<fs::path> rels;
    for (const auto& entry : fs::recursive_directory_iterator(repo)) {
        if (!entry.is_regular_file()) continue;
        fs::path rel = fs::relative(entry.path(), repo);
        std::string s = rel.generic_string();
        if (s.rfind(".git/", 0) == 0 || s == ".git" ||
            s.rfind("bin/", 0) == 0 ||
            s.rfind(".config/hypr/bin/", 0) == 0) {
            continue;
        }
        rels.push_back(rel);
    }
    std::sort(rels.begin(), rels.end(), [](const fs::path& a, const fs::path& b) {
        return a.generic_string() < b.generic_string();
    });

    std::vector<std::pair<int, std::string>> rows;
    std::set<std::string> seen_dirs;
    for (const auto& rel : rels) {
        std::vector<std::string> parts;
        for (const auto& p : rel) parts.push_back(p.string());
        std::string key;
        for (size_t i = 0; i + 1 < parts.size(); ++i) {
            if (i > 0) key.push_back('/');
            key += parts[i];
            if (!seen_dirs.count(key)) {
                seen_dirs.insert(key);
                rows.emplace_back(static_cast<int>(i), parts[i] + "/");
            }
        }
        if (!parts.empty()) {
            rows.emplace_back(static_cast<int>(parts.size() - 1), parts.back());
        }
    }
    return rows;
}

constexpr const char* CSS = R"CSS(
:root{
  --ink:#080808; --surface:#0d0d0d; --surface-2:#141414; --surface-3:#1a1a1a;
  --text:#e6e6e6; --dim:#8a8a8a; --faint:#4a4a4a;
  --line:rgba(255,255,255,.08); --line-strong:rgba(255,255,255,.16);
  --mono:"JetBrainsMono Nerd Font","JetBrains Mono",ui-monospace,SFMono-Regular,Menlo,Consolas,monospace;
}
*{box-sizing:border-box}
body{
  margin:0; background:var(--ink); color:var(--text);
  font-family:var(--mono); font-size:14px; line-height:1.6;
  -webkit-font-smoothing:antialiased;
}
.wrap{max-width:1180px; margin:0 auto; padding:56px 24px 96px}
a{color:#fff; text-decoration:none; border-bottom:1px solid var(--line-strong)}
a:hover{border-bottom-color:#fff}
h1{font-size:34px; margin:0 0 6px; letter-spacing:-.5px; color:#fff}
h2{font-size:19px; margin:64px 0 4px; color:#fff; letter-spacing:.2px}
h2 .num{color:var(--faint); margin-right:10px; font-weight:400}
h3{font-size:14px; margin:28px 0 8px; color:var(--dim); text-transform:uppercase; letter-spacing:1.4px; font-weight:500}
p{margin:8px 0; color:var(--text)}
.lead{color:var(--dim); max-width:74ch}
hr{border:0; border-top:1px solid var(--line); margin:20px 0 0}
code{font-family:var(--mono); background:var(--surface-2); border:1px solid var(--line);
     border-radius:5px; padding:1px 6px; font-size:12.5px; color:#fff}
.badges{display:flex; flex-wrap:wrap; gap:8px; margin:18px 0 0}
.badge{border:1px solid var(--line); background:var(--surface); border-radius:999px;
       padding:4px 12px; font-size:12px; color:var(--dim)}
.badge b{color:#fff; font-weight:500}
.note{border:1px solid var(--line); background:var(--surface); border-radius:10px;
      padding:12px 16px; color:var(--dim); font-size:12.5px; margin:18px 0}
.figure{border:1px solid var(--line); border-radius:14px; overflow:hidden; margin-top:18px; background:#000}
.figure svg{display:block; width:100%; height:auto}
.caption{color:var(--faint); font-size:12px; margin:10px 0 0}
.bar{display:flex; flex-direction:column; gap:7px; background:#050505;
     border:1px solid var(--line); border-radius:14px; padding:12px 14px; margin:18px 0 6px}
.brow{display:flex; gap:10px; align-items:center; flex-wrap:wrap}
.blabel{color:var(--dim); font-size:10px; letter-spacing:.16em; text-transform:uppercase;
        min-width:50px}
.group{display:flex; gap:2px; align-items:center; background:rgba(10,10,10,.72);
       border:1px solid var(--line); border-radius:12px; padding:4px 8px; flex-wrap:wrap}
.mod{color:var(--text); padding:0 8px; font-size:12.5px; white-space:nowrap}
.mod b{color:#fff; font-weight:500; margin-left:5px}
.ws{color:#6f6f6f; padding:2px 7px; border-radius:7px; font-size:12.5px}
.ws.active{background:#e6e6e6; color:#0a0a0a; font-weight:600}
.mod.custom-focus,.mod.hyprland-submap,.mod.custom-perf{background:#e6e6e6; color:#0a0a0a; border-radius:7px;
       padding:1px 8px; margin:0 3px; font-weight:600; font-size:11.5px}
.mod.custom-focus b,.mod.hyprland-submap b,.mod.custom-perf b{color:#0a0a0a; font-weight:700}
.wgrid{display:grid; grid-template-columns:repeat(auto-fill,minmax(195px,1fr)); gap:10px; margin-top:12px}
.simbar{display:flex; flex-wrap:wrap; gap:6px; align-items:center; margin:16px 0 10px}
.simbtn{appearance:none; background:var(--surface); border:1px solid var(--line-strong); color:var(--dim);
        border-radius:8px; padding:5px 11px; font:inherit; font-size:11.5px; cursor:pointer;
        transition:all .15s ease}
.simbtn:hover{color:#fff; border-color:#fff}
.simbtn.on{background:#e6e6e6; color:#080808; border-color:#e6e6e6; font-weight:600}
.simstage{position:relative; aspect-ratio:16/9; border:1px solid var(--line-strong); border-radius:14px;
          overflow:hidden; background:#050505; user-select:none; --dur:440ms; --ease:cubic-bezier(0.16,1,0.3,1)}
.simstage.snappy{--dur:160ms; --ease:cubic-bezier(0.2,0,0,1)}
.simstage.cinematic{--dur:680ms; --ease:cubic-bezier(0.12,1.04,0.28,1)}
.simstage.minimal,.simstage.turbo{--dur:0ms; --ease:linear}
.simstage.turbo .simgrid{inset:38px 0 0 0; gap:0}
.simstage.turbo .simwin{border-radius:0; backdrop-filter:none; background:#090909}
.simstage.glass-xray .simwin{backdrop-filter:blur(5px); background:rgba(10,10,10,.88)}
.simstage.glass-acrylic .simwin{backdrop-filter:blur(18px) saturate(1.1) contrast(1.06); background:rgba(18,18,18,.68); border-color:rgba(255,255,255,.26)}
.simstage.glass-off .simwin{backdrop-filter:none; background:#0b0b0b}
.simstage.sh-noir{filter:grayscale(1) contrast(1.12)}
.simstage.sh-grain{filter:grayscale(0.9) contrast(1.08) brightness(0.96)}
.simstage.sh-crt{filter:grayscale(1) contrast(1.16) brightness(1.03)}
.simstage.sh-eink{filter:grayscale(1) contrast(0.84) brightness(1.08)}
.simstage.sh-oled{filter:grayscale(1) contrast(1.24) brightness(0.92)}
.simtabs{display:flex; gap:4px; margin-bottom:7px}
.simtab{appearance:none; background:rgba(24,24,24,.88); border:1px solid rgba(255,255,255,.10); color:#8a8a8a;
        border-radius:7px; padding:2px 8px; font:inherit; font-size:10px; cursor:pointer; transition:all .16s ease}
.simtab.active{background:#383838; color:#fff; border-color:rgba(255,255,255,.32); font-weight:600}
.simcrt{position:absolute; inset:0; pointer-events:none; opacity:0; z-index:6;
        background:repeating-linear-gradient(0deg,rgba(0,0,0,.24) 0px,rgba(0,0,0,.24) 1px,transparent 1px,transparent 3px);
        transition:opacity 240ms ease}
.simstage.sh-crt .simcrt{opacity:1}
.simcanvas{position:absolute; inset:0; width:100%; height:100%; display:block; transition:opacity 420ms var(--ease)}
.simbg{position:absolute; inset:0; width:100%; height:100%; object-fit:cover; opacity:0;
       clip-path:circle(145% at 50% 50%); transition:opacity 360ms var(--ease), clip-path 680ms var(--ease)}
.simbg.show{opacity:1}
.simtop{position:absolute; top:8px; left:10px; right:10px; height:28px; display:flex; align-items:center;
        justify-content:space-between; padding:0 10px; background:rgba(10,10,10,.82); backdrop-filter:blur(10px);
        border:1px solid var(--line-strong); border-radius:9px; font-size:11px; z-index:3;
        transition:transform var(--dur) var(--ease), opacity var(--dur) var(--ease)}
.simstage.zen .simtop{transform:translateY(-44px); opacity:0}
.simws{display:inline-block; color:#6f6f6f; padding:1px 6px; min-width:18px; text-align:center;
       border-radius:6px; cursor:pointer; transition:all 260ms cubic-bezier(0.16,1,0.3,1)}
.simws:hover{color:#fff; background:rgba(255,255,255,.1)}
.simws.active{background:#e6e6e6; color:#080808; font-weight:700; padding:1px 11px; min-width:26px}
.simgrid{position:absolute; inset:46px 12px 12px 12px; display:grid; grid-template-columns:1.15fr .85fr;
         gap:10px; z-index:2; transition:inset var(--dur) var(--ease), gap var(--dur) var(--ease), transform var(--dur) var(--ease), opacity var(--dur) var(--ease)}
.simstage.zen .simgrid{inset:20px 26px 20px 26px; gap:18px}
.simstage.airy .simgrid{inset:52px 24px 24px 24px; gap:18px}
.simstage.compact .simgrid{inset:40px 5px 5px 5px; gap:4px}
.simstage.zero .simgrid{inset:38px 0 0 0; gap:0}
@keyframes borderOrbit{0%{border-color:rgba(255,255,255,.92); box-shadow:0 0 0 1px rgba(255,255,255,.38),0 14px 38px rgba(0,0,0,.6)}
50%{border-color:rgba(135,135,135,.55); box-shadow:0 0 0 1px rgba(180,180,180,.12),0 14px 38px rgba(0,0,0,.6)}
100%{border-color:rgba(255,255,255,.92); box-shadow:0 0 0 1px rgba(255,255,255,.38),0 14px 38px rgba(0,0,0,.6)}}
.simwin{background:rgba(10,10,10,.84); backdrop-filter:blur(10px); border:1px solid rgba(255,255,255,.14);
        border-radius:10px; padding:12px 14px; font-size:11.5px; line-height:1.55; overflow:hidden;
        display:flex; flex-direction:column; transition:border-radius var(--dur) var(--ease), transform var(--dur) var(--ease), border-color 240ms ease}
.simstage.zero .simwin{border-radius:0}
.simwin.focus{border-color:rgba(255,255,255,.72); box-shadow:0 12px 36px rgba(0,0,0,.55)}
.simstage.cinematic .simwin.focus{animation:borderOrbit 2.8s ease-in-out infinite}
.simdim{position:absolute; inset:0; background:rgba(0,0,0,.46); backdrop-filter:blur(3px); opacity:0;
        pointer-events:none; z-index:3; transition:opacity var(--dur) var(--ease)}
.simdim.on{opacity:1}
.simtitle{display:flex; justify-content:space-between; color:var(--dim); font-size:10.5px;
          border-bottom:1px solid var(--line); padding-bottom:6px; margin-bottom:8px}
.simdrawer{position:absolute; left:14%; right:14%; top:15%; bottom:17%; background:rgba(12,12,12,.94);
           border:1px solid #e6e6e6; border-radius:12px; padding:14px 16px; box-shadow:0 24px 64px rgba(0,0,0,.82);
           display:flex; flex-direction:column; z-index:4; opacity:0; pointer-events:none;
           transform:translateY(-28px) scale(0.95); transition:transform var(--dur) var(--ease), opacity var(--dur) var(--ease)}
.simdrawer.open{opacity:1; pointer-events:auto; transform:translateY(0) scale(1)}
.simpal{position:absolute; left:20%; right:20%; top:16%; background:rgba(10,10,10,.96);
        border:1px solid #ffffff; border-radius:12px; padding:12px; box-shadow:0 24px 64px rgba(0,0,0,.88);
        display:flex; flex-direction:column; gap:8px; z-index:5; opacity:0; pointer-events:none;
        transform:scale(0.92) translateY(-8px); transition:transform var(--dur) var(--ease), opacity var(--dur) var(--ease)}
.simpal.open{opacity:1; pointer-events:auto; transform:scale(1) translateY(0)}
.simpal input{width:100%; background:#141414; border:1px solid var(--line-strong); border-radius:8px;
              color:#fff; font:inherit; font-size:12px; padding:7px 10px; outline:none}
.simpal ul{list-style:none; margin:0; padding:0; font-size:11.5px; max-height:150px; overflow:auto}
.simpal li{padding:5px 8px; border-radius:6px; color:var(--text); cursor:pointer}
.simpal li:first-child,.simpal li:hover{background:#e6e6e6; color:#080808; font-weight:600}
.wcard{appearance:none; background:var(--surface); border:1px solid var(--line); border-radius:10px;
       padding:0; overflow:hidden; text-align:left; cursor:pointer; color:var(--text); font:inherit;
       transition:border-color .15s ease, transform .15s ease}
.wcard:hover{border-color:var(--line-strong); transform:translateY(-1px)}
.wcard.active{border-color:#fff; box-shadow:0 0 0 1px #fff}
.wcard img{display:block; width:100%; aspect-ratio:16/9; object-fit:cover}
.wmeta{padding:6px 8px; font-size:10.5px}
.wmeta b{display:block; color:#fff; font-weight:500; white-space:nowrap; overflow:hidden; text-overflow:ellipsis}
.wmeta span{color:var(--faint)}
.kbsearch{width:100%; background:var(--surface-2); border:1px solid var(--line-strong); border-radius:10px;
          padding:10px 14px; color:#fff; font-family:var(--mono); font-size:13px; margin:14px 0 6px; outline:none}
.kbsearch:focus{border-color:#fff}
@media (max-width:760px){ .wgrid{grid-template-columns:repeat(2,minmax(0,1fr))} }
.dot{display:inline-block; width:9px; height:9px; border-radius:2px; background:#3a3a3a; margin:0 3px}
table{width:100%; border-collapse:collapse; margin:10px 0 0; font-size:13px}
th,td{text-align:left; padding:8px 10px; border-bottom:1px solid var(--line); vertical-align:top}
th{color:var(--faint); font-weight:500; font-size:11.5px; text-transform:uppercase; letter-spacing:1.2px}
td:first-child{white-space:nowrap; color:#fff}
tbody tr:hover{background:var(--surface)}
.two{display:grid; grid-template-columns:repeat(auto-fit,minmax(320px,1fr)); gap:28px}
.swatches{display:flex; flex-wrap:wrap; gap:10px; margin-top:12px}
.sw{border:1px solid var(--line); border-radius:10px; overflow:hidden; width:132px; background:var(--surface)}
.sw .chip{height:44px}
.sw .meta{padding:7px 10px; font-size:11.5px; color:var(--dim)}
.sw .meta b{display:block; color:#fff; font-weight:500}
.sw .meta span{color:var(--faint)}
pre{background:var(--surface); border:1px solid var(--line); border-radius:10px;
    padding:14px 16px; overflow-x:auto; font-size:12.5px; color:var(--text); margin:10px 0}
pre .c{color:var(--faint)}
pre .k{color:#fff}
.tree{columns:2; column-gap:32px; font-size:12.5px; color:var(--dim); margin-top:12px}
.tree div{break-inside:avoid; white-space:pre}
.tree .d{color:#fff}
.grid3{display:grid; grid-template-columns:repeat(auto-fit,minmax(260px,1fr)); gap:14px; margin-top:14px}
.card{border:1px solid var(--line); background:var(--surface); border-radius:12px; padding:14px 16px}
.card h4{margin:0 0 6px; font-size:13px; color:#fff; font-weight:600}
.card p{margin:0; font-size:12.5px; color:var(--dim)}
.card kbd{font-family:var(--mono); background:#fff; color:#0a0a0a; border-radius:5px;
          padding:1px 7px; font-size:11.5px; font-weight:600}
footer{margin-top:72px; padding-top:20px; border-top:1px solid var(--line);
       color:var(--faint); font-size:12px}
@media (max-width:760px){ .tree{columns:1} h1{font-size:26px} }
)CSS";

constexpr const char* SIM_JS = R"JS(<script>
(function(){
  var stage = document.getElementById('sim-stage');
  var sgrid = document.getElementById('sim-grid');
  var sdim = document.getElementById('sim-dim');
  var dbox = document.getElementById('sim-drawer-box');
  var pbox = document.getElementById('sim-pal-box');
  var bgImg = document.getElementById('wall-stage-img');
  var liveBtn = document.getElementById('sim-live-btn');
  var cvs = document.getElementById('sim-live-canvas');
  var liveMode = true;
  var liveScene = 0;

  if (cvs && cvs.getContext) {
    var ctx = cvs.getContext('2d');
    var startT = performance.now();
    function drawOrrery(now) {
      if (liveMode) {
        var t = (now - startT) * 0.00055;
        var w = cvs.width, h = cvs.height, cx = w * 0.5, cy = h * 0.5;
        ctx.fillStyle = '#070707';
        ctx.fillRect(0, 0, w, h);
        if (liveScene === 0) {
          ctx.strokeStyle = 'rgba(255,255,255,0.04)';
          ctx.lineWidth = 1;
          ctx.beginPath(); ctx.moveTo(w*0.06, cy); ctx.lineTo(w*0.94, cy);
          ctx.moveTo(cx, h*0.06); ctx.lineTo(cx, h*0.94); ctx.stroke();
          var rc = h * 0.165;
          var g = ctx.createRadialGradient(cx, cy, rc * 0.4, cx, cy, rc * (1.55 + 0.08 * Math.sin(t * 2)));
          g.addColorStop(0, 'rgba(255,255,255,0.16)'); g.addColorStop(1, 'rgba(0,0,0,0)');
          ctx.fillStyle = g; ctx.beginPath(); ctx.arc(cx, cy, rc * 1.7, 0, Math.PI * 2); ctx.fill();
          var orbs = [rc * 1.48, rc * 1.98, rc * 2.52];
          orbs.forEach(function(r, i) {
            ctx.strokeStyle = 'rgba(255,255,255,' + (0.15 - i * 0.035) + ')';
            ctx.beginPath(); ctx.arc(cx, cy, r, 0, Math.PI * 2); ctx.stroke();
          });
          var sg = ctx.createLinearGradient(cx, cy - rc, cx, cy + rc);
          sg.addColorStop(0, '#dcdcdc'); sg.addColorStop(0.55, '#545454'); sg.addColorStop(1, '#101010');
          ctx.fillStyle = sg; ctx.beginPath(); ctx.arc(cx, cy, rc, 0, Math.PI * 2); ctx.fill();
          ctx.save(); ctx.beginPath(); ctx.arc(cx, cy, rc, 0, Math.PI * 2); ctx.clip();
          ctx.fillStyle = '#080808';
          for (var y = cy; y < cy + rc; y += 5) ctx.fillRect(cx - rc, y, rc * 2, 2.2);
          ctx.restore();
          [[orbs[0], t * 1.3, 6.5, '#f4f4f4'], [orbs[1], -t * 0.85 + 1.8, 4.5, '#c4c4c4'], [orbs[2], t * 1.9 + 0.5, 3.2, '#9a9a9a']].forEach(function(m) {
            var mx = cx + Math.cos(m[1]) * m[0], my = cy + Math.sin(m[1]) * m[0];
            ctx.fillStyle = m[3]; ctx.beginPath(); ctx.arc(mx, my, m[2], 0, Math.PI * 2); ctx.fill();
          });
        } else {
          var gx0 = w*0.14, gx1 = w*0.86, gy0 = h*0.12, gy1 = h*0.88;
          ctx.strokeStyle = 'rgba(255,255,255,0.06)';
          ctx.strokeRect(gx0, gy0, gx1 - gx0, gy1 - gy0);
          var rx = h * 0.31, ry = h * 0.31;
          ctx.strokeStyle = 'rgba(255,255,255,0.16)'; ctx.lineWidth = 1;
          ctx.beginPath();
          for (var j = 0; j <= 240; j++) {
            var sj = (j / 240) * Math.PI * 2;
            var jx = cx + rx * 0.74 * Math.sin(2 * sj - t * 2.2), jy = cy + ry * 0.74 * Math.cos(3 * sj);
            if (j === 0) ctx.moveTo(jx, jy); else ctx.lineTo(jx, jy);
          }
          ctx.stroke();
          ctx.save();
          ctx.shadowColor = 'rgba(255,255,255,0.65)'; ctx.shadowBlur = 12;
          ctx.strokeStyle = '#f0f0f0'; ctx.lineWidth = 2;
          ctx.beginPath();
          for (var i = 0; i <= 300; i++) {
            var s = (i / 300) * Math.PI * 2;
            var px = cx + rx * Math.sin(3 * s + t * 2.4), py = cy + ry * Math.sin(2 * s);
            if (i === 0) ctx.moveTo(px, py); else ctx.lineTo(px, py);
          }
          ctx.stroke();
          ctx.fillStyle = '#ffffff';
          ctx.beginPath(); ctx.arc(cx + rx * Math.sin(t * 2.4), cy, 4, 0, Math.PI * 2); ctx.fill();
          ctx.restore();
        }
      }
      requestAnimationFrame(drawOrrery);
    }
    requestAnimationFrame(drawOrrery);
  }

  var sparkEl = document.getElementById('sim-spark');
  var bars = ['▁','▂','▃','▄','▅','▆','▇'];
  var ring = [1,2,4,3,2,1];
  if (sparkEl) setInterval(function() {
    var v = Math.max(0, Math.min(6, ring[ring.length - 1] + (Math.random() > 0.5 ? 1 : -1)));
    ring.push(v); ring.shift();
    var pct = 10 + v * 5;
    sparkEl.textContent = ring.map(function(i){ return bars[i]; }).join('') + ' ' + pct + '%';
  }, 1400);

  function syncDim() {
    if (sdim) sdim.classList.toggle('on', dbox.classList.contains('open') || pbox.classList.contains('open'));
  }

  var grid = document.getElementById('wall-grid');
  if (grid) {
    grid.addEventListener('click', function(ev) {
      var btn = ev.target.closest('.wcard');
      if (!btn) return;
      liveMode = false;
      if (liveBtn) liveBtn.classList.remove('on');
      document.querySelectorAll('.wcard').forEach(function(b){ b.classList.remove('active'); });
      btn.classList.add('active');
      bgImg.style.transition = 'none';
      bgImg.style.clipPath = 'circle(0% at 50% 50%)';
      bgImg.src = btn.getAttribute('data-uri');
      bgImg.classList.add('show');
      requestAnimationFrame(function() {
        requestAnimationFrame(function() {
          bgImg.style.transition = '';
          bgImg.style.clipPath = 'circle(145% at 50% 50%)';
        });
      });
      document.getElementById('wall-stage-title').textContent = 'Active: ' + btn.getAttribute('data-title');
      document.getElementById('wall-stage-desc').textContent = btn.getAttribute('data-desc');
    });
  }
  if (liveBtn) liveBtn.addEventListener('click', function() {
    if (liveMode) liveScene = (liveScene + 1) % 2;
    liveMode = true;
    liveBtn.classList.add('on');
    bgImg.classList.remove('show');
    if (liveScene === 0) {
      liveBtn.textContent = '✦ Live: Orrery (60fps)';
      document.getElementById('wall-stage-title').textContent = 'Active: mono-live-01 — Bauhaus Celestial Orrery (60fps Live)';
      document.getElementById('wall-stage-desc').textContent = 'Click again to switch to P4 Harmonic Lissajous Oscilloscope';
    } else {
      liveBtn.textContent = '∿ Live: Lissajous (60fps)';
      document.getElementById('wall-stage-title').textContent = 'Active: mono-live-03 — P4 White-Phosphor Lissajous (60fps Live)';
      document.getElementById('wall-stage-desc').textContent = 'Harmonic 3:2 phase figure · click again for Bauhaus Orrery';
    }
  });

  var turboBtn = document.getElementById('sim-turbo-btn');
  var turboPill = document.getElementById('sim-turbo-pill');
  if (turboBtn) turboBtn.addEventListener('click', function() {
    var on = stage.classList.toggle('turbo');
    turboBtn.classList.toggle('on', on);
    if (turboPill) turboPill.style.display = on ? 'inline-block' : 'none';
  });

  var glasses = [['','Glass: Kawase'],['glass-xray','Glass: X-Ray'],['glass-acrylic','Glass: Acrylic'],['glass-off','Glass: Off']];
  var glIdx = 0;
  var glBtn = document.getElementById('sim-glass-btn');
  if (glBtn) glBtn.addEventListener('click', function() {
    glIdx = (glIdx + 1) % glasses.length;
    stage.classList.remove('glass-xray','glass-acrylic','glass-off');
    if (glasses[glIdx][0]) stage.classList.add(glasses[glIdx][0]);
    glBtn.textContent = glasses[glIdx][1];
    glBtn.classList.toggle('on', glIdx > 0);
  });

  var tabBufs = {
    anim: ['NORMAL · ~/.config/hypr/modules/animations.lua', 'silkSpring · 29 leaves',
      '<div style="color:#686868;font-style:italic">-- Damped physical springs + 29-leaf choreography</div><div>hl.curve(<span style="color:#b8b8b8">"silkSpring"</span>, { type = <span style="color:#b8b8b8">"spring"</span>, mass = <b style="color:#fff">1.0</b>, stiffness = <b style="color:#fff">175</b>, damping = <b style="color:#fff">22</b> })</div><div>hl.animation({ leaf = <span style="color:#b8b8b8">"windowsIn"</span>, enabled = <b style="color:#fff">true</b>, speed = <b style="color:#fff">4.6</b>, spring = <span style="color:#b8b8b8">"silkSpring"</span>, style = <span style="color:#b8b8b8">"popin 84%"</span> })</div>'],
    appear: ['NORMAL · ~/.config/hypr/modules/appearance.lua', 'squircle=4.0 · vrr=2',
      '<div style="color:#686868;font-style:italic">-- G2 squircles, direct scanout, async cursor &amp; Liquid Glass</div><div>decoration = { rounding = <b style="color:#fff">10</b>, rounding_power = <b style="color:#fff">4.0</b> },</div><div>render = { direct_scanout = <b style="color:#fff">2</b>, new_render_scheduling = <b style="color:#fff">true</b>, async_commit = <b style="color:#fff">true</b> }</div>'],
    perf: ['NORMAL · ~/.config/hypr/scripts/perf.sh', 'turbo · glass · bench',
      '<div style="color:#686868;font-style:italic"># Sub-ms Turbo / Game Mode &amp; Optical Glass Engine</div><div>hyprctl --batch <span style="color:#b8b8b8">"keyword general:allow_tearing 1; keyword render:direct_scanout 1; keyword misc:vrr 1"</span></div><div><span style="color:#8a8a8a"># mono_var: 0.18ms (0 forks) · mono-calc: 0.6ms (C++20)</span></div>']
  };
  document.querySelectorAll('.simtab').forEach(function(tb) {
    tb.addEventListener('click', function() {
      document.querySelectorAll('.simtab').forEach(function(x){ x.classList.remove('active'); });
      tb.classList.add('active');
      var d = tabBufs[tb.getAttribute('data-tab')];
      if (d) {
        document.getElementById('sim-buf-path').textContent = d[0];
        document.getElementById('sim-buf-tag').textContent = d[1];
        document.getElementById('sim-code-body').innerHTML = d[2];
        document.getElementById('sim-wintitle').textContent = 'nvim · ' + tb.textContent.split(': ')[1];
      }
    });
  });

  var curWs = 1;
  document.querySelectorAll('.simws').forEach(function(ws) {
    ws.addEventListener('click', function() {
      var target = parseInt(ws.getAttribute('data-ws'), 10) || 1;
      if (target === curWs) return;
      var dir = target > curWs ? 1 : -1;
      curWs = target;
      document.querySelectorAll('.simws').forEach(function(w){ w.classList.remove('active'); });
      ws.classList.add('active');
      sgrid.style.transition = 'none';
      sgrid.style.transform = 'translateX(' + (dir * 38) + 'px)';
      sgrid.style.opacity = '0.35';
      requestAnimationFrame(function() {
        requestAnimationFrame(function() {
          sgrid.style.transition = '';
          sgrid.style.transform = 'translateX(0)';
          sgrid.style.opacity = '1';
        });
      });
    });
  });

  document.querySelectorAll('[data-mot]').forEach(function(btn) {
    btn.addEventListener('click', function() {
      document.querySelectorAll('[data-mot]').forEach(function(b){ b.classList.remove('on'); });
      btn.classList.add('on');
      stage.classList.remove('snappy','cinematic','minimal');
      var m = btn.getAttribute('data-mot');
      if (m !== 'fluid') stage.classList.add(m);
    });
  });

  var shaders = [['off','Shader: Off'],['sh-noir','Shader: Noir'],['sh-grain','Shader: 35mm'],['sh-crt','Shader: P4 CRT'],['sh-eink','Shader: E-Ink'],['sh-oled','Shader: OLED Pitch']];
  var shIdx = 0;
  var shBtn = document.getElementById('sim-shader-btn');
  if (shBtn) shBtn.addEventListener('click', function() {
    shIdx = (shIdx + 1) % shaders.length;
    stage.classList.remove('sh-noir','sh-grain','sh-crt','sh-eink','sh-oled');
    if (shaders[shIdx][0] !== 'off') stage.classList.add(shaders[shIdx][0]);
    shBtn.textContent = shaders[shIdx][1];
    shBtn.classList.toggle('on', shIdx > 0);
  });

  document.querySelectorAll('[data-gap]').forEach(function(btn) {
    btn.addEventListener('click', function() {
      document.querySelectorAll('[data-gap]').forEach(function(b){ b.classList.remove('on'); });
      btn.classList.add('on');
      stage.classList.remove('airy','compact','zero');
      var g = btn.getAttribute('data-gap');
      if (g !== 'default') stage.classList.add(g);
    });
  });
  var zenBtn = document.getElementById('sim-zen');
  if (zenBtn) zenBtn.addEventListener('click', function() {
    zenBtn.classList.toggle('on');
    stage.classList.toggle('zen');
  });
  var drwBtn = document.getElementById('sim-drawer');
  if (drwBtn) drwBtn.addEventListener('click', function() {
    drwBtn.classList.toggle('on');
    dbox.classList.toggle('open');
    syncDim();
  });
  var palBtn = document.getElementById('sim-pal-btn');
  var palIn = document.getElementById('sim-pal-in');
  var palMath = document.getElementById('sim-pal-math');
  if (palBtn) palBtn.addEventListener('click', function() {
    palBtn.classList.toggle('on');
    pbox.classList.toggle('open');
    syncDim();
    if (pbox.classList.contains('open')) palIn.focus();
  });
  if (palIn) palIn.addEventListener('input', function() {
    var raw = palIn.value.trim();
    var expr = raw.charAt(0) === '=' ? raw.slice(1).trim() : raw;
    if (expr && /^[0-9+\-*/().%\s]+$/.test(expr)) {
      try {
        var v = Function('"use strict";return (' + expr + ')')();
        if (typeof v === 'number' && isFinite(v)) {
          palMath.textContent = '= ' + v.toLocaleString() + '  (live calculator)';
          return;
        }
      } catch (_e) {}
    }
    palMath.textContent = 'Tip: type "= 3840 * 2160" or click an action below';
  });
  var palList = document.getElementById('sim-pal-list');
  if (palList) palList.addEventListener('click', function(ev) {
    var li = ev.target.closest('li');
    if (!li) return;
    var a = li.getAttribute('data-act');
    if (a === 'zen') zenBtn.click();
    else if (a === 'drawer') drwBtn.click();
    else { var gb = document.querySelector('[data-gap="' + a + '"]'); if (gb) gb.click(); }
    palBtn.click();
  });
  var input = document.getElementById('kb-filter');
  var box = document.getElementById('kb-container');
  if (input && box) {
    input.addEventListener('input', function() {
      var q = input.value.trim().toLowerCase();
      box.querySelectorAll('table').forEach(function(tbl) {
        var shown = 0;
        tbl.querySelectorAll('tbody tr').forEach(function(tr) {
          var ok = !q || tr.textContent.toLowerCase().indexOf(q) !== -1;
          tr.style.display = ok ? '' : 'none';
          if (ok) shown++;
        });
        var h3 = tbl.previousElementSibling;
        tbl.style.display = shown ? '' : 'none';
        if (h3 && h3.tagName === 'H3') h3.style.display = shown ? '' : 'none';
      });
    });
  }
})();
</script>)JS";

std::string build_page(const fs::path& repo) {
    fs::path cfg = repo / ".config";
    auto [raw_sections, main_mod] = mono_kb::parse(mono_cfg::read_text(cfg / "hypr/modules/binds.lua"), repo);
    auto sections = mono_kb::documented_rows(raw_sections, main_mod);
    std::string main_label = mono_kb::mod_label(main_mod);

    auto waybar_cfg = mono_cfg::parse_jsonc(mono_cfg::read_text(cfg / "waybar/config.jsonc"));
    auto rofi = mono_cfg::parse_rofi_palette(mono_cfg::read_text(cfg / "rofi/theme.rasi"));
    auto kitty = mono_cfg::parse_kitty_palette(mono_cfg::read_text(cfg / "kitty/theme.conf"));
    auto toml = mono_cfg::parse_and_validate_toml(mono_cfg::read_text(cfg / "starship.toml"));
    auto starship = toml["palettes.mono"];
    auto wb_colours = mono_cfg::parse_waybar_colours(mono_cfg::read_text(cfg / "waybar/style.css"));
    auto pkgs = mono_cfg::parse_packages_txt(mono_cfg::read_text(repo / "packages.txt"));
    auto aur = mono_cfg::parse_aur_packages_txt(mono_cfg::read_text(repo / "aur-packages.txt"));
    auto tree = build_project_tree(repo);

    std::string desktop_svg = mono_cfg::read_text(repo / "docs/preview.svg");
    if (size_t xml_end = desktop_svg.find("?>"); xml_end != std::string::npos && desktop_svg.rfind("<?xml", 0) == 0) {
        desktop_svg = mono_cfg::trim(desktop_svg.substr(xml_end + 2));
    }
    if (size_t wh = desktop_svg.find("width=\"1600\" height=\"900\""); wh != std::string::npos) {
        desktop_svg.replace(wh, 25, "preserveAspectRatio=\"xMidYMid meet\"");
    }

    auto [hero_wall_uri, wall_cards_html] = build_wallpaper_cards(repo);

    size_t total_binds = 0;
    for (const auto& [_, rows] : sections) total_binds += rows.size();
    size_t total_pkgs = 0;
    for (const auto& [_, entries] : pkgs) total_pkgs += entries.size();

    std::string bind_tables;
    for (const auto& [title, rows] : sections) {
        std::string body;
        for (const auto& [k, d] : rows) {
            body += "<tr><td><kbd>" + html_escape(k) + "</kbd></td><td>" + html_escape(d) + "</td></tr>";
        }
        bind_tables += "<h3>" + html_escape(title) + "</h3>"
                       "<table><thead><tr><th>Keys</th><th>Action</th></tr></thead>"
                       "<tbody>" + body + "</tbody></table>";
    }

    std::string rofi_sw;
    for (const auto& [name, val] : rofi) rofi_sw += swatch_html(name, val);

    std::string kitty_sw;
    for (const char* k : {"background", "foreground", "cursor"}) {
        if (auto it = kitty.find(k); it != kitty.end()) kitty_sw += swatch_html(k, it->second);
    }
    std::string kitty_ramp;
    for (int i = 0; i < 16; ++i) {
        std::string ck = "color" + std::to_string(i);
        if (auto it = kitty.find(ck); it != kitty.end()) kitty_ramp += swatch_html(ck, it->second);
    }

    std::string star_sw;
    for (const auto& [name, val] : starship) star_sw += swatch_html(name, val);

    std::string wb_sw;
    for (size_t i = 0; i < std::min<size_t>(8, wb_colours.size()); ++i) {
        wb_sw += swatch_html("style.css #" + std::to_string(i + 1), wb_colours[i]);
    }

    std::string pkg_html;
    for (const auto& [title, entries] : pkgs) {
        std::string rows;
        for (const auto& [name, comment] : entries) {
            rows += "<tr><td>" + html_escape(name) + "</td><td>" + html_escape(comment) + "</td></tr>";
        }
        pkg_html += "<h3>" + html_escape(title) + "</h3><table><tbody>" + rows + "</tbody></table>";
    }

    std::string aur_rows;
    for (const auto& [name, comment] : aur) {
        aur_rows += "<tr><td>" + html_escape(name) + "</td><td>" + html_escape(comment) + "</td></tr>";
    }

    std::string tree_html;
    for (const auto& [depth, name] : tree) {
        std::string indent;
        for (int i = 0; i < depth; ++i) indent += "\xe2\x94\x82  ";
        bool is_dir = !name.empty() && name.back() == '/';
        tree_html += "<div>" + indent + "<span class=\"" + (is_dir ? "d" : "") + "\">" +
                     html_escape(name) + "</span></div>";
    }

    std::string user_lua = mono_cfg::read_text(cfg / "hypr/modules/user.lua");
    std::string gaps_in = mono_cfg::lua_value(user_lua, "gaps_in", "5");
    std::string gaps_out = mono_cfg::lua_value(user_lua, "gaps_out", "10");
    std::string rounding = mono_cfg::lua_value(user_lua, "rounding", "10");
    std::string browser = mono_cfg::lua_value(user_lua, "browser", "firefox");
    std::string terminal = mono_cfg::lua_value(user_lua, "terminal", "kitty");
    std::string files_app = mono_cfg::lua_value(user_lua, "file_manager", "thunar");

    std::ostringstream o;
    o << "<!DOCTYPE html>\n"
      << "<html lang=\"en\">\n"
      << "<head>\n"
      << "<meta charset=\"utf-8\">\n"
      << "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">\n"
      << "<title>hyprland-monochrome — preview</title>\n"
      << "<style>" << CSS << "</style>\n"
      << "</head>\n"
      << "<body>\n"
      << "<div class=\"wrap\">\n\n"
      << "  <h1>hyprland-monochrome</h1>\n"
      << "  <p class=\"lead\">A universal monochrome Hyprland desktop that runs on any PC (AMD, Intel, NVIDIA, hybrid\n"
      << "  Optimus laptops, VMs &amp; Arch/Debian/Fedora/openSUSE), opens any Linux or Windows app, and ships an\n"
      << "  <b>OpenCode</b> browser theme across Firefox &amp; Chromium. Generated directly from the repository files.</p>\n"
      << "  <div class=\"badges\">\n"
      << "    <span class=\"badge\"><b>Hyprland</b> 0.55+ · Lua config · C++20 tools</span>\n"
      << "    <span class=\"badge\"><b>Any PC</b> AMD · Intel · NVIDIA · Optimus · VM</span>\n"
      << "    <span class=\"badge\"><b>Any App</b> Wayland · X11 · Flatpak · AppImage · Wine</span>\n"
      << "    <span class=\"badge\"><b>OpenCode</b> Browser Theme + Startpage</span>\n"
      << "    <span class=\"badge\"><b>" << total_binds << "</b> keybinds</span>\n"
      << "    <span class=\"badge\"><b>" << total_pkgs << "</b> official packages</span>\n"
      << "    <span class=\"badge\"><b>" << html_escape(terminal) << "</b> · <b>" << html_escape(files_app) << "</b> · Zsh + Starship</span>\n"
      << "  </div>\n\n"
      << "  <h2><span class=\"num\">01</span>Desktop layout &amp; Wallpaper Studio</h2>\n"
      << "  <p class=\"lead\">Tiled windows with " << gaps_in << "px inner and " << gaps_out << "px outer gaps, " << rounding << "px rounding,\n"
      << "  a focused-window border that goes white-to-grey, monochrome GTK3/GTK4 overrides, and a frosted top bar\n"
      << "  with live Pomodoro/Zen and modal submap pills. Rofi slides in over the top for the Command Palette,\n"
      << "  launcher and visual pickers.</p>\n"
      << "  <div class=\"figure\">" << desktop_svg << "</div>\n"
      << "  <p class=\"caption\">Vector layout drawn to the real gap, rounding and bar geometry from\n"
      << "  <code>modules/appearance.lua</code> (<code>docs/preview.svg</code>).</p>\n\n"
      << "  <h3>60 FPS Velocity &amp; Kinetic Simulator, Squircle Liquid Glass, GLSL Shaders &amp; 20-Wallpaper Studio</h3>\n"
      << "  <div class=\"simbar\" id=\"sim-controls\">\n"
      << "    <button type=\"button\" class=\"simbtn on\" id=\"sim-live-btn\">✦ Live: Orrery (60fps)</button>\n"
      << "    <button type=\"button\" class=\"simbtn\" id=\"sim-turbo-btn\">⚡ Turbo (Super+F12)</button>\n"
      << "    <button type=\"button\" class=\"simbtn\" id=\"sim-glass-btn\">Glass: Kawase</button>\n"
      << "    <button type=\"button\" class=\"simbtn on\" data-gap=\"default\">Gaps: Default</button>\n"
      << "    <button type=\"button\" class=\"simbtn\" data-gap=\"airy\">Airy</button>\n"
      << "    <button type=\"button\" class=\"simbtn\" data-gap=\"compact\">Compact</button>\n"
      << "    <button type=\"button\" class=\"simbtn\" data-gap=\"zero\">Zero</button>\n"
      << "    <button type=\"button\" class=\"simbtn on\" data-mot=\"fluid\">Motion: Fluid</button>\n"
      << "    <button type=\"button\" class=\"simbtn\" data-mot=\"snappy\">Snappy</button>\n"
      << "    <button type=\"button\" class=\"simbtn\" data-mot=\"cinematic\">Cinematic + Shimmer</button>\n"
      << "    <button type=\"button\" class=\"simbtn\" id=\"sim-shader-btn\">Shader: Off</button>\n"
      << "    <button type=\"button\" class=\"simbtn\" id=\"sim-zen\">Zen (Super+Z)</button>\n"
      << "    <button type=\"button\" class=\"simbtn\" id=\"sim-drawer\">btop++ Drawer</button>\n"
      << "    <button type=\"button\" class=\"simbtn\" id=\"sim-pal-btn\">Palette (Super+;)</button>\n"
      << "  </div>\n"
      << "  <div class=\"simstage\" id=\"sim-stage\">\n"
      << "    <canvas class=\"simcanvas\" id=\"sim-live-canvas\" width=\"960\" height=\"540\"></canvas>\n"
      << "    <img class=\"simbg\" id=\"wall-stage-img\" src=\"" << hero_wall_uri << "\" alt=\"Selected monochrome wallpaper\">\n"
      << "    <div class=\"simcrt\"></div>\n"
      << "    <div class=\"simdim\" id=\"sim-dim\"></div>\n"
      << "    <div class=\"simtop\">\n"
      << "      <div><b style=\"color:#fff;margin-right:7px\">◆</b><span class=\"simws active\" data-ws=\"1\">1</span> <span class=\"simws\" data-ws=\"2\">2</span> <span class=\"simws\" data-ws=\"3\">3</span> <span style=\"color:var(--dim);margin-left:8px\" id=\"sim-wintitle\">nvim · modules/animations.lua</span></div>\n"
      << "      <div style=\"color:var(--dim)\">♫ Midnight City — M83</div>\n"
      << "      <div><span id=\"sim-turbo-pill\" style=\"display:none;background:#fff;color:#080808;padding:1px 7px;border-radius:6px;font-weight:700;margin-right:7px\">⚡ TURBO</span><span style=\"color:#e6e6e6;margin-right:8px\" id=\"sim-spark\">▁▂▄▆▃▂ 12%</span><span style=\"background:#e6e6e6;color:#080808;padding:1px 7px;border-radius:6px;font-weight:700;margin-right:8px\">◎ 24:58</span><b style=\"color:#fff\">21:42</b></div>\n"
      << "    </div>\n"
      << "    <div class=\"simgrid\" id=\"sim-grid\">\n"
      << "      <div class=\"simwin focus\">\n"
      << "        <div class=\"simtabs\" id=\"sim-group-tabs\">\n"
      << "          <button type=\"button\" class=\"simtab active\" data-tab=\"anim\">1: animations.lua</button>\n"
      << "          <button type=\"button\" class=\"simtab\" data-tab=\"appear\">2: appearance.lua</button>\n"
      << "          <button type=\"button\" class=\"simtab\" data-tab=\"perf\">3: perf.sh</button>\n"
      << "        </div>\n"
      << "        <div class=\"simtitle\"><span id=\"sim-buf-path\">NORMAL · ~/.config/hypr/modules/animations.lua</span><span id=\"sim-buf-tag\">silkSpring · 29 leaves</span></div>\n"
      << "        <div id=\"sim-code-body\">\n"
      << "          <div style=\"color:#686868;font-style:italic\">-- Damped physical springs + 29-leaf choreography</div>\n"
      << "          <div>hl.curve(<span style=\"color:#b8b8b8;font-style:italic\">\"silkSpring\"</span>, { type = <span style=\"color:#b8b8b8;font-style:italic\">\"spring\"</span>, mass = <b style=\"color:#fff\">1.0</b>, stiffness = <b style=\"color:#fff\">175</b>, damping = <b style=\"color:#fff\">22</b> })</div>\n"
      << "          <div>hl.animation({ leaf = <span style=\"color:#b8b8b8;font-style:italic\">\"windowsIn\"</span>,  enabled = <b style=\"color:#fff\">true</b>, speed = <b style=\"color:#fff\">4.6</b>, spring = <span style=\"color:#b8b8b8;font-style:italic\">\"silkSpring\"</span>, style = <span style=\"color:#b8b8b8;font-style:italic\">\"popin 84%\"</span> })</div>\n"
      << "          <div>hl.animation({ leaf = <span style=\"color:#b8b8b8;font-style:italic\">\"workspaces\"</span>, enabled = <b style=\"color:#fff\">true</b>, speed = <b style=\"color:#fff\">5.2</b>, bezier = <span style=\"color:#b8b8b8;font-style:italic\">\"workspaceGlide\"</span>, style = <span style=\"color:#b8b8b8;font-style:italic\">\"slidefade 18%\"</span> })</div>\n"
      << "        </div>\n"
      << "        <div style=\"margin-top:auto;background:#141414;padding:3px 8px;border-radius:5px;color:var(--dim);display:flex;justify-content:space-between\">\n"
      << "          <span><b style=\"background:#e6e6e6;color:#080808;padding:0 5px;border-radius:3px\">GROUP</b> Super+G tab stack</span><span>squircle=4.0 · vfr+vrr</span>\n"
      << "        </div>\n"
      << "      </div>\n"
      << "      <div class=\"simwin\">\n"
      << "        <div class=\"simtitle\"><span>kitty --single-instance · doctor.sh (&lt;3ms spawn)</span><span>C++20 + 0-fork</span></div>\n"
      << "        <div><b style=\"color:#fff\">❯</b> ~/.config/hypr/scripts/doctor.sh</div>\n"
      << "        <div style=\"color:#e6e6e6\">✓ Lua config &nbsp;&nbsp;&nbsp;&nbsp;&nbsp;11/11 modules · squircle=4.0 · snap=on</div>\n"
      << "        <div style=\"color:#e6e6e6\">✓ Velocity &nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;C++20 tools · 0-fork bash · direct_scanout=2</div>\n"
      << "        <div style=\"color:#e6e6e6\">✓ Kinetic &amp; GLSL &nbsp;3 springs · 29 leaves · 5 GLSL ES 3.00 shaders</div>\n"
      << "        <div style=\"color:#e6e6e6\">✓ Wallpapers &nbsp;&nbsp;&nbsp;&nbsp;&nbsp;17 static (to 16K) + 3 live loops (120fps awww)</div>\n"
      << "        <div style=\"margin-top:auto;color:var(--dim)\" id=\"wall-stage-title\">Active: mono-live-01 — Bauhaus Celestial Orrery (60fps Live)</div>\n"
      << "        <div style=\"color:var(--faint);font-size:10.5px\" id=\"wall-stage-desc\">Click Live button to toggle Lissajous Oscilloscope · click tabs 1/2/3 on left</div>\n"
      << "      </div>\n"
      << "    </div>\n"
      << "    <div class=\"simdrawer\" id=\"sim-drawer-box\">\n"
      << "      <div class=\"simtitle\"><b style=\"color:#fff\">▣ special:btop — Floating Telemetry Drawer (Super + Shift + B)</b><span>Esc to hide</span></div>\n"
      << "      <div style=\"color:#e6e6e6\">CPU  ██████████░░░░░░░░░░  34%   ▁▂▃▅▄▆▃▂▁▃▄▂   48°C</div>\n"
      << "      <div style=\"color:#bdbdbd\">MEM  ████████░░░░░░░░░░░░  4.1 / 16.0 GiB (25%) · ZRAM 0B</div>\n"
      << "      <div style=\"color:#8a8a8a;margin-top:8px\">PID   USER   COMMAND                  CPU%   MEM%</div>\n"
      << "      <div style=\"color:#e6e6e6\">1042  user   Hyprland (Lua 0.56)       2.1    1.4</div>\n"
      << "      <div style=\"color:#bdbdbd\">1188  user   waybar                    0.3    0.6</div>\n"
      << "      <div style=\"color:#bdbdbd\">1409  user   kitty --class mono-notes  0.1    0.5</div>\n"
      << "    </div>\n"
      << "    <div class=\"simpal\" id=\"sim-pal-box\">\n"
      << "      <input id=\"sim-pal-in\" type=\"text\" value=\"= 1920 * 1080\" placeholder=\"Type command or '= 1920*1080'…\">\n"
      << "      <div id=\"sim-pal-math\" style=\"color:#fff;font-size:11.5px;padding:0 4px\">= 2,073,600 (live C++20 calculator preview)</div>\n"
      << "      <ul id=\"sim-pal-list\">\n"
      << "        <li data-act=\"zen\">◌ Focus · Toggle Distraction-Free Zen Mode (Super + Z)</li>\n"
      << "        <li data-act=\"drawer\">▣ Drawer · Toggle Floating btop++ Monitor (Super + Shift + B)</li>\n"
      << "        <li data-act=\"airy\">⊟ Gaps · Airy Gallery Framing (10 / 22px)</li>\n"
      << "        <li data-act=\"zero\">⊟ Gaps · Zero Edge-to-Edge Monocle (0px)</li>\n"
      << "      </ul>\n"
      << "    </div>\n"
      << "  </div>\n"
      << "  <div class=\"wgrid\" id=\"wall-grid\">" << wall_cards_html << "</div>\n\n"
      << "  <h2><span class=\"num\">02</span>The bar</h2>\n"
      << "  <p class=\"lead\">Waybar as configured: three pill groups, workspace numbers that invert when active,\n"
      << "  live inverted pills for active modal chords (<code>RESIZE</code> / <code>CAPTURE</code>) and Deep-Work\n"
      << "  focus timers (<code>◎ 24:58</code>), plus center now-playing that hides itself when idle.</p>\n"
      << "  " << build_bar_html(waybar_cfg) << "\n\n"
      << "  <h2><span class=\"num\">03</span>OpenCode Browser Theme &amp; Universal PC / App Engine</h2>\n"
      << "  <p class=\"lead\">Every browser family (Firefox, LibreWolf, Zen, Floorp, Brave, Chromium, Chrome, Vivaldi) receives\n"
      << "  an <b>OpenCode</b> terminal-grid theme (<code>#131010</code> canvas, <code>#201d1d</code> elevated prompt,\n"
      << "  <code>1px solid #3d3838</code> structural rules, <code>4px</code> utilitarian corners, 100% monospace chrome)\n"
      << "  plus an offline <code>startpage.html</code> (<kbd>" << html_escape(main_label) << " + Alt + B</kbd>).</p>\n"
      << "  <div style=\"border:1px solid #3d3838;border-radius:4px;background:#131010;overflow:hidden;margin:18px 0\">\n"
      << "    <div style=\"display:flex;align-items:center;gap:6px;padding:6px 10px 0;background:#0d0b0b;border-bottom:1px solid #3d3838;font-size:12px\">\n"
      << "      <span style=\"padding:5px 14px;background:#201d1d;border:1px solid #3d3838;border-bottom:2px solid #ffffff;border-radius:4px 4px 0 0;color:#fdfcfc;font-weight:700\">opencode // newtab</span>\n"
      << "      <span style=\"padding:5px 14px;color:#7f7a7a\">wiki.hypr.land</span>\n"
      << "      <span style=\"padding:5px 14px;color:#7f7a7a\">github.com</span>\n"
      << "      <span style=\"margin-left:auto;color:#7f7a7a;padding-bottom:4px\">userChrome.css + MV3</span>\n"
      << "    </div>\n"
      << "    <div style=\"display:flex;align-items:center;gap:10px;padding:7px 12px;background:#1b1818;border-bottom:1px solid #3d3838;font-size:12px\">\n"
      << "      <span style=\"color:#7f7a7a\">&larr; &rarr; &#8635;</span>\n"
      << "      <span style=\"flex:1;padding:4px 10px;background:#131010;border:1px solid #3d3838;border-radius:4px;color:#fdfcfc\">file://~/.config/browser/opencode/startpage.html</span>\n"
      << "      <span style=\"padding:2px 8px;border:1px solid #3d3838;border-radius:4px;background:#201d1d;color:#fdfcfc;font-size:11px\">INSERT</span>\n"
      << "    </div>\n"
      << "    <div style=\"padding:24px;display:flex;flex-direction:column;gap:18px\">\n"
      << "      <div style=\"display:flex;justify-content:space-between;align-items:baseline;flex-wrap:wrap\">\n"
      << "        <div style=\"font-size:26px;font-weight:700;color:#fdfcfc\">21:42:08<span style=\"display:inline-block;width:10px;height:21px;background:#fff;margin-left:6px;vertical-align:-2px\"></span></div>\n"
      << "        <div style=\"color:#7f7a7a;font-size:12px\">opencode-dark · 1px grid · 4px radius</div>\n"
      << "      </div>\n"
      << "      <div style=\"display:flex;align-items:center;gap:10px;padding:10px 14px;background:#201d1d;border:1px solid #fdfcfc;border-radius:4px\">\n"
      << "        <b style=\"color:#fff\">&gt;</b>\n"
      << "        <span style=\"color:#b8b2b2;flex:1\">/gh hyprland · /aw nvidia · :3000 · = 3840 * 2160</span>\n"
      << "        <span style=\"background:#fff;color:#131010;padding:2px 10px;border-radius:4px;font-size:11.5px;font-weight:700\">RUN &crarr;</span>\n"
      << "      </div>\n"
      << "      <div style=\"display:grid;grid-template-columns:repeat(auto-fit,minmax(190px,1fr));border-top:1px solid #3d3838;border-left:1px solid #3d3838\">\n"
      << "        <div style=\"padding:12px;background:#1b1818;border-right:1px solid #3d3838;border-bottom:1px solid #3d3838\"><b style=\"color:#fdfcfc\">Any GPU / PC</b><div style=\"color:#7f7a7a;font-size:12px\">modules/hardware.lua auto-tunes AMD, Intel, NVIDIA, Optimus &amp; VMs</div></div>\n"
      << "        <div style=\"padding:12px;background:#1b1818;border-right:1px solid #3d3838;border-bottom:1px solid #3d3838\"><b style=\"color:#fdfcfc\">Multi-Distro</b><div style=\"color:#7f7a7a;font-size:12px\">install.sh supports pacman, apt-get, dnf &amp; zypper out of the box</div></div>\n"
      << "        <div style=\"padding:12px;background:#1b1818;border-right:1px solid #3d3838;border-bottom:1px solid #3d3838\"><b style=\"color:#fdfcfc\">Open Any App</b><div style=\"color:#7f7a7a;font-size:12px\">Wayland, X11, Electron, Qt/GTK, Java, Flatpak, AppImage &amp; Wine</div></div>\n"
      << "        <div style=\"padding:12px;background:#1b1818;border-right:1px solid #3d3838;border-bottom:1px solid #3d3838\"><b style=\"color:#fdfcfc\">Monochrome Viewers</b><div style=\"color:#7f7a7a;font-size:12px\">imv (images), zathura (PDF/EPUB), mpv (video), file-roller, bat</div></div>\n"
      << "      </div>\n"
      << "    </div>\n"
      << "  </div>\n\n"
      << "  <h2><span class=\"num\">04</span>Keybinds</h2>\n"
      << "  <p class=\"lead\">The main modifier is <kbd>" << html_escape(main_label) << "</kbd> (one line in\n"
      << "  <code>modules/user.lua</code> changes it). Every bind carries a description, so\n"
      << "  <kbd>" << html_escape(main_label) << " + /</kbd> (interactive launcher) and <code>hyprctl binds</code> show the same wording\n"
      << "  as this page. Filter all " << total_binds << " binds live below:</p>\n"
      << "  <input id=\"kb-filter\" class=\"kbsearch\" type=\"search\"\n"
      << "         placeholder=\"Filter " << total_binds << " keybinds (try: palette, zen, resize, capture, ocr, layout, record)…\">\n"
      << "  <div id=\"kb-container\">" << bind_tables << "</div>\n\n"
      << "  <h2><span class=\"num\">05</span>Palette</h2>\n"
      << "  <p class=\"lead\">Greyscale everywhere. Each surface has its own one-line palette so you can shift\n"
      << "  the whole theme by editing a handful of values.</p>\n\n"
      << "  <h3>Menus — rofi/theme.rasi</h3>\n"
      << "  <div class=\"swatches\">" << rofi_sw << "</div>\n\n"
      << "  <div class=\"two\">\n"
      << "    <div>\n"
      << "      <h3>Terminal — kitty/theme.conf</h3>\n"
      << "      <div class=\"swatches\">" << kitty_sw << "</div>\n"
      << "      <div class=\"swatches\">" << kitty_ramp << "</div>\n"
      << "    </div>\n"
      << "    <div>\n"
      << "      <h3>Prompt — starship.toml</h3>\n"
      << "      <div class=\"swatches\">" << star_sw << "</div>\n"
      << "      <h3>Bar — waybar/style.css</h3>\n"
      << "      <div class=\"swatches\">" << wb_sw << "</div>\n"
      << "    </div>\n"
      << "  </div>\n\n"
      << "  <h2><span class=\"num\">06</span>What gets installed</h2>\n"
      << "  <p class=\"lead\">Everything in <code>packages.txt</code> is in the official Arch repositories,\n"
      << "  so the install works on Arch, EndeavourOS, CachyOS and Manjaro without an AUR helper.</p>\n"
      << "  " << pkg_html << "\n"
      << "  <h3>Optional AUR extras</h3>\n"
      << "  <table><tbody>" << aur_rows << "</tbody></table>\n\n"
      << "  <h2><span class=\"num\">07</span>Project layout</h2>\n"
      << "  <p class=\"lead\">Hyprland reads <code>hyprland.lua</code>, which loads the modules beside it —\n"
      << "  so the file you normally open is <code>modules/user.lua</code>. Native C++20 tools build\n"
      << "  the wallpapers, docs and expression engine; shell scripts drive the menus and bar helpers.</p>\n"
      << "  <div class=\"tree\">" << tree_html << "</div>\n\n"
      << "  <h2><span class=\"num\">08</span>Try it</h2>\n"
      << "  <div class=\"grid3\">\n"
      << "    <div class=\"card\"><h4>Install</h4><p><code>./install.sh</code><br>\n"
      << "      Packages, backups (twice), configs, C++20 helpers, wallpapers. <code>--dry-run</code> shows the plan first.</p></div>\n"
      << "    <div class=\"card\"><h4>See the keys</h4><p><kbd>" << html_escape(main_label) << " + /</kbd><br>\n"
      << "      Searchable cheat sheet built from your live binds, or read <code>docs/KEYBINDS.md</code>.</p></div>\n"
      << "    <div class=\"card\"><h4>Make it yours</h4><p><code>modules/user.lua</code><br>\n"
      << "      Programs, gaps, keyboard, paths — the other modules read from it.\n"
      << "      Right now: browser <b>" << html_escape(browser) << "</b>, terminal <b>" << html_escape(terminal) << "</b>.</p></div>\n"
      << "    <div class=\"card\"><h4>Pick a wallpaper</h4><p><kbd>" << html_escape(main_label) << " + W</kbd><br>\n"
      << "      Thumbnail grid over <code>~/Pictures/Wallpapers</code>, or <code>--random</code> on every boot.</p></div>\n"
      << "    <div class=\"card\"><h4>Record it</h4><p><kbd>" << html_escape(main_label) << " + Alt + Print</kbd><br>\n"
      << "    screen recording (again to stop) · <kbd>" << html_escape(main_label) << " + Alt + S</kbd> saves the last minute ·\n"
      << "    gpu-screen-recorder or wf-recorder, whichever you have.</p></div>\n"
      << "    <div class=\"card\"><h4>System settings</h4><p><kbd>" << html_escape(main_label) << " + I</kbd><br>\n"
      << "      Info, Network, Bluetooth, Monitors, Sound, night shift, and every config file.</p></div>\n"
      << "    <div class=\"card\"><h4>Command Palette</h4><p><kbd>" << html_escape(main_label) << " + Shift + P</kbd> or <kbd>" << html_escape(main_label) << " + ;</kbd><br>\n"
      << "      Jump to open windows, trigger focus/layout actions, or type <code>= 1920*1080</code> for C++20 math.</p></div>\n"
      << "    <div class=\"card\"><h4>Deep Work &amp; Zen</h4><p><kbd>" << html_escape(main_label) << " + Z</kbd> · <kbd>" << html_escape(main_label) << " + Shift + Z</kbd><br>\n"
      << "      One-key distraction-free Zen framing + Waybar Pomodoro countdowns &amp; DND.</p></div>\n"
      << "    <div class=\"card\"><h4>Modal Chords</h4><p><kbd>" << html_escape(main_label) << " + R</kbd> · <kbd>" << html_escape(main_label) << " + C</kbd><br>\n"
      << "      <code>RESIZE</code> mode (h/j/k/l + 1-3/0 gap presets) and single-chord <code>CAPTURE</code> + OCR.</p></div>\n"
      << "    <div class=\"card\"><h4>Undo everything</h4><p><code>./uninstall.sh</code><br>\n"
      << "      Removes the config and restores your originals from <code>~/.config-backups/pristine</code>.</p></div>\n"
      << "  </div>\n\n"
      << "  <pre><span class=\"c\"># the short version</span>\n"
      << "git clone &lt;this repo&gt; &amp;&amp; cd hyprland-monochrome\n"
      << "<span class=\"k\">./install.sh</span>            <span class=\"c\"># then log out and back in</span>\n"
      << "<span class=\"k\">make test</span>               <span class=\"c\"># C++20 + shellcheck + lua + json + 178+ smoke checks</span></pre>\n\n"
      << "  <footer>\n"
      << "    Generated from this repository's config on " << today_iso() << " by\n"
      << "    <code>tools/build-preview.cpp</code> · " << total_binds << " binds, " << total_pkgs << " packages, MIT licensed.\n"
      << "    Start reading at <code>README.md</code>; troubleshooting lives in\n"
      << "    <code>docs/TROUBLESHOOTING.md</code>.\n"
      << "  </footer>\n\n"
      << "</div>\n"
      << SIM_JS << "\n"
      << "</body>\n"
      << "</html>\n";
    return o.str();
}

std::string strip_date_stamp(const std::string& page) {
    static const std::regex stamp_re(R"(on \d{4}-\d{2}-\d{2} by)");
    return std::regex_replace(page, stamp_re, "on <date> by");
}

}  // namespace

int main(int argc, char* argv[]) {
    bool check_only = false;
    for (int i = 1; i < argc; ++i) {
        if (std::string(argv[i]) == "--check") check_only = true;
    }

    fs::path repo = mono_cfg::find_repo_root(argv[0]);
    fs::path out = repo / "docs/preview.html";
    std::string page = build_page(repo);

    if (check_only) {
        if (fs::exists(out) && strip_date_stamp(mono_cfg::read_text(out)) == strip_date_stamp(page)) {
            std::cout << "docs/preview.html is up to date.\n";
            return 0;
        }
        std::cerr << "docs/preview.html is OUT OF DATE — run make docs (./bin/build-preview)\n";
        return 1;
    }

    mono_cfg::write_text(out, page);
    std::cout << "wrote docs/preview.html (" << (page.size() / 1024) << " KB)\n";
    return 0;
}
