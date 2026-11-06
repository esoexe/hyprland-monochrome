/* ===========================================================================
   hyprland-monochrome : Live Interactive Website Engine (Vanilla ES2022)
   - 60 FPS cursor-reactive generative monochrome canvas (Flow / Topo / Grid)
   - Live 4-workspace Hyprland 0.56 compositor simulator + GLSL shader studio
   - Interactive Kitty shell + JS port of C++20 mono-calc expression engine
   - Hardware & distro profile configurator + 24-wallpaper studio + 172 keybinds
   - Global Ctrl+K Command Palette
   =========================================================================== */

(function () {
  "use strict";

  const $ = (sel, root = document) => root.querySelector(sel);
  const $$ = (sel, root = document) => Array.from(root.querySelectorAll(sel));

  /* -------------------------------------------------------------------------
   * Toast Helper
   * ----------------------------------------------------------------------- */
  let toastTimer = 0;
  function showToast(msg) {
    const el = $("#toast");
    if (!el) return;
    el.textContent = msg;
    el.classList.add("show");
    clearTimeout(toastTimer);
    toastTimer = window.setTimeout(() => el.classList.remove("show"), 2200);
  }

  function copyText(txt, label) {
    if (navigator.clipboard && navigator.clipboard.writeText) {
      navigator.clipboard.writeText(txt).then(() => showToast(label || "Copied to clipboard"));
    } else {
      showToast(label || "Copied");
    }
  }

  /* -------------------------------------------------------------------------
   * Scroll Luminance Bar, Reveal Choreography & Active Nav Spy
   * ----------------------------------------------------------------------- */
  const progressEl = $("#scroll-progress");
  function updateScrollBar() {
    const max = document.documentElement.scrollHeight - window.innerHeight;
    const ratio = max > 0 ? Math.min(1, Math.max(0, window.scrollY / max)) : 0;
    if (progressEl) progressEl.style.transform = `scaleX(${ratio.toFixed(4)})`;
  }
  window.addEventListener("scroll", updateScrollBar, { passive: true });
  updateScrollBar();

  const revealObs = new IntersectionObserver(
    (entries) => {
      for (const e of entries) {
        if (e.isIntersecting) {
          e.target.classList.add("visible");
          revealObs.unobserve(e.target);
        }
      }
    },
    { threshold: 0.08 }
  );
  $$(".reveal").forEach((el) => revealObs.observe(el));

  const sections = ["simulator", "browser", "hardware", "apps", "wallpapers", "footage", "keybinds"];
  const navSpy = new IntersectionObserver(
    (entries) => {
      entries.forEach((e) => {
        if (e.isIntersecting) {
          const id = e.target.id;
          $$(".nav-link").forEach((a) => {
            a.classList.toggle("active", a.getAttribute("href") === `#${id}`);
          });
        }
      });
    },
    { rootMargin: "-38% 0px -52% 0px" }
  );
  sections.forEach((id) => {
    const sec = document.getElementById(id);
    if (sec) navSpy.observe(sec);
  });

  /* Spotlight Cards mouse glow coordinates */
  document.addEventListener(
    "pointermove",
    (ev) => {
      $$(".spot-card").forEach((card) => {
        const r = card.getBoundingClientRect();
        if (ev.clientY < r.top - 160 || ev.clientY > r.bottom + 160) return;
        card.style.setProperty("--mx", `${ev.clientX - r.left}px`);
        card.style.setProperty("--my", `${ev.clientY - r.top}px`);
      });
    },
    { passive: true }
  );

  /* Install Command Copy */
  $("#copy-install")?.addEventListener("click", () => {
    copyText(
      "git clone https://github.com/esoexe/hyprland-monochrome.git && cd hyprland-monochrome && ./install.sh",
      "Install command copied"
    );
  });

  /* -------------------------------------------------------------------------
   * 60 FPS Generative Monochrome Hero Canvas + Live FPS Telemetry
   * ----------------------------------------------------------------------- */
  const heroCanvas = $("#hero-canvas");
  const fpsEl = $("#fps-meter");
  let canvasMode = "flow";
  let heroVisible = true;
  const pointer = { x: 0.5, y: 0.45, vx: 0, vy: 0 };

  window.addEventListener(
    "pointermove",
    (e) => {
      const nx = e.clientX / Math.max(1, window.innerWidth);
      const ny = e.clientY / Math.max(1, window.innerHeight);
      pointer.vx = nx - pointer.x;
      pointer.vy = ny - pointer.y;
      pointer.x = nx;
      pointer.y = ny;
    },
    { passive: true }
  );

  $$(".c-mode").forEach((btn) => {
    btn.addEventListener("click", () => {
      canvasMode = btn.dataset.cmode || "flow";
      $$(".c-mode").forEach((b) => b.classList.toggle("active", b === btn));
    });
  });

  if (heroCanvas) {
    const ctx = heroCanvas.getContext("2d");
    let w = 0;
    let h = 0;
    const particles = [];
    const PARTICLE_COUNT = 135;

    function resizeHero() {
      const dpr = Math.min(window.devicePixelRatio || 1, 2);
      w = heroCanvas.clientWidth || window.innerWidth;
      h = heroCanvas.clientHeight || window.innerHeight;
      heroCanvas.width = Math.floor(w * dpr);
      heroCanvas.height = Math.floor(h * dpr);
      ctx.setTransform(dpr, 0, 0, dpr, 0, 0);
    }
    resizeHero();
    window.addEventListener("resize", resizeHero);

    for (let i = 0; i < PARTICLE_COUNT; i++) {
      particles.push({
        x: Math.random() * (w || 1200),
        y: Math.random() * (h || 800),
        speed: 0.45 + Math.random() * 0.95,
        len: 8 + Math.random() * 18,
        alpha: 0.14 + Math.random() * 0.42
      });
    }

    const heroVisObs = new IntersectionObserver(
      (entries) => {
        heroVisible = entries[0]?.isIntersecting ?? true;
      },
      { threshold: 0.02 }
    );
    heroVisObs.observe($("#top"));

    let frames = 0;
    let lastFpsTick = performance.now();

    function fieldAngle(x, y, t) {
      const nx = x * 0.0026;
      const ny = y * 0.0026;
      const dx = x / Math.max(1, w) - pointer.x;
      const dy = y / Math.max(1, h) - pointer.y;
      const pull = Math.exp(-(dx * dx + dy * dy) * 9.0) * 2.1;
      return (
        Math.sin(nx * 2.1 + t * 0.00055) * 1.6 +
        Math.cos(ny * 2.4 - t * 0.00045) * 1.6 +
        Math.sin((nx + ny) * 1.5 + t * 0.0003) +
        pull * Math.atan2(dy, dx)
      );
    }

    function renderHero(now) {
      frames++;
      if (now - lastFpsTick >= 650) {
        const fps = Math.min(60, Math.round((frames * 1000) / (now - lastFpsTick)));
        if (fpsEl) fpsEl.textContent = `${fps} FPS`;
        frames = 0;
        lastFpsTick = now;
      }

      if (heroVisible && ctx) {
        ctx.clearRect(0, 0, w, h);

        if (canvasMode === "flow") {
          ctx.lineWidth = 1.1;
          for (const p of particles) {
            const ang = fieldAngle(p.x, p.y, now);
            const vx = Math.cos(ang) * p.speed;
            const vy = Math.sin(ang) * p.speed;
            const x2 = p.x + Math.cos(ang) * p.len;
            const y2 = p.y + Math.sin(ang) * p.len;

            ctx.strokeStyle = `rgba(255,255,255,${p.alpha.toFixed(3)})`;
            ctx.beginPath();
            ctx.moveTo(p.x, p.y);
            ctx.lineTo(x2, y2);
            ctx.stroke();

            p.x += vx;
            p.y += vy;
            if (p.x < -20) p.x = w + 20;
            if (p.x > w + 20) p.x = -20;
            if (p.y < -20) p.y = h + 20;
            if (p.y > h + 20) p.y = -20;
          }
        } else if (canvasMode === "topo") {
          ctx.lineWidth = 1;
          const bands = 13;
          for (let b = 0; b < bands; b++) {
            const baseY = (h / (bands + 1)) * (b + 1);
            const lum = 0.08 + (b / bands) * 0.28;
            ctx.strokeStyle = `rgba(255,255,255,${lum.toFixed(3)})`;
            ctx.beginPath();
            for (let x = 0; x <= w; x += 18) {
              const dx = x / w - pointer.x;
              const dy = baseY / h - pointer.y;
              const cursorWave = Math.exp(-(dx * dx + dy * dy) * 12) * 34;
              const y =
                baseY +
                Math.sin(x * 0.006 + now * 0.0011 + b * 0.65) * 18 +
                Math.cos(x * 0.013 - now * 0.0007 + b) * 9 -
                cursorWave;
              if (x === 0) ctx.moveTo(x, y);
              else ctx.lineTo(x, y);
            }
            ctx.stroke();
          }
        } else if (canvasMode === "grid") {
          const step = 30;
          for (let x = step / 2; x < w; x += step) {
            for (let y = step / 2; y < h; y += step) {
              const dx = x / w - pointer.x;
              const dy = y / h - pointer.y;
              const dist = Math.hypot(dx, dy);
              const wave = 0.5 + 0.5 * Math.sin(dist * 18 - now * 0.0024);
              const boost = Math.exp(-dist * dist * 14);
              const r = 0.8 + (wave * 1.4 + boost * 2.6);
              const a = 0.08 + wave * 0.16 + boost * 0.45;
              ctx.fillStyle = `rgba(255,255,255,${Math.min(0.85, a).toFixed(3)})`;
              ctx.beginPath();
              ctx.arc(x, y, r, 0, Math.PI * 2);
              ctx.fill();
            }
          }
        }
      }

      requestAnimationFrame(renderHero);
    }
    requestAnimationFrame(renderHero);
  }

  /* -------------------------------------------------------------------------
   * JS Port of C++20 mono-calc Recursive-Descent Expression Evaluator
   * ----------------------------------------------------------------------- */
  function evalMonoCalc(expr) {
    const s = String(expr || "").trim();
    if (!s) return null;
    let i = 0;
    const skip = () => {
      while (i < s.length && /\s/.test(s[i])) i++;
    };

    function parseExpr() {
      let v = parseTerm();
      while (true) {
        skip();
        if (s[i] === "+") {
          i++;
          v += parseTerm();
        } else if (s[i] === "-") {
          i++;
          v -= parseTerm();
        } else break;
      }
      return v;
    }

    function parseTerm() {
      let v = parsePower();
      while (true) {
        skip();
        if (s[i] === "*") {
          i++;
          v *= parsePower();
        } else if (s[i] === "/") {
          i++;
          const d = parsePower();
          if (d === 0) throw new Error("div0");
          v /= d;
        } else if (s[i] === "%") {
          i++;
          v %= parsePower();
        } else break;
      }
      return v;
    }

    function parsePower() {
      let base = parseUnary();
      skip();
      if (s[i] === "^") {
        i++;
        base = Math.pow(base, parsePower());
      }
      return base;
    }

    function parseUnary() {
      skip();
      if (s[i] === "+") {
        i++;
        return parseUnary();
      }
      if (s[i] === "-") {
        i++;
        return -parseUnary();
      }
      return parsePrimary();
    }

    function parsePrimary() {
      skip();
      if (s[i] === "(") {
        i++;
        const v = parseExpr();
        skip();
        if (s[i] !== ")") throw new Error("paren");
        i++;
        return v;
      }
      const idMatch = s.slice(i).match(/^[a-zA-Z_]\w*/);
      if (idMatch) {
        const name = idMatch[0].toLowerCase();
        i += name.length;
        if (name === "pi") return Math.PI;
        if (name === "e") return Math.E;
        skip();
        if (s[i] !== "(") throw new Error("fn");
        i++;
        const arg = parseExpr();
        skip();
        if (s[i] !== ")") throw new Error("fn)");
        i++;
        const fns = {
          sqrt: Math.sqrt,
          sin: Math.sin,
          cos: Math.cos,
          tan: Math.tan,
          abs: Math.abs,
          log: Math.log10,
          ln: Math.log,
          floor: Math.floor,
          ceil: Math.ceil,
          round: Math.round
        };
        if (!fns[name]) throw new Error("unknown");
        return fns[name](arg);
      }
      const numMatch = s.slice(i).match(/^(?:\d+\.?\d*|\.\d+)(?:e[+-]?\d+)?/i);
      if (!numMatch) throw new Error("num");
      i += numMatch[0].length;
      return parseFloat(numMatch[0]);
    }

    try {
      const out = parseExpr();
      skip();
      if (i !== s.length || !Number.isFinite(out)) return null;
      return Number(out.toPrecision(10)).toString();
    } catch {
      return null;
    }
  }

  /* -------------------------------------------------------------------------
   * 01. Live Hyprland 0.56 Desktop Simulator
   * ----------------------------------------------------------------------- */
  const osStage = $("#os-stage");
  const osTrack = $("#os-track");
  const wbTitle = $("#wb-win-title");
  const wbShaderPill = $("#wb-shader-pill");
  const wsTitles = [
    "hyprland.conf.lua",
    "monochrome // newtab",
    "rofi-wayland + mono-calc",
    "btop++ + mono-record"
  ];
  const shaders = ["off", "noir", "grain", "crt", "eink", "oled"];
  let currentWs = 0;
  let currentShader = "off";
  const glassModes = ["Frosted", "X-Ray", "Acrylic"];
  let glassIdx = 0;

  function setWorkspace(idx) {
    currentWs = Math.max(0, Math.min(3, idx));
    if (osTrack) osTrack.style.transform = `translate3d(-${currentWs * 25}%, 0, 0)`;
    if (wbTitle) wbTitle.textContent = wsTitles[currentWs];
    $$("[data-ws]").forEach((b) => {
      const match = Number(b.dataset.ws) === currentWs;
      b.classList.toggle("on", match && b.classList.contains("sbtn"));
      b.classList.toggle("active", match && b.classList.contains("wb-ws"));
    });
  }

  $$("[data-ws]").forEach((btn) => {
    btn.addEventListener("click", () => setWorkspace(Number(btn.dataset.ws)));
  });

  function setMotion(m) {
    if (!osStage) return;
    osStage.classList.remove("mot-snappy", "mot-cinematic", "mot-minimal");
    if (m !== "spring") osStage.classList.add(`mot-${m}`);
    $$("[data-motion]").forEach((b) => b.classList.toggle("on", b.dataset.motion === m));
  }
  $$("[data-motion]").forEach((b) => b.addEventListener("click", () => setMotion(b.dataset.motion)));

  function setShader(sh) {
    if (!osStage) return;
    currentShader = sh;
    shaders.forEach((s) => osStage.classList.remove(`sh-${s}`));
    if (sh !== "off") osStage.classList.add(`sh-${sh}`);
    $$("[data-shader]").forEach((b) => b.classList.toggle("on", b.dataset.shader === sh));
    if (wbShaderPill) wbShaderPill.textContent = `SHADER: ${sh.toUpperCase()}`;
  }
  $$("[data-shader]").forEach((b) => b.addEventListener("click", () => setShader(b.dataset.shader)));
  wbShaderPill?.addEventListener("click", () => {
    const next = shaders[(shaders.indexOf(currentShader) + 1) % shaders.length];
    setShader(next);
  });

  $("#sim-glass")?.addEventListener("click", (e) => {
    glassIdx = (glassIdx + 1) % glassModes.length;
    osStage?.classList.remove("glass-xray", "glass-acrylic");
    if (glassIdx === 1) osStage?.classList.add("glass-xray");
    if (glassIdx === 2) osStage?.classList.add("glass-acrylic");
    e.currentTarget.textContent = `Glass: ${glassModes[glassIdx]}`;
  });

  $("#sim-quake")?.addEventListener("click", (e) => {
    const open = $("#os-quake")?.classList.toggle("open");
    e.currentTarget.classList.toggle("on", Boolean(open));
  });

  $("#sim-turbo")?.addEventListener("click", (e) => {
    const on = osStage?.classList.toggle("turbo");
    e.currentTarget.classList.toggle("on", Boolean(on));
  });

  $("#sim-zen")?.addEventListener("click", (e) => {
    const on = osStage?.classList.toggle("zen");
    e.currentTarget.classList.toggle("on", Boolean(on));
  });

  /* Code viewer tabs inside WS 1 */
  const CODE_SNIPPETS = {
    lua: `-- ~/.config/hypr/hyprland.conf.lua (Hyprland 0.56 Lua Engine)
local theme = require("theme")
require("env")         -- Wayland, Qt6, GTK4, Ozone, Java AWT, SDL
require("monitors")    -- Auto-scale any resolution / refresh rate
require("general")     -- Monochrome 1.5px borders & dwindle split
require("decoration")  -- Adaptive blur + frosted / x-ray / acrylic
require("animations")  -- Spring bezier(0.16, 1, 0.3, 1) presets
require("input")       -- Desktop mouse + laptop touchpad gestures
require("rules")       -- Universal Wayland/X11/Steam/Java window rules
require("binds")       -- 172 ergonomic shortcuts + mono-open
pcall(require, "hw-auto")  -- Generated by mono-pc-detect on login`,

    pc: `# ~/.config/hypr/scripts/mono-pc-detect (runs via exec-once)
# Reads /sys/bus/pci/devices/*/class (0x0300 / 0x0302 / 0x0380)
# Writes ~/.config/hypr/lua/hw-auto.lua for this specific machine:

hl.cursor({ no_hardware_cursors = 2 }) -- auto-safe on NVIDIA, HW on AMD/Intel
hl.env("AQ_DRM_DEVICES", "/dev/dri/card1:/dev/dri/card0") -- Hybrid Optimus
hl.render({ explicit_sync = 2, direct_scanout = 1 })
hl.xwayland({ force_zero_scaling = true }) -- Crisp HiDPI X11 apps`,

    glsl: `// ~/.config/hypr/shaders/noir.glsl (Live Compositor Post-Process)
#version 300 es
precision highp float;
in vec2 v_texcoord;
uniform sampler2D tex;
out vec4 fragColor;

void main() {
    vec4 pix = texture(tex, v_texcoord);
    float lum = dot(pix.rgb, vec3(0.2126, 0.7152, 0.0722));
    lum = smoothstep(0.03, 0.97, pow(lum, 1.06));
    fragColor = vec4(vec3(lum), pix.a);
}`
  };

  const simCodeView = $("#sim-code-view");
  if (simCodeView) simCodeView.textContent = CODE_SNIPPETS.lua;
  $$(".wtab").forEach((tab) => {
    tab.addEventListener("click", () => {
      $$(".wtab").forEach((t) => t.classList.toggle("active", t === tab));
      if (simCodeView) simCodeView.textContent = CODE_SNIPPETS[tab.dataset.code] || "";
    });
  });

  /* Film-grain animated shader overlay */
  const grainCanvas = $("#os-grain-canvas");
  if (grainCanvas) {
    const gctx = grainCanvas.getContext("2d");
    window.setInterval(() => {
      if (currentShader !== "grain" || !gctx) return;
      const img = gctx.createImageData(grainCanvas.width, grainCanvas.height);
      const d = img.data;
      for (let i = 0; i < d.length; i += 4) {
        const v = (Math.random() * 255) | 0;
        d[i] = v;
        d[i + 1] = v;
        d[i + 2] = v;
        d[i + 3] = 48;
      }
      gctx.putImageData(img, 0, 0);
    }, 90);
  }

  /* Waybar Live Clock, SVG Sparkline & WS4 Smooth Core Telemetry */
  const wbClock = $("#wb-clock");
  const wbSparkSvg = $("#wb-spark-svg");
  const btopWrap = $("#sim-btop-bars");
  const sparkHeights = [5, 7, 9, 6, 8, 10];

  if (btopWrap) {
    const initLoads = [62, 48, 74, 38, 55, 29, 66, 42];
    btopWrap.innerHTML = initLoads
      .map(
        (pct, idx) =>
          `<div class="core-row">` +
          `<span style="color:#9e9a9a;width:54px">Core ${idx}</span>` +
          `<div class="core-track"><div class="core-fill" data-core="${idx}" style="width:${pct}%"></div></div>` +
          `<span class="core-pct" data-core-pct="${idx}" style="color:#fff;width:36px;text-align:right">${pct}%</span>` +
          `</div>`
      )
      .join("");
  }

  function tickTelemetry() {
    const now = new Date();
    if (wbClock) wbClock.textContent = now.toTimeString().slice(0, 8);

    sparkHeights.shift();
    sparkHeights.push(2 + Math.floor(Math.random() * 10));
    if (wbSparkSvg) {
      const rects = wbSparkSvg.querySelectorAll("rect");
      rects.forEach((rect, idx) => {
        const h = sparkHeights[idx] || 4;
        rect.setAttribute("y", String(12 - h));
        rect.setAttribute("height", String(h));
      });
    }

    if (btopWrap) {
      for (let c = 0; c < 8; c++) {
        const pct = 18 + Math.floor(Math.random() * 72);
        const fill = btopWrap.querySelector(`[data-core="${c}"]`);
        const lbl = btopWrap.querySelector(`[data-core-pct="${c}"]`);
        if (fill) fill.style.width = `${pct}%`;
        if (lbl) lbl.textContent = `${pct}%`;
      }
    }
  }
  tickTelemetry();
  window.setInterval(tickTelemetry, 1200);

  /* WS3 Live Calculator */
  const wsCalcIn = $("#ws-calc-in");
  const wsCalcOut = $("#ws-calc-out");
  wsCalcIn?.addEventListener("input", () => {
    const res = evalMonoCalc(wsCalcIn.value);
    if (wsCalcOut) wsCalcOut.textContent = res !== null ? res : "...";
  });

  /* Interactive Kitty Terminal inside WS 1 */
  const kittyForm = $("#kitty-form");
  const kittyInput = $("#kitty-input");
  const kittyOut = $("#kitty-out");

  function appendKitty(cmd, lines) {
    if (!kittyOut) return;
    const promptRow = document.createElement("div");
    promptRow.innerHTML = `<span style="color:#fff;font-weight:700">&gt;</span> `;
    const span = document.createElement("span");
    span.textContent = cmd;
    promptRow.appendChild(span);
    kittyOut.appendChild(promptRow);

    lines.forEach((ln) => {
      const r = document.createElement("div");
      r.style.color = "#b8b3b3";
      r.textContent = ln;
      kittyOut.appendChild(r);
    });
    kittyOut.scrollTop = kittyOut.scrollHeight;
  }

  kittyForm?.addEventListener("submit", (e) => {
    e.preventDefault();
    const raw = (kittyInput?.value || "").trim();
    if (!raw) return;
    kittyInput.value = "";
    const parts = raw.split(/\s+/);
    const cmd = parts[0].toLowerCase();

    if (cmd === "clear") {
      if (kittyOut) kittyOut.innerHTML = "";
      return;
    }
    if (cmd === "help") {
      appendKitty(raw, [
        "Available commands: fastfetch | calc <expr> | shader <off|noir|grain|crt|eink|oled> | ws <1-4> | clear"
      ]);
    } else if (cmd === "fastfetch" || cmd === "neofetch") {
      appendKitty(raw, [
        "  hyprland-monochrome v5.2.0",
        "  WM:      Hyprland 0.56 (Lua Engine)",
        "  Core:    C++20 + Lua 5.4 + POSIX Sh",
        "  Browser: Monochrome 1px Structural Grid",
        "  GPU:     Auto-Profiled (AMD / Intel / NVIDIA / Hybrid / VM)"
      ]);
    } else if (cmd === "calc") {
      const expr = raw.slice(4).trim();
      const val = evalMonoCalc(expr);
      appendKitty(raw, [val !== null ? `= ${val}` : "Syntax error in expression"]);
    } else if (cmd === "shader") {
      const sh = (parts[1] || "off").toLowerCase();
      if (shaders.includes(sh)) {
        setShader(sh);
        appendKitty(raw, [`Applied live GLSL shader: ${sh}`]);
      } else {
        appendKitty(raw, [`Choose: ${shaders.join(", ")}`]);
      }
    } else if (cmd === "ws") {
      const n = parseInt(parts[1], 10);
      if (n >= 1 && n <= 4) {
        setWorkspace(n - 1);
        appendKitty(raw, [`Switched to workspace ${n}`]);
      } else {
        appendKitty(raw, ["Usage: ws 1..4"]);
      }
    } else {
      const maybeMath = evalMonoCalc(raw);
      if (maybeMath !== null) {
        appendKitty(raw, [`= ${maybeMath}`]);
      } else {
        appendKitty(raw, [`mono-sh: unknown command '${cmd}' (try 'help')`]);
      }
    }
  });

  /* -------------------------------------------------------------------------
   * 02. Monochrome Browser Suite Showcase
   * ----------------------------------------------------------------------- */
  const BROWSER_VIEWS = {
    start: {
      url: "file://~/.config/browser/monochrome/startpage.html",
      code: null
    },
    firefox: {
      url: "~/.mozilla/firefox/<profile>/chrome/userChrome.css",
      code: `/* Firefox / Zen / LibreWolf / Floorp Monochrome Chrome */
:root {
  --toolbar-bgcolor: #131010 !important;
  --lwt-accent-color: #0a0909 !important;
  --tab-selected-bgcolor: #201d1d !important;
  --toolbar-field-background-color: #201d1d !important;
  --toolbar-field-border-color: #3d3838 !important;
  --tab-border-radius: 4px !important;
}`
    },
    ext: {
      url: "~/.config/browser/monochrome/chromium-theme/manifest.json",
      code: `{
  "manifest_version": 3,
  "name": "Monochrome Structural Browser Theme",
  "version": "5.2.0",
  "theme": {
    "colors": {
      "frame": [10, 9, 9],
      "toolbar": [19, 16, 16],
      "omnibox_background": [32, 29, 29],
      "omnibox_text": [253, 252, 252],
      "tab_text": [253, 252, 252]
    }
  }
}`
    },
    qute: {
      url: "~/.config/qutebrowser/config.py",
      code: `# Qutebrowser Monochrome Structural Palette
c.colors.statusbar.normal.bg = "#131010"
c.colors.statusbar.normal.fg = "#FDFCFC"
c.colors.tabs.even.bg        = "#0A0909"
c.colors.tabs.selected.even.bg = "#201D1D"
c.colors.webpage.preferred_color_scheme = "dark"
c.url.start_pages = ["~/.config/browser/monochrome/startpage.html"]`
    }
  };

  const ocBody = $("#oc-body-view");
  const ocUrl = $("#oc-url-pill");
  const ocInitialHtml = ocBody ? ocBody.innerHTML : "";

  function bindOcStartEvents() {
    $$(".oc-swatch").forEach((sw) => {
      sw.addEventListener("click", () => {
        copyText(sw.dataset.hex, `Copied palette token ${sw.dataset.hex}`);
      });
    });
    $("#oc-demo-search")?.addEventListener("submit", (e) => {
      e.preventDefault();
      window.open("newtab.html", "_blank", "noopener");
    });
  }
  bindOcStartEvents();

  $$("[data-btab]").forEach((tab) => {
    tab.addEventListener("click", () => {
      $$("[data-btab]").forEach((t) => t.classList.toggle("active", t === tab));
      const key = tab.dataset.btab;
      const view = BROWSER_VIEWS[key];
      if (!view || !ocBody || !ocUrl) return;
      ocUrl.textContent = view.url;
      if (key === "start") {
        ocBody.innerHTML = ocInitialHtml;
        bindOcStartEvents();
      } else {
        ocBody.innerHTML = `<pre class="pc-code" style="margin:0;border-radius:4px;background:#0d0b0b;border-color:#3d3838">${view.code.replace(/</g, "&lt;")}</pre>`;
      }
    });
  });

  /* -------------------------------------------------------------------------
   * 03. Hardware & Distribution Matrix Configurator
   * ----------------------------------------------------------------------- */
  let selGpu = "hybrid";
  let selForm = "laptop";
  let selDistro = "arch";

  const GPU_PROFILES = {
    hybrid: `-- Dual-GPU Hybrid Laptop (iGPU primary + NVIDIA offload)
hl.env("AQ_DRM_DEVICES", "/dev/dri/card1:/dev/dri/card0")
hl.cursor({ no_hardware_cursors = 2 }) -- Auto-safe on NVIDIA outputs
hl.render({ explicit_sync = 2 })`,
    nvidia: `-- Dedicated NVIDIA RTX / GTX (Proprietary DRM Modeset)
hl.env("LIBVA_DRIVER_NAME", "nvidia")
hl.env("__GLX_VENDOR_LIBRARY_NAME", "nvidia")
hl.env("GBM_BACKEND", "nvidia-drm")
hl.env("NVD_BACKEND", "direct")
hl.cursor({ no_hardware_cursors = 2 })
hl.opengl({ nvidia_anti_flicker = true })`,
    amd: `-- AMD Radeon (Mesa RADV + Hardware Cursors + Direct Scanout)
hl.env("LIBVA_DRIVER_NAME", "radeonsi")
hl.cursor({ no_hardware_cursors = 0 })
hl.render({ explicit_sync = 2, direct_scanout = 1 })`,
    intel: `-- Intel Iris Xe / Arc (iHD VAAPI + Tear-Free Compositing)
hl.env("LIBVA_DRIVER_NAME", "iHD")
hl.cursor({ no_hardware_cursors = 0 })
hl.render({ explicit_sync = 2 })`,
    vm: `-- Virtual Machine (QEMU / KVM / VirtIO / VMware / VirtualBox)
hl.env("WLR_RENDERER_ALLOW_SOFTWARE", "1")
hl.env("AQ_NO_MODIFIERS", "1")
hl.cursor({ no_hardware_cursors = 1 })
hl.decoration({ blur = { enabled = false }, shadow = { enabled = false } })`
  };

  const FORM_PROFILES = {
    laptop: `hl.monitor(",preferred,auto,auto") -- Fractional HiDPI + Crisp XWayland
hl.input({ touchpad = { natural_scroll = true, tap_to_click = true } })
hl.gestures({ workspace_swipe = true, workspace_swipe_fingers = 3 })`,
    desktop: `hl.monitor(",highrr,auto,1") -- Highest refresh rate + VRR
hl.misc({ vrr = 2 })
hl.input({ accel_profile = "flat", follow_mouse = 1 })`,
    ultrawide: `hl.monitor(",preferred,auto,1")
hl.dwindle({ split_width_multiplier = 1.35, preserve_split = true })
hl.general({ gaps_out = 16, gaps_in = 8 })`
  };

  const DISTRO_NOTES = {
    arch: "Installer backend: pacman + yay/paru (Arch, EndeavourOS, CachyOS, Manjaro).",
    fedora: "Installer backend: dnf install hyprland waybar kitty rofi-wayland thunar ...",
    debian: "Installer backend: apt-get install hyprland waybar kitty rofi thunar ...",
    opensuse: "Installer backend: zypper / xbps-install / apk auto-detected via /etc/os-release."
  };

  function updatePcMatrix() {
    const out = $("#pc-code-out");
    const note = $("#pc-install-note");
    if (out) {
      out.textContent =
        `-- Auto-generated by mono-pc-detect on login\n` +
        GPU_PROFILES[selGpu] +
        `\n\n` +
        FORM_PROFILES[selForm] +
        `\nhl.xwayland({ force_zero_scaling = true })`;
    }
    if (note) note.textContent = DISTRO_NOTES[selDistro];
  }

  $$("#gpu-chips .hw-chip").forEach((b) =>
    b.addEventListener("click", () => {
      selGpu = b.dataset.gpu;
      $$("#gpu-chips .hw-chip").forEach((x) => x.classList.toggle("active", x === b));
      updatePcMatrix();
    })
  );
  $$("#form-chips .hw-chip").forEach((b) =>
    b.addEventListener("click", () => {
      selForm = b.dataset.form;
      $$("#form-chips .hw-chip").forEach((x) => x.classList.toggle("active", x === b));
      updatePcMatrix();
    })
  );
  $$("#distro-chips .hw-chip").forEach((b) =>
    b.addEventListener("click", () => {
      selDistro = b.dataset.distro;
      $$("#distro-chips .hw-chip").forEach((x) => x.classList.toggle("active", x === b));
      updatePcMatrix();
    })
  );
  updatePcMatrix();

  /* -------------------------------------------------------------------------
   * 05. Procedural C++20 Wallpaper Studio (20 JPGs + 4 Live GIFs)
   * ----------------------------------------------------------------------- */
  const WALLPAPERS = [
    { file: "mono-live-01.gif", title: "Live 01 : Lunar Eclipse", cat: "live", badge: "LIVE GIF" },
    { file: "mono-live-02.gif", title: "Live 02 : Topo Drift", cat: "live", badge: "LIVE GIF" },
    { file: "mono-live-03.gif", title: "Live 03 : Rain Matrix", cat: "live", badge: "LIVE GIF" },
    { file: "mono-live-04.gif", title: "Live 04 : Pulse Horizon", cat: "live", badge: "LIVE GIF" },
    { file: "mono-01.jpg", title: "01 : Obsidian Mist", cat: "minimal", badge: "1080P" },
    { file: "mono-02.jpg", title: "02 : Brutalist Grid", cat: "geo", badge: "1080P" },
    { file: "mono-03.jpg", title: "03 : Topographic Ridges", cat: "field", badge: "1080P" },
    { file: "mono-04.jpg", title: "04 : Eclipse Corona", cat: "minimal", badge: "1080P" },
    { file: "mono-05.jpg", title: "05 : Architectural Beams", cat: "geo", badge: "1080P" },
    { file: "mono-06.jpg", title: "06 : Fine Halftone", cat: "geo", badge: "1080P" },
    { file: "mono-07.jpg", title: "07 : Silk Flow Field", cat: "field", badge: "1080P" },
    { file: "mono-08.jpg", title: "08 : Zenith Gradient", cat: "minimal", badge: "1080P" },
    { file: "mono-09.jpg", title: "09 : Concentric Orbit", cat: "geo", badge: "1080P" },
    { file: "mono-10.jpg", title: "10 : Silver Dune", cat: "field", badge: "1080P" },
    { file: "mono-11.jpg", title: "11 : Monolith Horizon", cat: "minimal", badge: "DEFAULT" },
    { file: "mono-12.jpg", title: "12 : Isometric Vault", cat: "geo", badge: "1080P" },
    { file: "mono-13.jpg", title: "13 : Interference Wave", cat: "field", badge: "1080P" },
    { file: "mono-14.jpg", title: "14 : Pure OLED Void", cat: "minimal", badge: "OLED" },
    { file: "mono-15.jpg", title: "15 : Vector Contours", cat: "field", badge: "1080P" },
    { file: "mono-16.jpg", title: "16 : Bauhaus Slices", cat: "geo", badge: "1080P" },
    { file: "mono-17.jpg", title: "17 : Film Grain 400", cat: "minimal", badge: "NOIR" },
    { file: "mono-18.jpg", title: "18 : Magnetic Flux", cat: "field", badge: "1080P" },
    { file: "mono-19.jpg", title: "19 : Cathedral Light", cat: "geo", badge: "1080P" },
    { file: "mono-20.jpg", title: "20 : Event Horizon", cat: "minimal", badge: "1080P" }
  ];

  const wallGrid = $("#wall-grid");
  const osWallImg = $("#os-wallpaper");
  const lightboxModal = $("#lightbox-modal");
  const lightboxImg = $("#lightbox-img");
  const lightboxCap = $("#lightbox-caption");
  let activeLightboxUrl = "wallpapers/mono-11.jpg";

  function applyWallpaperToSimulator(url, title) {
    if (osWallImg) osWallImg.src = url;
    showToast(`Applied ${title} to simulator`);
    document.getElementById("simulator")?.scrollIntoView({ behavior: "smooth", block: "center" });
  }

  function renderWallpapers(cat = "all") {
    if (!wallGrid) return;
    const list = cat === "all" ? WALLPAPERS : WALLPAPERS.filter((w) => w.cat === cat);
    wallGrid.innerHTML = "";
    list.forEach((w) => {
      const url = `wallpapers/${w.file}`;
      const card = document.createElement("div");
      card.className = "wall-card";
      card.innerHTML = `
        <div class="wall-thumb-wrap" title="Click for full 1920x1080 preview">
          <img src="${url}" alt="${w.title}" loading="lazy" class="wall-thumb" />
          <span class="wall-badge">${w.badge}</span>
        </div>
        <div class="wall-meta">
          <div>
            <div class="wall-name">${w.title}</div>
            <div class="wall-sub">${w.file}</div>
          </div>
          <button type="button" class="wall-apply">Apply to Desktop</button>
        </div>
      `;
      card.querySelector(".wall-thumb-wrap")?.addEventListener("click", () => {
        activeLightboxUrl = url;
        if (lightboxImg) lightboxImg.src = url;
        if (lightboxCap) lightboxCap.textContent = `${w.title} (${w.file})`;
        lightboxModal?.classList.add("open");
      });
      card.querySelector(".wall-apply")?.addEventListener("click", () => {
        applyWallpaperToSimulator(url, w.title);
      });
      wallGrid.appendChild(card);
    });
  }
  renderWallpapers("all");

  $$("#wall-filters .sbtn").forEach((b) => {
    b.addEventListener("click", () => {
      $$("#wall-filters .sbtn").forEach((x) => x.classList.toggle("on", x === b));
      renderWallpapers(b.dataset.wcat || "all");
    });
  });

  $("#lightbox-apply")?.addEventListener("click", () => {
    lightboxModal?.classList.remove("open");
    applyWallpaperToSimulator(activeLightboxUrl, "selected wallpaper");
  });
  $("#lightbox-close")?.addEventListener("click", () => lightboxModal?.classList.remove("open"));
  lightboxModal?.addEventListener("click", (e) => {
    if (e.target === lightboxModal) lightboxModal.classList.remove("open");
  });

  /* -------------------------------------------------------------------------
   * 07. Searchable 172-Keybind Explorer (Sanitized & Synced)
   * ----------------------------------------------------------------------- */
  let allKeybinds = [];
  let activeKbCat = "All";
  const kbTbody = $("#kb-tbody");
  const kbSearch = $("#kb-search");
  const kbCats = $("#kb-cats");

  function sanitizeLabel(str) {
    return String(str || "").replace(/[\u2014\u2013]/g, " : ");
  }

  function renderKeybinds() {
    if (!kbTbody) return;
    const q = (kbSearch?.value || "").trim().toLowerCase();
    const filtered = allKeybinds.filter((k) => {
      if (activeKbCat !== "All" && k.category !== activeKbCat) return false;
      if (!q) return true;
      return (
        k.shortcut.toLowerCase().includes(q) ||
        k.description.toLowerCase().includes(q) ||
        k.category.toLowerCase().includes(q)
      );
    });

    if (!filtered.length) {
      kbTbody.innerHTML = `<tr><td colspan="3" style="color:var(--ink-muted)">No matching shortcuts found.</td></tr>`;
      return;
    }

    kbTbody.innerHTML = filtered
      .map(
        (k) => `
      <tr>
        <td><span class="keycap">${k.shortcut.replace(/</g, "&lt;")}</span></td>
        <td style="color:var(--ink-muted)">${k.category.replace(/</g, "&lt;")}</td>
        <td style="color:var(--ink)">${k.description.replace(/</g, "&lt;")}</td>
      </tr>`
      )
      .join("");
  }

  function formatModShortcut(row) {
    if (row.shortcut) return String(row.shortcut);
    const mods = [];
    const m = Number(row.modmask || 0);
    if (m & 64) mods.push("Super");
    if (m & 4) mods.push("Ctrl");
    if (m & 8) mods.push("Alt");
    if (m & 1) mods.push("Shift");
    if (row.key) mods.push(String(row.key));
    const combo = mods.join(" + ") || String(row.key || "");
    return row.submap ? `[${row.submap}] ${combo}` : combo;
  }

  fetch("keybinds.json")
    .then((r) => r.json())
    .then((data) => {
      allKeybinds = Array.isArray(data)
        ? data.map((row) => ({
            ...row,
            shortcut: sanitizeLabel(formatModShortcut(row)),
            category: sanitizeLabel(row.category || (row.submap ? `Submap: ${row.submap}` : "Core")),
            description: sanitizeLabel(row.description)
          }))
        : [];
      const topCats = ["All", ...Array.from(new Set(allKeybinds.map((k) => k.category))).slice(0, 6)];
      if (kbCats) {
        kbCats.innerHTML = topCats
          .map(
            (c) =>
              `<button type="button" class="sbtn ${c === "All" ? "on" : ""}" data-kbcat="${c}">${c}</button>`
          )
          .join("");
        $$("[data-kbcat]", kbCats).forEach((btn) => {
          btn.addEventListener("click", () => {
            activeKbCat = btn.dataset.kbcat || "All";
            $$("[data-kbcat]", kbCats).forEach((x) => x.classList.toggle("on", x === btn));
            renderKeybinds();
          });
        });
      }
      renderKeybinds();
    })
    .catch(() => {
      if (kbTbody) {
        kbTbody.innerHTML = `<tr><td colspan="3" style="color:var(--ink-muted)">Run ./bin/serve-site to load keybinds.json.</td></tr>`;
      }
    });

  kbSearch?.addEventListener("input", renderKeybinds);

  /* -------------------------------------------------------------------------
   * Global Ctrl+K Command Palette (Actions + Keybinds + Live mono-calc)
   * ----------------------------------------------------------------------- */
  const cmdkModal = $("#cmdk-modal");
  const palInput = $("#pal-input");
  const palList = $("#pal-list");
  const palMath = $("#pal-math");

  const PALETTE_ACTIONS = [
    { label: "Jump : Live Hyprland 0.56 Desktop Simulator", hint: "Section 01", run: () => (location.hash = "#simulator") },
    { label: "Jump : Monochrome Browser Suite", hint: "Section 02", run: () => (location.hash = "#browser") },
    { label: "Jump : Hardware & Distribution Matrix", hint: "Section 03", run: () => (location.hash = "#hardware") },
    { label: "Jump : Universal Application Compatibility", hint: "Section 04", run: () => (location.hash = "#apps") },
    { label: "Jump : Procedural 1080p Wallpaper Studio", hint: "Section 05", run: () => (location.hash = "#wallpapers") },
    { label: "Jump : 172-Keybind Reference", hint: "Section 07", run: () => (location.hash = "#keybinds") },
    {
      label: "Action : Copy One-Line Install Command",
      hint: "Clipboard",
      run: () =>
        copyText(
          "git clone https://github.com/esoexe/hyprland-monochrome.git && cd hyprland-monochrome && ./install.sh",
          "Install command copied"
        )
    },
    { label: "Action : Open Live Browser Startpage", hint: "newtab.html", run: () => window.open("newtab.html", "_blank", "noopener") },
    { label: "Simulator : Toggle CRT Scanline Shader", hint: "GLSL", run: () => { location.hash = "#simulator"; setShader("crt"); } },
    { label: "Simulator : Toggle Film Noir Shader", hint: "GLSL", run: () => { location.hash = "#simulator"; setShader("noir"); } },
    { label: "Simulator : Switch to Workspace 2 (Browser)", hint: "WS 2", run: () => { location.hash = "#simulator"; setWorkspace(1); } },
    { label: "External : Open GitHub Repository (esoexe/hyprland-monochrome)", hint: "GitHub", run: () => window.open("https://github.com/esoexe/hyprland-monochrome", "_blank", "noopener") }
  ];

  let palFiltered = PALETTE_ACTIONS;
  let palIdx = 0;

  function openPalette() {
    cmdkModal?.classList.add("open");
    if (palInput) {
      palInput.value = "";
      renderPalette("");
      window.setTimeout(() => palInput.focus(), 20);
    }
  }

  function closePalette() {
    cmdkModal?.classList.remove("open");
  }

  function renderPalette(q) {
    if (!palList) return;
    const query = q.trim().toLowerCase();

    const mathVal = evalMonoCalc(q);
    if (palMath) {
      if (mathVal !== null && /[+\-*/^()]/.test(q)) {
        palMath.style.display = "block";
        palMath.textContent = `mono-calc = ${mathVal} (press Enter to copy)`;
      } else {
        palMath.style.display = "none";
      }
    }

    const kbItems = query
      ? allKeybinds
          .filter((k) => k.shortcut.toLowerCase().includes(query) || k.description.toLowerCase().includes(query))
          .slice(0, 8)
          .map((k) => ({
            label: `${k.description}`,
            hint: k.shortcut,
            run: () => copyText(k.shortcut, `Shortcut: ${k.shortcut}`)
          }))
      : [];

    palFiltered = [
      ...PALETTE_ACTIONS.filter(
        (a) => !query || a.label.toLowerCase().includes(query) || a.hint.toLowerCase().includes(query)
      ),
      ...kbItems
    ];
    palIdx = 0;

    palList.innerHTML = palFiltered
      .map(
        (item, idx) =>
          `<li class="pal-item ${idx === 0 ? "active" : ""}" data-pidx="${idx}">` +
          `<span>${item.label.replace(/</g, "&lt;")}</span>` +
          `<span style="opacity:0.65;font-size:11px">${item.hint.replace(/</g, "&lt;")}</span>` +
          `</li>`
      )
      .join("");

    $$(".pal-item", palList).forEach((li) => {
      li.addEventListener("click", () => {
        const item = palFiltered[Number(li.dataset.pidx)];
        closePalette();
        item?.run();
      });
    });
  }

  $("#open-cmdk")?.addEventListener("click", openPalette);
  cmdkModal?.addEventListener("click", (e) => {
    if (e.target === cmdkModal) closePalette();
  });

  palInput?.addEventListener("input", () => renderPalette(palInput.value));
  palInput?.addEventListener("keydown", (e) => {
    if (e.key === "ArrowDown") {
      e.preventDefault();
      palIdx = Math.min(Math.max(0, palFiltered.length - 1), palIdx + 1);
    } else if (e.key === "ArrowUp") {
      e.preventDefault();
      palIdx = Math.max(0, palIdx - 1);
    } else if (e.key === "Enter") {
      e.preventDefault();
      const mathVal = evalMonoCalc(palInput.value);
      if (mathVal !== null && /[+\-*/^()]/.test(palInput.value) && palFiltered.length === 0) {
        copyText(mathVal, `Copied ${mathVal}`);
        closePalette();
        return;
      }
      const chosen = palFiltered[palIdx];
      closePalette();
      chosen?.run();
      return;
    }
    $$(".pal-item", palList).forEach((li, i) => li.classList.toggle("active", i === palIdx));
  });

  window.addEventListener("keydown", (e) => {
    if ((e.ctrlKey || e.metaKey) && e.key.toLowerCase() === "k") {
      e.preventDefault();
      if (cmdkModal?.classList.contains("open")) closePalette();
      else openPalette();
    } else if (e.key === "Escape") {
      closePalette();
      lightboxModal?.classList.remove("open");
    }
  });
})();
