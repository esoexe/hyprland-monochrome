/* ===========================================================================
   hyprland-monochrome : Live Interactive Website Engine (Vanilla ES2022)
   - 60 FPS cursor-reactive generative monochrome canvas (Flow / Topo / Grid)
   - Live 4-workspace Hyprland 0.56 compositor simulator + GLSL shader studio
   - Interactive Kitty shell + JS port of Pure C11 mono-calc, mono-ipc & mono-tui
   - Hardware & distro profile configurator + 20-wallpaper studio + 176 keybinds
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

  const sections = ["simulator", "browser", "hardware", "apps", "wallpapers", "keybinds"];
  if ("IntersectionObserver" in window) {
    const revealObs = new window.IntersectionObserver(
      (entries) => {
        for (const e of entries) {
          if (e.isIntersecting) {
            e.target.classList.remove("reveal-pending");
            e.target.classList.add("visible");
            revealObs.unobserve(e.target);
          }
        }
      },
      { threshold: 0.08 }
    );
    $$(".reveal").forEach((el) => {
      el.classList.add("reveal-pending");
      revealObs.observe(el);
    });

    const navSpy = new window.IntersectionObserver(
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
  } else {
    // Progressive enhancement: never hide content when observers are unavailable.
    $$(".reveal").forEach((el) => el.classList.add("visible"));
  }

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

    if ("IntersectionObserver" in window) {
      const heroVisObs = new window.IntersectionObserver(
        (entries) => {
          heroVisible = entries[0]?.isIntersecting ?? true;
        },
        { threshold: 0.02 }
      );
      heroVisObs.observe($("#top"));
    }

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
   * JS Port of Pure C11 mono-calc Recursive-Descent Expression Evaluator
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
require("binds")       -- 176 ergonomic shortcuts + mono-open
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
        "Available commands: fastfetch | mono-tui | mono-ipc | calc <expr> | shader <off|noir|grain|crt|eink|oled> | ws <1-4> | clear"
      ]);
    } else if (cmd === "mono-tui") {
      appendKitty(raw, [
        "  +-- hyprland-monochrome v5.3.0 // Pure ISO C11 Control Deck --+",
        "  | [1:TELEMETRY]  2:VELOCITY  3:SHADERS  4:INPUT  5:WALLS  6:BENCH",
        "  | CPU  14%   RAM  4.1 / 16.0 GiB (25%)   IPC: .socket.sock LIVE",
        "  | Bench: /proc 50.8 us  |  state 14.3 us  |  AF_UNIX 0.13 us",
        "  +-------------------------------------------------------------+"
      ]);
    } else if (cmd === "mono-ipc") {
      appendKitty(raw, [
        "  mono-ipc (Pure ISO C11 AF_UNIX .socket.sock / .socket2.sock)",
        "  -> j/version      : v0.56.0-lua",
        "  -> .socket2.sock  : streaming (0 jq subprocesses)"
      ]);
    } else if (cmd === "fastfetch" || cmd === "neofetch") {
      appendKitty(raw, [
        "  hyprland-monochrome v5.3.0",
        "  WM:      Hyprland 0.56 (Lua Engine)",
        "  Core:    Pure C11 + Lua 5.4 + POSIX Sh",
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
  "version": "5.3.0",
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
   * 05. Procedural Pure C11 Live Wallpaper Studio (All 20 Seamless Live Loops)
   * ----------------------------------------------------------------------- */
  const WALLPAPERS = [
    { file: "mono-live-01-orbital.gif", title: "Live 01 : Orbital Rings", cat: "live", badge: "LIVE LOOP" },
    { file: "mono-live-02-pulse.gif", title: "Live 02 : Harmonic Pulse", cat: "live", badge: "LIVE LOOP" },
    { file: "mono-live-03-lissajous.gif", title: "Live 03 : Lissajous Scope", cat: "live", badge: "LIVE LOOP" },
    { file: "mono-01.gif", title: "01 : Noir Vignette", cat: "minimal", badge: "LIVE LOOP" },
    { file: "mono-02.gif", title: "02 : Architectural Grid", cat: "geo", badge: "LIVE LOOP" },
    { file: "mono-03.gif", title: "03 : Concentric Arcs", cat: "geo", badge: "LIVE LOOP" },
    { file: "mono-04.gif", title: "04 : Topographic Contours", cat: "field", badge: "LIVE LOOP" },
    { file: "mono-05.gif", title: "05 : Diagonal Strata", cat: "geo", badge: "LIVE LOOP" },
    { file: "mono-06.gif", title: "06 : Horizon Mist", cat: "field", badge: "LIVE LOOP" },
    { file: "mono-07.gif", title: "07 : Solar Eclipse", cat: "minimal", badge: "LIVE LOOP" },
    { file: "mono-08.gif", title: "08 : Alpine Ridgeline", cat: "field", badge: "LIVE LOOP" },
    { file: "mono-09.gif", title: "09 : Brutalist Monoliths", cat: "geo", badge: "LIVE LOOP" },
    { file: "mono-10.gif", title: "10 : Caustic Silk Ribbon", cat: "field", badge: "LIVE LOOP" },
    { file: "mono-11.gif", title: "11 : Gravitational Singularity", cat: "minimal", badge: "LIVE DEFAULT" },
    { file: "mono-12.gif", title: "12 : Liquid Mercury", cat: "field", badge: "LIVE LOOP" },
    { file: "mono-13.gif", title: "13 : Rain & Bokeh Noir", cat: "minimal", badge: "LIVE NOIR" },
    { file: "mono-14.gif", title: "14 : Bauhaus Compass", cat: "live", badge: "LIVE LOOP" },
    { file: "mono-07-uhd.gif", title: "07 Cinema : Solar Eclipse", cat: "uhd", badge: "LIVE CINEMA" },
    { file: "mono-11-uhd.gif", title: "11 Cinema : Singularity", cat: "uhd", badge: "LIVE CINEMA" },
    { file: "mono-06-uhd.gif", title: "06 Cinema : Horizon Mist", cat: "uhd", badge: "LIVE CINEMA" }
  ];

  const wallGrid = $("#wall-grid");
  const osWallImg = $("#os-wallpaper");
  const lightboxModal = $("#lightbox-modal");
  const lightboxImg = $("#lightbox-img");
  const lightboxCap = $("#lightbox-caption");
  const inlineWalls = window.__INLINE_WALLS__ || null;
  let activeLightboxUrl = (inlineWalls && inlineWalls["mono-11.gif"]) || "wallpapers/mono-11.gif";
  if (inlineWalls && inlineWalls["mono-11.gif"] && osWallImg) {
    osWallImg.src = inlineWalls["mono-11.gif"];
  }

  function resolveWallUrl(file) {
    if (inlineWalls && inlineWalls[file]) return inlineWalls[file];
    return `wallpapers/${file}`;
  }

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
      const url = resolveWallUrl(w.file);
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

  function loadKeybindsData(data) {
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
  }

  if (Array.isArray(window.__INLINE_KEYBINDS__)) {
    loadKeybindsData(window.__INLINE_KEYBINDS__);
  } else {
    fetch("keybinds.json")
      .then((r) => r.json())
      .then(loadKeybindsData)
      .catch(() => {
        if (kbTbody) {
          kbTbody.innerHTML = `<tr><td colspan="3" style="color:var(--ink-muted)">Run ./bin/serve-site to load keybinds.json.</td></tr>`;
        }
      });
  }

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
    { label: "Jump : Pure ISO C11 Core, AF_UNIX IPC & mono-tui", hint: "Section 01B", run: () => (location.hash = "#c11-runtime") },
    { label: "TUI : Open Pure C11 CLOCK_MONOTONIC Benchmark", hint: "mono-tui Tab 6", run: () => { location.hash = "#c11-runtime"; window.__setMonoTuiTab?.(5); } },
    { label: "IPC : Inspect .socket2.sock Live Event Stream", hint: "mono-ipc", run: () => { location.hash = "#c11-runtime"; window.__setMonoIpcQuery?.("socket2"); } },
    { label: "Jump : Monochrome Browser Suite", hint: "Section 02", run: () => (location.hash = "#browser") },
    { label: "Jump : Hardware & Distribution Matrix", hint: "Section 03", run: () => (location.hash = "#hardware") },
    { label: "Jump : Universal Application Compatibility", hint: "Section 04", run: () => (location.hash = "#apps") },
    { label: "Jump : Procedural Live Wallpaper Studio", hint: "Section 05", run: () => (location.hash = "#wallpapers") },
    { label: "Jump : Input & Cursor Studio", hint: "Section 03B", run: () => (location.hash = "#input-studio") },
    { label: "Cursor : Apply Monochrome-Obsidian Pointer", hint: "Live Page", run: () => window.__setMonoCursor?.("obsidian") },
    { label: "Cursor : Apply Monochrome-Ivory Pointer", hint: "Live Page", run: () => window.__setMonoCursor?.("ivory") },
    { label: "Cursor : Apply Monochrome-Precision Reticle", hint: "Live Page", run: () => window.__setMonoCursor?.("precision") },
    { label: "Cursor : Apply Monochrome-Dot Halo", hint: "Live Page", run: () => window.__setMonoCursor?.("dot") },
    { label: "Keyboard : 60% / HHKB Mode (Caps=Esc)", hint: "Visualizer", run: () => { location.hash = "#input-studio"; window.__setMonoKbType?.("compact60"); } },
    { label: "Keyboard : Colemak-DH Ergonomic Matrix", hint: "Visualizer", run: () => { location.hash = "#input-studio"; window.__setMonoKbLayout?.("colemak-dh"); } },
    { label: "Jump : 176-Keybind Reference", hint: "Section 07", run: () => (location.hash = "#keybinds") },
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

  /* -------------------------------------------------------------------------
   * 08. Bespoke Monochrome Cursor Designer & Keyboard Type Studio
   * ----------------------------------------------------------------------- */
  let curTheme = "obsidian";
  let curSize = 24;
  let curKitty = "block";
  let curPageActive = true;

  const CURSOR_THEME_NAMES = {
    obsidian: "Monochrome-Obsidian",
    ivory: "Monochrome-Ivory",
    precision: "Monochrome-Precision",
    dot: "Monochrome-Dot"
  };

  const curStyleEl = document.createElement("style");
  curStyleEl.id = "mono-live-cursor-style";
  document.head.appendChild(curStyleEl);

  function buildCursorSvgUri(theme, shape, sz) {
    const fill = theme === "ivory" ? "#f7f7f7" : theme === "obsidian" ? "#0a0a0a" : "#f4f4f4";
    const stroke = theme === "ivory" ? "#0a0a0a" : theme === "obsidian" ? "#f5f5f5" : "#0c0c0c";
    let body = "";
    let hx = Math.round(sz * 0.19);
    let hy = Math.round(sz * 0.13);

    if (shape === "default") {
      if (theme === "dot") {
        hx = Math.round(sz * 0.5);
        hy = Math.round(sz * 0.5);
        body =
          `<circle cx="32" cy="32" r="16" fill="none" stroke="${stroke}" stroke-width="4" opacity="0.55"/>` +
          `<circle cx="32" cy="32" r="16" fill="none" stroke="${fill}" stroke-width="2.2"/>` +
          `<circle cx="32" cy="32" r="7" fill="${fill}" stroke="${stroke}" stroke-width="1.8"/>`;
      } else if (theme === "precision") {
        body =
          `<polygon points="12,8 12,50 24,39 32,54 37,51 29,36 44,36" fill="none" stroke="${stroke}" stroke-width="4.4" stroke-linejoin="round"/>` +
          `<polygon points="12,8 12,50 24,39 32,54 37,51 29,36 44,36" fill="none" stroke="${fill}" stroke-width="2.1" stroke-linejoin="round"/>` +
          `<circle cx="12" cy="8" r="2.5" fill="#ffffff"/>`;
      } else {
        body = `<polygon points="12,8 12,50 24,39 32,54 37,51 29,36 44,36" fill="${fill}" stroke="${stroke}" stroke-width="2.6" stroke-linejoin="round"/>`;
      }
    } else if (shape === "pointer") {
      if (theme === "dot") {
        hx = Math.round(sz * 0.5);
        hy = Math.round(sz * 0.5);
        body =
          `<circle cx="32" cy="32" r="20" fill="none" stroke="${fill}" stroke-width="2.4"/>` +
          `<circle cx="32" cy="32" r="9.5" fill="${fill}" stroke="${stroke}" stroke-width="1.8"/>`;
      } else {
        hx = Math.round(sz * 0.38);
        hy = Math.round(sz * 0.15);
        body =
          `<polygon points="23,9 28,9 28,27 46,27 46,50 19,50 14,38 18,33 23,37" fill="${theme === "precision" ? "none" : fill}" stroke="${stroke}" stroke-width="2.6" stroke-linejoin="round"/>` +
          (theme === "precision"
            ? `<polygon points="23,9 28,9 28,27 46,27 46,50 19,50 14,38 18,33 23,37" fill="none" stroke="${fill}" stroke-width="1.6" stroke-linejoin="round"/>`
            : "");
      }
    } else if (shape === "text") {
      hx = Math.round(sz * 0.5);
      hy = Math.round(sz * 0.5);
      body =
        `<path d="M32 13V51M24 13H40M24 51H40" fill="none" stroke="${stroke}" stroke-width="5" stroke-linecap="round"/>` +
        `<path d="M32 13V51M24 13H40M24 51H40" fill="none" stroke="${fill}" stroke-width="2.4" stroke-linecap="round"/>`;
    } else if (shape === "crosshair") {
      hx = Math.round(sz * 0.5);
      hy = Math.round(sz * 0.5);
      body =
        `<path d="M10 32H25M39 32H54M32 10V25M32 39V54" fill="none" stroke="${stroke}" stroke-width="4.6" stroke-linecap="round"/>` +
        `<path d="M10 32H25M39 32H54M32 10V25M32 39V54" fill="none" stroke="${fill}" stroke-width="2.2" stroke-linecap="round"/>` +
        `<circle cx="32" cy="32" r="2.2" fill="${fill}"/>`;
    } else if (shape === "ew-resize") {
      hx = Math.round(sz * 0.5);
      hy = Math.round(sz * 0.5);
      body =
        `<path d="M12 32H52M32 22V42M20 24L12 32L20 40M44 24L52 32L44 40" fill="none" stroke="${stroke}" stroke-width="4.8" stroke-linecap="round" stroke-linejoin="round"/>` +
        `<path d="M12 32H52M32 22V42M20 24L12 32L20 40M44 24L52 32L44 40" fill="none" stroke="${fill}" stroke-width="2.3" stroke-linecap="round" stroke-linejoin="round"/>`;
    } else {
      hx = Math.round(sz * 0.5);
      hy = Math.round(sz * 0.5);
      body =
        `<circle cx="32" cy="32" r="17" fill="none" stroke="${stroke}" stroke-width="4.8" opacity="0.55"/>` +
        `<circle cx="32" cy="32" r="17" fill="none" stroke="${fill}" stroke-width="2.5"/>` +
        `<circle cx="32" cy="15" r="4.6" fill="${fill}" stroke="${stroke}" stroke-width="1.6"/>`;
    }

    const svg = `<svg xmlns="http://www.w3.org/2000/svg" width="${sz}" height="${sz}" viewBox="0 0 64 64">${body}</svg>`;
    return `url("data:image/svg+xml,${encodeURIComponent(svg)}") ${hx} ${hy}`;
  }

  function syncCursorStudio() {
    const fullName = CURSOR_THEME_NAMES[curTheme] || "Monochrome-Obsidian";
    const badge = $("#cur-active-badge");
    if (badge) badge.textContent = `${fullName} / ${curSize}px`;

    const cli = $("#cur-cli-out");
    const cmdText =
      `cursor.sh theme ${curTheme} && cursor.sh size ${curSize}` +
      (curKitty !== "block" ? ` && cursor.sh kitty ${curKitty}` : "");
    if (cli) cli.textContent = cmdText;

    $$("#cursor-cards .cur-card").forEach((b) =>
      b.classList.toggle("active", b.dataset.cur === curTheme)
    );
    $$("#cur-size-chips .hw-chip").forEach((b) =>
      b.classList.toggle("active", Number(b.dataset.csize) === curSize)
    );
    $$("#cur-kitty-chips .hw-chip").forEach((b) =>
      b.classList.toggle("active", b.dataset.ckitty === curKitty)
    );

    if (!curPageActive) {
      curStyleEl.textContent = "";
      return;
    }

    const dDef = buildCursorSvgUri(curTheme, "default", curSize);
    const dPtr = buildCursorSvgUri(curTheme, "pointer", curSize);
    const dTxt = buildCursorSvgUri(curTheme, "text", curSize);
    const dCrs = buildCursorSvgUri(curTheme, "crosshair", curSize);
    const dRsz = buildCursorSvgUri(curTheme, "ew-resize", curSize);
    const dWat = buildCursorSvgUri(curTheme, "wait", curSize);

    curStyleEl.textContent =
      `html, body { cursor: ${dDef}, auto; }\n` +
      `a, button, [role="button"], .cur-card, .hw-chip, .cur-zone[data-czone="pointer"] { cursor: ${dPtr}, pointer !important; }\n` +
      `input[type="text"], input[type="search"], pre, code, .cur-zone[data-czone="text"] { cursor: ${dTxt}, text !important; }\n` +
      `.cur-zone[data-czone="default"] { cursor: ${dDef}, default !important; }\n` +
      `.cur-zone[data-czone="crosshair"] { cursor: ${dCrs}, crosshair !important; }\n` +
      `.cur-zone[data-czone="ew-resize"] { cursor: ${dRsz}, ew-resize !important; }\n` +
      `.cur-zone[data-czone="wait"] { cursor: ${dWat}, wait !important; }\n`;
  }

  window.__setMonoCursor = (theme) => {
    curTheme = theme;
    curPageActive = true;
    syncCursorStudio();
    showToast(`Applied ${CURSOR_THEME_NAMES[theme] || theme} (${curSize}px)`);
  };

  $$("#cursor-cards .cur-card").forEach((btn) => {
    btn.addEventListener("click", () => {
      curTheme = btn.dataset.cur || "obsidian";
      curPageActive = true;
      syncCursorStudio();
      showToast(`Live pointer: ${CURSOR_THEME_NAMES[curTheme]}`);
    });
  });

  $$("#cur-size-chips .hw-chip").forEach((btn) => {
    btn.addEventListener("click", () => {
      curSize = Number(btn.dataset.csize) || 24;
      curPageActive = true;
      syncCursorStudio();
    });
  });

  $$("#cur-kitty-chips .hw-chip").forEach((btn) => {
    btn.addEventListener("click", () => {
      curKitty = btn.dataset.ckitty || "block";
      syncCursorStudio();
    });
  });

  $("#cur-reset-page")?.addEventListener("click", () => {
    curPageActive = false;
    syncCursorStudio();
    showToast("Restored browser system cursor");
  });

  $("#cur-copy-cmd")?.addEventListener("click", () => {
    const txt = $("#cur-cli-out")?.textContent || "";
    copyText(`~/.config/hypr/scripts/${txt}`, "Copied cursor.sh command");
  });

  syncCursorStudio();

  // Physical Keyboard Type & Keymap Visualizer
  let kbType = "ansi";
  let kbLayout = "qwerty";
  let kbRate = "tactile";

  const KB_TYPES = {
    ansi:      { model: "pc104",         opts: "",                                    caps: "CAPS",  esc: "ESC",  lsuper: "SUPER", lalt: "ALT", enter: "ENTER" },
    compact60: { model: "pc104",         opts: "caps:escape",                         caps: "ESC*",  esc: "` ~",  lsuper: "SUPER", lalt: "ALT", enter: "ENTER" },
    vim:       { model: "pc104",         opts: "caps:swapescape,shift:both_capslock", caps: "ESC*",  esc: "CAPS*",lsuper: "SUPER", lalt: "ALT", enter: "ENTER" },
    emacs:     { model: "pc104",         opts: "ctrl:nocaps",                         caps: "CTRL*", esc: "ESC",  lsuper: "SUPER", lalt: "ALT", enter: "ENTER" },
    iso:       { model: "pc105",         opts: "",                                    caps: "CAPS",  esc: "ESC",  lsuper: "SUPER", lalt: "ALT", enter: "ISO RET" },
    apple:     { model: "applealu_ansi", opts: "altwin:swap_lalt_lwin",               caps: "CAPS",  esc: "ESC",  lsuper: "OPT*",  lalt: "CMD*",enter: "RETURN" }
  };

  const KB_LAYOUTS = {
    "qwerty":     { xkb: "us", variant: "",           badge: "US",     rows: ["QWERTYUIOP", "ASDFGHJKL;", "ZXCVBNM,./"] },
    "colemak-dh": { xkb: "us", variant: "colemak_dh", badge: "CMK-DH", rows: ["QWFPBJLUY;", "ARSTGMNEIO", "ZXCDVKH,./"] },
    "dvorak":     { xkb: "us", variant: "dvorak",     badge: "DVK",    rows: ["',.PYFGCRL", "AOEUIDHTNS", ";QJKXBMWVZ"] },
    "workman":    { xkb: "us", variant: "workman",    badge: "WRK",    rows: ["QDRWBJFUP;", "ASHTGYNEOI", "ZXMCVKL,./"] },
    "azerty":     { xkb: "fr", variant: "",           badge: "FR",     rows: ["AZERTYUIOP", "QSDFGHJKLM", "WXCVBN,;:!"] },
    "qwertz":     { xkb: "de", variant: "",           badge: "DE",     rows: ["QWERTZUIOP", "ASDFGHJKL#", "YXCVBNM,.-"] }
  };

  const KB_RATES = {
    tactile: { hz: 38, delay: 260 },
    rapid:   { hz: 55, delay: 190 },
    gentle:  { hz: 25, delay: 360 }
  };

  const kbdDeck = $("#kbd-deck");

  function renderKeyboardStudio() {
    const t = KB_TYPES[kbType] || KB_TYPES.ansi;
    const l = KB_LAYOUTS[kbLayout] || KB_LAYOUTS.qwerty;
    const r = KB_RATES[kbRate] || KB_RATES.tactile;

    const pill = $("#kb-waybar-pill");
    if (pill) pill.textContent = `[${l.badge}] ${kbType.toUpperCase()} / ${r.hz}Hz`;

    const luaOut = $("#kb-lua-out");
    if (luaOut) {
      luaOut.textContent =
        `input = { kb_model = "${t.model}", kb_layout = "${l.xkb}", ` +
        `kb_variant = "${l.variant}",\n` +
        `          kb_options = "${t.opts}", repeat_rate = ${r.hz}, repeat_delay = ${r.delay} }`;
    }

    const cliOut = $("#kb-cli-out");
    if (cliOut) {
      cliOut.textContent = `keyboard.sh type ${kbType} && keyboard.sh layout ${kbLayout} && keyboard.sh rate ${kbRate}`;
    }

    $$("#kb-type-chips .hw-chip").forEach((b) =>
      b.classList.toggle("active", b.dataset.kbtype === kbType)
    );
    $$("#kb-layout-chips .hw-chip").forEach((b) =>
      b.classList.toggle("active", b.dataset.kblayout === kbLayout)
    );
    $$("#kb-rate-chips .hw-chip").forEach((b) =>
      b.classList.toggle("active", b.dataset.kbrate === kbRate)
    );

    if (!kbdDeck) return;

    const r1 = [
      { lbl: t.esc, u: 1.15, mod: true, remap: t.esc.includes("*"), code: "Escape" },
      ..."1234567890".split("").map((d) => ({ lbl: d, u: 1, code: `Digit${d}` })),
      { lbl: "-", u: 1, code: "Minus" },
      { lbl: "BKSP", u: 1.85, mod: true, code: "Backspace" }
    ];

    const r2 = [
      { lbl: "TAB", u: 1.4, mod: true, code: "Tab" },
      ...l.rows[0].split("").map((c, idx) => ({
        lbl: c,
        u: 1,
        remap: kbLayout !== "qwerty" && c !== "QWERTYUIOP"[idx],
        code: `Key${"QWERTYUIOP"[idx] || "Q"}`
      })),
      { lbl: "[", u: 1, code: "BracketLeft" },
      { lbl: kbType === "iso" ? "#" : "\\", u: 1.6, mod: kbType === "iso", remap: kbType === "iso", code: "Backslash" }
    ];

    const r3 = [
      { lbl: t.caps, u: 1.7, mod: true, remap: t.caps.includes("*"), code: "CapsLock" },
      ...l.rows[1].split("").map((c, idx) => ({
        lbl: c,
        u: 1,
        remap: kbLayout !== "qwerty" && c !== "ASDFGHJKL;"[idx],
        code: idx < 9 ? `Key${"ASDFGHJKL"[idx]}` : "Semicolon"
      })),
      { lbl: t.enter, u: 2.3, mod: true, remap: kbType === "iso", code: "Enter" }
    ];

    const r4 = [
      { lbl: "SHIFT", u: kbType === "iso" ? 1.25 : 2.15, mod: true, code: "ShiftLeft" },
      ...(kbType === "iso" ? [{ lbl: "< >", u: 0.9, remap: true, code: "IntlBackslash" }] : []),
      ...l.rows[2].split("").map((c, idx) => ({
        lbl: c,
        u: 1,
        remap: kbLayout !== "qwerty" && c !== "ZXCVBNM,./"[idx],
        code: idx < 7 ? `Key${"ZXCVBNM"[idx]}` : "Period"
      })),
      { lbl: "SHIFT", u: 1.85, mod: true, code: "ShiftRight" }
    ];

    const r5 = [
      { lbl: "CTRL", u: 1.35, mod: true, code: "ControlLeft" },
      { lbl: t.lsuper, u: 1.35, mod: true, remap: t.lsuper.includes("*"), code: "MetaLeft" },
      { lbl: t.lalt, u: 1.35, mod: true, remap: t.lalt.includes("*"), code: "AltLeft" },
      { lbl: `${l.badge} SPACE`, u: 5.6, mod: false, code: "Space" },
      { lbl: "ALTGR", u: 1.35, mod: true, code: "AltRight" },
      { lbl: kbType === "compact60" ? "FN" : "CTRL", u: 1.35, mod: true, remap: kbType === "compact60", code: "ControlRight" }
    ];

    kbdDeck.innerHTML = [r1, r2, r3, r4, r5]
      .map(
        (row) =>
          `<div class="kbd-row">` +
          row
            .map(
              (k) =>
                `<div class="kcap${k.mod ? " mod" : ""}${k.remap ? " remapped" : ""}" ` +
                `style="--u:${k.u}" data-kcode="${k.code}">${k.lbl.replace(/</g, "&lt;")}</div>`
            )
            .join("") +
          `</div>`
      )
      .join("");
  }

  window.__setMonoKbType = (tp) => {
    kbType = tp;
    renderKeyboardStudio();
  };
  window.__setMonoKbLayout = (ly) => {
    kbLayout = ly;
    renderKeyboardStudio();
  };

  $$("#kb-type-chips .hw-chip").forEach((btn) => {
    btn.addEventListener("click", () => {
      kbType = btn.dataset.kbtype || "ansi";
      renderKeyboardStudio();
    });
  });

  $$("#kb-layout-chips .hw-chip").forEach((btn) => {
    btn.addEventListener("click", () => {
      kbLayout = btn.dataset.kblayout || "qwerty";
      renderKeyboardStudio();
    });
  });

  $$("#kb-rate-chips .hw-chip").forEach((btn) => {
    btn.addEventListener("click", () => {
      kbRate = btn.dataset.kbrate || "tactile";
      renderKeyboardStudio();
    });
  });

  $("#kb-copy-cmd")?.addEventListener("click", () => {
    const txt = $("#kb-cli-out")?.textContent || "";
    copyText(`~/.config/hypr/scripts/${txt}`, "Copied keyboard.sh command");
  });

  window.addEventListener("keydown", (e) => {
    if (!kbdDeck) return;
    const cap = kbdDeck.querySelector(`[data-kcode="${e.code}"]`);
    if (cap) cap.classList.add("pressed");
  });

  window.addEventListener("keyup", (e) => {
    if (!kbdDeck) return;
    const cap = kbdDeck.querySelector(`[data-kcode="${e.code}"]`);
    if (cap) cap.classList.remove("pressed");
  });

  renderKeyboardStudio();

  /* -------------------------------------------------------------------------
   * 01B. Interactive Pure ISO C11 Control Deck (mono-tui) & AF_UNIX mono-ipc
   * ----------------------------------------------------------------------- */
  const tuiBody = $("#tui-body");
  const tuiFeedback = $("#tui-feedback");
  const tuiStatusPill = $("#tui-status-pill");
  const tuiCliOut = $("#tui-cli-out");
  const tuiTerminal = $("#tui-terminal");
  const ipcOutBox = $("#ipc-out-box");
  const ipcSockNode = $("#ipc-sock-node");
  const ipcLatBadge = $("#ipc-lat-badge");
  const ipcWireBar = $("#ipc-wire-bar");

  let tuiTab = 0;
  let tuiWaveHist = [
    12, 14, 18, 15, 11, 13, 21, 26, 19, 14, 12, 16, 18, 14, 11, 15, 22, 28, 24, 17, 13, 12,
    15, 19, 16, 13, 11, 14, 20, 25, 18, 15, 13, 16, 21, 17, 14, 12, 15, 19, 23, 18, 14, 16
  ];
  let tuiCorePct = [18, 12, 24, 9, 15, 11, 21, 8];
  let benchNumbers = { proc: 50.87, cfg: 14.36, ipc: 0.13 };
  const WAVE_CHARS = ["▁", "▂", "▃", "▄", "▅", "▆", "▇", "█"];

  const TUI_ACTIONS = {
    1: [
      { lbl: "Motion : Fluid Silk Spring", det: "mass=1.0, stiff=175, damp=22", cli: "motion.sh set fluid", fn: () => setMotionLabel("fluid") },
      { lbl: "Motion : Snappy Studio", det: "160ms zero-overshoot curve", cli: "motion.sh set snappy", fn: () => setMotionLabel("snappy") },
      { lbl: "Glass  : 2-Pass Kawase", det: "frosted obsidian glass", cli: "perf.sh glass kawase", fn: () => setGlassLabel("kawase") },
      { lbl: "Glass  : Liquid Acrylic Lens", det: "refraction=18.0, aberration=0.0", cli: "perf.sh glass acrylic", fn: () => setGlassLabel("acrylic") },
      { lbl: "Turbo  : Direct Scanout Mode", det: "Super + F12 zero-latency VRR", cli: "perf.sh turbo toggle", fn: () => toggleTuiTurbo() }
    ],
    2: [
      { lbl: "Shader : Off (Native Passthrough)", det: "0 GPU post-processing cost", cli: "shader.sh off", fn: () => setShader("off") },
      { lbl: "Shader : Noir Luminance", det: "Rec.709 greyscale + S-curve", cli: "shader.sh set noir", fn: () => setShader("noir") },
      { lbl: "Shader : 35mm Cinema Grain", det: "silver-halide organic grain", cli: "shader.sh set cinema-grain", fn: () => setShader("grain") },
      { lbl: "Shader : P4 Studio CRT", det: "phosphor scanlines + lens", cli: "shader.sh set crt-studio", fn: () => setShader("crt") },
      { lbl: "Shader : Matte Paper E-Ink", det: "daylight reflective contrast", cli: "shader.sh set paper-eink", fn: () => setShader("eink") },
      { lbl: "Shader : OLED Pitch True-Black", det: "pixel-off deep contrast", cli: "shader.sh set oled-pitch", fn: () => setShader("oled") }
    ],
    3: [
      { lbl: "Cursor : Monochrome-Obsidian", det: ".hlc vector + Xcursor 24px", cli: "cursor.sh theme Monochrome-Obsidian", fn: () => window.__setMonoCursor?.("obsidian") },
      { lbl: "Cursor : Monochrome-Ivory", det: "high-contrast light pointer", cli: "cursor.sh theme Monochrome-Ivory", fn: () => window.__setMonoCursor?.("ivory") },
      { lbl: "Cursor : Monochrome-Precision", det: "CAD crosshair reticle", cli: "cursor.sh theme Monochrome-Precision", fn: () => window.__setMonoCursor?.("precision") },
      { lbl: "Cursor : Monochrome-Dot", det: "minimalist telemetry halo", cli: "cursor.sh theme Monochrome-Dot", fn: () => window.__setMonoCursor?.("dot") },
      { lbl: "Keybd  : 60% / HHKB Mode", det: "CapsLock -> Escape (caps:escape)", cli: "keyboard.sh type compact60", fn: () => window.__setMonoKbType?.("compact60") },
      { lbl: "Layout : Colemak-DH Matrix", det: "ergonomic home-row keymap", cli: "keyboard.sh layout colemak-dh", fn: () => window.__setMonoKbLayout?.("colemak-dh") }
    ],
    4: [
      { lbl: "Loop 01 : Orbital Rings", det: "wallpapers/mono-live-01-orbital.gif", cli: "wallpaper.sh set mono-live-01-orbital.gif", fn: () => applySimWall("mono-live-01-orbital.gif") },
      { lbl: "Loop 07 : Solar Eclipse", det: "wallpapers/mono-07.gif", cli: "wallpaper.sh set mono-07.gif", fn: () => applySimWall("mono-07.gif") },
      { lbl: "Loop 11 : Gravitational Singularity", det: "wallpapers/mono-11.gif", cli: "wallpaper.sh set mono-11.gif", fn: () => applySimWall("mono-11.gif") },
      { lbl: "Loop 12 : Liquid Mercury", det: "wallpapers/mono-12.gif", cli: "wallpaper.sh set mono-12.gif", fn: () => applySimWall("mono-12.gif") },
      { lbl: "Loop 14 : Bauhaus Compass", det: "wallpapers/mono-14.gif", cli: "wallpaper.sh set mono-14.gif", fn: () => applySimWall("mono-14.gif") }
    ]
  };

  let tuiMotion = "fluid";
  let tuiGlass = "kawase";
  let tuiTurbo = false;

  function setMotionLabel(m) { tuiMotion = m; }
  function setGlassLabel(g) { tuiGlass = g; }
  function toggleTuiTurbo() {
    tuiTurbo = !tuiTurbo;
    osStage?.classList.toggle("turbo", tuiTurbo);
    $("#sim-turbo")?.classList.toggle("on", tuiTurbo);
  }
  function applySimWall(file) {
    if (osWallImg) osWallImg.src = resolveWallUrl(file);
    showToast(`Applied ${file} to desktop simulator`);
  }

  function renderMonoTui() {
    if (!tuiBody) return;
    $$("#tui-tabs .tui-tab").forEach((b, idx) => {
      const names = ["1:TELEMETRY", "2:VELOCITY", "3:SHADERS", "4:INPUT+CURSOR", "5:WALLPAPERS", "6:BENCHMARK"];
      b.classList.toggle("active", idx === tuiTab);
      b.textContent = idx === tuiTab ? `[${names[idx]}]` : names[idx];
    });

    if (tuiTab === 0) {
      const avgCpu = Math.round(tuiCorePct.reduce((a, b) => a + b, 0) / tuiCorePct.length);
      const waveStr = tuiWaveHist
        .map((v) => WAVE_CHARS[Math.min(7, Math.max(0, Math.floor((v * 7) / 100)))])
        .join("");
      const coresHtml = tuiCorePct
        .map(
          (p, i) =>
            `<div class="tui-meter-row"><span style="width:44px;color:var(--ink-muted)">core${i}</span>` +
            `<div class="tui-bar-track"><div class="tui-bar-fill" style="width:${p}%"></div></div>` +
            `<span style="width:32px;text-align:right">${p}%</span></div>`
        )
        .join("");

      tuiBody.innerHTML =
        `<div class="tui-meter-row"><span style="font-weight:700;color:var(--pure)">CPU TOTAL</span>` +
        `<div class="tui-bar-track"><div class="tui-bar-fill" style="width:${avgCpu}%"></div></div>` +
        `<span style="font-weight:700;color:var(--pure)">${avgCpu}%  /  46.0 C</span></div>` +
        `<div class="tui-wave" aria-label="CPU rolling waveform">${waveStr}</div>` +
        `<div class="tui-cores-grid">${coresHtml}</div>` +
        `<div class="tui-meter-row"><span style="font-weight:700;color:var(--pure)">MEMORY</span>` +
        `<div class="tui-bar-track"><div class="tui-bar-fill" style="width:26%"></div></div>` +
        `<span style="color:var(--ink-2)">4.2 / 16.0 GiB (26%)</span></div>` +
        `<div class="tui-kv-box">` +
        `<div>Motion: <b>${tuiMotion}</b>  /  Glass: <b>${tuiGlass}</b></div>` +
        `<div>Turbo: <b>${tuiTurbo ? "ON (Direct Scanout)" : "off"}</b></div>` +
        `<div>Shader: <b>${currentShader}</b>  /  Cursor: <b>${curTheme}</b></div>` +
        `<div>Keymap: <b>${kbType} / ${kbLayout}</b></div>` +
        `</div>`;
      if (tuiCliOut) tuiCliOut.textContent = "~/.config/hypr/bin/mono-tui --snapshot";
    } else if (tuiTab >= 1 && tuiTab <= 4) {
      const items = TUI_ACTIONS[tuiTab] || [];
      tuiBody.innerHTML = items
        .map(
          (it, idx) =>
            `<button type="button" class="tui-action-row" data-tuiact="${idx}">` +
            `<span>${it.lbl}</span><span>${it.det}</span></button>`
        )
        .join("");
      $$("#tui-body .tui-action-row").forEach((row) => {
        row.addEventListener("click", () => {
          const idx = parseInt(row.dataset.tuiact || "0", 10);
          const item = items[idx];
          if (!item) return;
          item.fn();
          if (tuiFeedback) tuiFeedback.textContent = `Applied: ${item.lbl}`;
          if (tuiCliOut) tuiCliOut.textContent = `~/.config/hypr/scripts/${item.cli}`;
          renderMonoTui();
        });
      });
    } else {
      const totalUs = (benchNumbers.proc + benchNumbers.cfg + benchNumbers.ipc).toFixed(2);
      tuiBody.innerHTML =
        `<div style="color:var(--pure);font-weight:700;font-size:11.5px">PURE ISO C11 MICROSECOND RUNTIME AUDIT (CLOCK_MONOTONIC)</div>` +
        `<div class="tui-action-row"><span>/proc/stat + /proc/meminfo + /sys/bus/pci</span><span style="color:var(--pure);font-weight:700">${benchNumbers.proc.toFixed(2)} us</span></div>` +
        `<div class="tui-action-row"><span>XDG state &amp; compositor profile reader</span><span style="color:var(--pure);font-weight:700">${benchNumbers.cfg.toFixed(2)} us</span></div>` +
        `<div class="tui-action-row"><span>Hyprland AF_UNIX .socket.sock stat probe</span><span style="color:var(--pure);font-weight:700">${benchNumbers.ipc.toFixed(2)} us</span></div>` +
        `<div class="tui-action-row"><span>Total in-process C11 frame budget</span><span style="color:var(--pure);font-weight:700">${totalUs} us  (0 forks)</span></div>` +
        `<div class="tui-kv-box">` +
        `<div>Compiler: <b>cc -std=c11 -O3 -Werror</b></div>` +
        `<div>Runtime Spawns: <b>0 Python / 0 C++ / 0 jq</b></div>` +
        `</div>`;
      if (tuiCliOut) tuiCliOut.textContent = "~/.config/hypr/bin/mono-tui --bench";
    }
  }

  function runTuiBenchmark() {
    benchNumbers = {
      proc: +(44 + Math.random() * 11).toFixed(2),
      cfg: +(11 + Math.random() * 5).toFixed(2),
      ipc: +(0.10 + Math.random() * 0.09).toFixed(2)
    };
    if (tuiStatusPill) tuiStatusPill.textContent = `CLOCK_MONOTONIC : ${benchNumbers.proc.toFixed(1)} us`;
    if (tuiFeedback) tuiFeedback.textContent = "CLOCK_MONOTONIC audit updated";
    renderMonoTui();
  }

  window.__setMonoTuiTab = (idx) => {
    tuiTab = Math.max(0, Math.min(5, idx));
    renderMonoTui();
  };

  $$("#tui-tabs .tui-tab").forEach((btn) => {
    btn.addEventListener("click", () => {
      tuiTab = parseInt(btn.dataset.tuitab || "0", 10);
      renderMonoTui();
    });
  });

  tuiTerminal?.addEventListener("keydown", (e) => {
    if (e.key >= "1" && e.key <= "6") {
      tuiTab = parseInt(e.key, 10) - 1;
      renderMonoTui();
      e.preventDefault();
    } else if (e.key === "Tab") {
      tuiTab = (tuiTab + 1) % 6;
      renderMonoTui();
      e.preventDefault();
    }
  });

  $("#tui-rebench-btn")?.addEventListener("click", () => {
    tuiTab = 5;
    runTuiBenchmark();
  });

  $("#tui-copy-cmd")?.addEventListener("click", () => {
    copyText(tuiCliOut?.textContent || "~/.config/hypr/bin/mono-tui", "Copied mono-tui command");
  });

  window.setInterval(() => {
    if (document.hidden) return;
    const next = Math.max(6, Math.min(48, Math.round(16 + Math.sin(Date.now() * 0.0023) * 9 + (Math.random() - 0.5) * 8)));
    tuiWaveHist.shift();
    tuiWaveHist.push(next);
    tuiCorePct = tuiCorePct.map((v, i) =>
      Math.max(4, Math.min(62, Math.round(v * 0.65 + (10 + ((next + i * 5) % 28)) * 0.35)))
    );
    if (tuiTab === 0) renderMonoTui();
  }, 900);

  const IPC_PAYLOADS = {
    version: {
      sock: "$HIS/.socket.sock",
      lat: "0.07 ms round-trip",
      body:
        `$ mono-ipc raw "j/version"\n` +
        `{\n  "branch": "main",\n  "tag": "v0.56.0",\n  "flags": ["lua", "hyprcursor", "aquamarine"]\n}`
    },
    activewindow: {
      sock: "$HIS/.socket.sock",
      lat: "0.08 ms round-trip",
      body:
        `$ mono-ipc raw "j/activewindow"\n` +
        `{\n  "class": "kitty",\n  "title": "nvim .config/hypr/src/mono-ipc.c",\n  "workspace": { "id": 1, "name": "1" },\n  "floating": false,\n  "at": [14, 52],\n  "size": [939, 1014]\n}`
    },
    sysgraph: {
      sock: "/proc/stat + /proc/meminfo",
      lat: "0.05 ms C11 native",
      body:
        `$ mono-ipc sysgraph\n` +
        `{"text":" ▂▃▄▃▂▃ 16%","tooltip":"CPU 16%  ·  load 0.24 0.19 0.14\\nRAM 4.2 / 16.0 GiB (26%)\\nZero-fork Pure ISO C11 telemetry"}`
    },
    socket2: {
      sock: "$HIS/.socket2.sock",
      lat: "0.00 ms push stream",
      body:
        `$ mono-ipc watch\n` +
        `workspacev2>>1,1\n` +
        `activewindow>>kitty,mono-tui\n` +
        `activelayout>>at-translated-set-2-keyboard,English (Colemak-DH)\n` +
        `screencastv2>>1,monitor,DP-1  [Dunst auto-paused]`
    },
    binds: {
      sock: "$HIS/.socket.sock",
      lat: "0.11 ms sorted C11",
      body:
        `$ mono-ipc binds-rofi | head -n 4\n` +
        `Super + B                     Browser              exec  open-app.sh browser\n` +
        `Super + Ctrl + C              Open Cursor Studio   exec  cursor.sh menu\n` +
        `Super + Ctrl + M              Open Control Deck    exec  tui.sh\n` +
        `Super + Return                Terminal (Kitty)     exec  kitty --single-instance`
    },
    float: {
      sock: "$HIS/.socket.sock",
      lat: "0.08 ms C11 geometry",
      body:
        `$ mono-ipc float-target\n` +
        `true 1344 756 288 162\n` +
        `# 70% logical monitor box centered in 1 C11 call (replaces jq + awk)`
    }
  };

  function selectIpcQuery(key) {
    const item = IPC_PAYLOADS[key] || IPC_PAYLOADS.version;
    $$("#ipc-query-chips .hw-chip").forEach((b) => b.classList.toggle("active", b.dataset.ipc === key));
    if (ipcSockNode) ipcSockNode.textContent = item.sock;
    if (ipcLatBadge) ipcLatBadge.textContent = item.lat;
    if (ipcOutBox) ipcOutBox.textContent = item.body;
    if (ipcWireBar) {
      ipcWireBar.classList.remove("flash");
      void ipcWireBar.offsetWidth;
      ipcWireBar.classList.add("flash");
    }
  }

  window.__setMonoIpcQuery = selectIpcQuery;

  $$("#ipc-query-chips .hw-chip").forEach((btn) => {
    btn.addEventListener("click", () => selectIpcQuery(btn.dataset.ipc || "version"));
  });

  renderMonoTui();
  selectIpcQuery("version");
})();
