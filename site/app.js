/* ═══════════════════════════════════════════════════════════════════════════
   hyprland-monochrome — Live Animated Website Engine (app.js)
   60 FPS Generative Canvas · Interactive Hyprland 0.56 Simulator · ⌘K Palette
   ═══════════════════════════════════════════════════════════════════════════ */
(function () {
  'use strict';

  var toastEl = document.getElementById('toast');
  var toastTimer = null;
  function showToast(msg) {
    if (!toastEl) return;
    toastEl.textContent = msg;
    toastEl.classList.add('show');
    clearTimeout(toastTimer);
    toastTimer = setTimeout(function () {
      toastEl.classList.remove('show');
    }, 2100);
  }

  function copyText(txt, label) {
    if (navigator.clipboard && navigator.clipboard.writeText) {
      navigator.clipboard.writeText(txt).then(function () {
        showToast(label || 'Copied to clipboard');
      }).catch(function () {
        showToast(label || 'Copied');
      });
    } else {
      showToast(label || 'Ready');
    }
  }

  /* ── 1. Scroll Progress, Active Section Spy & Reveal Choreography ──────── */
  var progEl = document.getElementById('scroll-progress');
  var navLinks = Array.prototype.slice.call(document.querySelectorAll('.nav-link'));
  var sections = navLinks.map(function (a) {
    return document.querySelector(a.getAttribute('href'));
  });

  function onScroll() {
    var max = document.documentElement.scrollHeight - window.innerHeight;
    var pct = max > 0 ? Math.min(1, Math.max(0, window.scrollY / max)) : 0;
    if (progEl) progEl.style.transform = 'scaleX(' + pct.toFixed(4) + ')';

    var mid = window.scrollY + window.innerHeight * 0.34;
    var activeIdx = 0;
    for (var i = 0; i < sections.length; i++) {
      if (sections[i] && sections[i].offsetTop <= mid) activeIdx = i;
    }
    navLinks.forEach(function (lnk, idx) {
      lnk.classList.toggle('active', idx === activeIdx);
    });
  }
  window.addEventListener('scroll', onScroll, { passive: true });
  onScroll();

  var revealObs = new IntersectionObserver(function (entries) {
    entries.forEach(function (e) {
      if (e.isIntersecting) {
        e.target.classList.add('visible');
        revealObs.unobserve(e.target);
      }
    });
  }, { threshold: 0.10 });

  document.querySelectorAll('.reveal').forEach(function (el) {
    revealObs.observe(el);
  });

  /* Radial spotlight cursor coordinates on .spot-card */
  document.addEventListener('pointermove', function (ev) {
    var cards = document.querySelectorAll('.spot-card');
    for (var i = 0; i < cards.length; i++) {
      var r = cards[i].getBoundingClientRect();
      if (ev.clientY >= r.top - 120 && ev.clientY <= r.bottom + 120) {
        cards[i].style.setProperty('--mx', (ev.clientX - r.left) + 'px');
        cards[i].style.setProperty('--my', (ev.clientY - r.top) + 'px');
      }
    }
  }, { passive: true });

  /* Copy Install Command */
  var copyInstallBtn = document.getElementById('copy-install-btn');
  if (copyInstallBtn) {
    copyInstallBtn.addEventListener('click', function () {
      var cmd = 'git clone https://github.com/esoexe/hyprland-monochrome.git && cd hyprland-monochrome && ./install.sh';
      copyText(cmd, 'Copied install command!');
      copyInstallBtn.textContent = 'Copied ✓';
      setTimeout(function () { copyInstallBtn.textContent = 'Copy'; }, 1800);
    });
  }

  /* ── 2. 60 FPS Generative Hero Canvas (Orrery · Singularity · Lissajous) ─ */
  var heroCanvas = document.getElementById('hero-canvas');
  var hCtx = heroCanvas ? heroCanvas.getContext('2d') : null;
  var heroMode = 'orrery';
  var heroVisible = true;
  var ptrX = 0, ptrY = 0, smoothX = 0, smoothY = 0;

  function resizeHero() {
    if (!heroCanvas) return;
    var dpr = Math.min(window.devicePixelRatio || 1, 2);
    heroCanvas.width = Math.floor(heroCanvas.clientWidth * dpr);
    heroCanvas.height = Math.floor(heroCanvas.clientHeight * dpr);
    if (hCtx) hCtx.setTransform(dpr, 0, 0, dpr, 0, 0);
  }
  window.addEventListener('resize', resizeHero);
  resizeHero();

  window.addEventListener('pointermove', function (e) {
    ptrX = (e.clientX / Math.max(1, window.innerWidth) - 0.5) * 2;
    ptrY = (e.clientY / Math.max(1, window.innerHeight) - 0.5) * 2;
  }, { passive: true });

  document.querySelectorAll('.c-mode').forEach(function (btn) {
    btn.addEventListener('click', function () {
      document.querySelectorAll('.c-mode').forEach(function (b) { b.classList.remove('active'); });
      btn.classList.add('active');
      heroMode = btn.getAttribute('data-hmode') || 'orrery';
    });
  });

  if (heroCanvas) {
    var heroObs = new IntersectionObserver(function (entries) {
      heroVisible = entries[0].isIntersecting;
    }, { threshold: 0.02 });
    heroObs.observe(heroCanvas);
  }

  /* FPS Telemetry Pill */
  var fpsBadge = document.getElementById('fps-badge');
  var frameCount = 0, lastFpsCheck = performance.now();

  function drawHero(now) {
    requestAnimationFrame(drawHero);

    frameCount++;
    if (now - lastFpsCheck >= 1000) {
      var fps = Math.min(120, Math.round((frameCount * 1000) / (now - lastFpsCheck)));
      if (fpsBadge) fpsBadge.textContent = fps + ' FPS · 0-fork';
      frameCount = 0;
      lastFpsCheck = now;
    }

    if (!heroVisible || !hCtx || !heroCanvas) return;

    var w = heroCanvas.clientWidth;
    var h = heroCanvas.clientHeight;
    smoothX += (ptrX - smoothX) * 0.05;
    smoothY += (ptrY - smoothY) * 0.05;

    var cx = w * 0.5 + smoothX * 28;
    var cy = h * 0.46 + smoothY * 20;
    var t = now * 0.001;
    var rBase = Math.min(w, h) * 0.36;

    hCtx.clearRect(0, 0, w, h);

    /* Subtle architectural crosshair grid */
    hCtx.strokeStyle = 'rgba(255,255,255,0.035)';
    hCtx.lineWidth = 1;
    hCtx.beginPath();
    hCtx.moveTo(0, cy); hCtx.lineTo(w, cy);
    hCtx.moveTo(cx, 0); hCtx.lineTo(cx, h);
    hCtx.stroke();

    if (heroMode === 'orrery') {
      var rings = [0.34, 0.56, 0.78, 1.02, 1.28];
      for (var i = 0; i < rings.length; i++) {
        var rr = rBase * rings[i];
        hCtx.strokeStyle = i % 2 === 0 ? 'rgba(255,255,255,0.13)' : 'rgba(255,255,255,0.06)';
        hCtx.lineWidth = 1;
        hCtx.beginPath();
        hCtx.arc(cx, cy, rr, 0, Math.PI * 2);
        hCtx.stroke();

        var ang = t * (0.42 - i * 0.065) * (i % 2 === 0 ? 1 : -1) + i * 1.4;
        var px = cx + Math.cos(ang) * rr;
        var py = cy + Math.sin(ang) * rr;

        /* Sweeping arc wake */
        hCtx.strokeStyle = 'rgba(255,255,255,0.34)';
        hCtx.lineWidth = 1.6;
        hCtx.beginPath();
        hCtx.arc(cx, cy, rr, ang - 0.55, ang);
        hCtx.stroke();

        /* Satellite node */
        hCtx.fillStyle = '#ffffff';
        hCtx.beginPath();
        hCtx.arc(px, py, i === 1 ? 4.2 : 2.8, 0, Math.PI * 2);
        hCtx.fill();
      }
      var coreGrad = hCtx.createRadialGradient(cx, cy, 2, cx, cy, rBase * 0.25);
      coreGrad.addColorStop(0, 'rgba(255,255,255,0.28)');
      coreGrad.addColorStop(1, 'rgba(255,255,255,0)');
      hCtx.fillStyle = coreGrad;
      hCtx.beginPath();
      hCtx.arc(cx, cy, rBase * 0.25, 0, Math.PI * 2);
      hCtx.fill();
    } else if (heroMode === 'singularity') {
      /* Relativistic Photon Ring + Doppler Beaming */
      var rad = rBase * 0.68;
      var glow = hCtx.createRadialGradient(cx - rad * 0.18, cy, rad * 0.35, cx, cy, rad * 1.65);
      glow.addColorStop(0, 'rgba(0,0,0,1)');
      glow.addColorStop(0.45, 'rgba(255,255,255,0.32)');
      glow.addColorStop(0.54, 'rgba(255,255,255,0.08)');
      glow.addColorStop(1, 'rgba(0,0,0,0)');
      hCtx.fillStyle = glow;
      hCtx.beginPath();
      hCtx.arc(cx, cy, rad * 1.65, 0, Math.PI * 2);
      hCtx.fill();

      /* Accretion disk ellipse */
      hCtx.save();
      hCtx.translate(cx, cy);
      hCtx.rotate(-0.24 + smoothX * 0.08);
      for (var k = 0; k < 5; k++) {
        hCtx.strokeStyle = 'rgba(255,255,255,' + (0.36 - k * 0.06).toFixed(2) + ')';
        hCtx.lineWidth = 1.5;
        hCtx.beginPath();
        hCtx.ellipse(0, 0, rad * (1.25 + k * 0.12), rad * (0.24 + k * 0.03), 0, 0, Math.PI * 2);
        hCtx.stroke();
      }
      hCtx.restore();

      hCtx.fillStyle = '#050505';
      hCtx.beginPath();
      hCtx.arc(cx, cy, rad * 0.56, 0, Math.PI * 2);
      hCtx.fill();
      hCtx.strokeStyle = 'rgba(255,255,255,0.68)';
      hCtx.lineWidth = 1.4;
      hCtx.stroke();
    } else {
      /* P4 White-Phosphor Harmonic Lissajous Oscilloscope */
      hCtx.strokeStyle = 'rgba(255,255,255,0.42)';
      hCtx.lineWidth = 1.6;
      hCtx.beginPath();
      var steps = 420;
      var ax = rBase * 1.15;
      var ay = rBase * 0.78;
      for (var s = 0; s <= steps; s++) {
        var u = (s / steps) * Math.PI * 2;
        var lx = cx + Math.sin(3 * u + t * 0.9) * ax;
        var ly = cy + Math.sin(4 * u + t * 0.45 + smoothX * 0.6) * ay;
        if (s === 0) hCtx.moveTo(lx, ly);
        else hCtx.lineTo(lx, ly);
      }
      hCtx.stroke();
    }
  }
  requestAnimationFrame(drawHero);

  /* ── 3. Interactive Hyprland 0.56 Desktop Simulator ────────────────────── */
  var osStage = document.getElementById('os-stage');
  var osTrack = document.getElementById('os-track');
  var wbTitle = document.getElementById('wb-title');
  var wbSpark = document.getElementById('wb-spark');
  var wbClock = document.getElementById('wb-clock');
  var wbPomo = document.getElementById('wb-pomo');
  var wbTurboPill = document.getElementById('wb-turbo-pill');
  var simOcClock = document.getElementById('sim-oc-clock');

  var wsTitles = [
    'nvim · modules/hardware.lua',
    'opencode // newtab — Firefox Wayland',
    'open-app.sh picker · Universal Opener',
    'btop++ · 8-Core Monochrome Telemetry'
  ];

  function setWorkspace(idx) {
    if (!osTrack) return;
    osTrack.style.transform = 'translate3d(-' + (idx * 25) + '%, 0, 0)';
    if (wbTitle) wbTitle.textContent = wsTitles[idx] || wsTitles[0];
    document.querySelectorAll('[data-ws]').forEach(function (btn) {
      var match = Number(btn.getAttribute('data-ws')) === idx;
      btn.classList.toggle('active', match);
      btn.classList.toggle('on', match && btn.classList.contains('sbtn'));
    });
  }

  document.querySelectorAll('[data-ws]').forEach(function (btn) {
    btn.addEventListener('click', function () {
      setWorkspace(Number(btn.getAttribute('data-ws')) || 0);
    });
  });

  /* Motion Physics Controls */
  document.querySelectorAll('[data-mot]').forEach(function (btn) {
    btn.addEventListener('click', function () {
      document.querySelectorAll('[data-mot]').forEach(function (b) { b.classList.remove('on'); });
      btn.classList.add('on');
      if (!osStage) return;
      osStage.classList.remove('mot-snappy', 'mot-cinematic', 'mot-minimal');
      var m = btn.getAttribute('data-mot');
      if (m && m !== 'fluid') osStage.classList.add('mot-' + m);
    });
  });

  /* Optical Glass Controls */
  document.querySelectorAll('[data-glass]').forEach(function (btn) {
    btn.addEventListener('click', function () {
      document.querySelectorAll('[data-glass]').forEach(function (b) { b.classList.remove('on'); });
      btn.classList.add('on');
      if (!osStage) return;
      osStage.classList.remove('glass-xray', 'glass-acrylic');
      var g = btn.getAttribute('data-glass');
      if (g && g !== 'kawase') osStage.classList.add('glass-' + g);
    });
  });

  /* GLSL Shader Controls + 35mm Grain Canvas */
  var grainCanvas = document.getElementById('os-grain');
  var gCtx = grainCanvas ? grainCanvas.getContext('2d') : null;
  var activeShader = 'off';

  function setShader(sh) {
    activeShader = sh;
    document.querySelectorAll('[data-shader]').forEach(function (b) {
      b.classList.toggle('on', b.getAttribute('data-shader') === sh);
    });
    if (!osStage) return;
    osStage.classList.remove('sh-noir', 'sh-grain', 'sh-crt', 'sh-eink', 'sh-oled');
    if (sh && sh !== 'off') osStage.classList.add('sh-' + sh);
  }

  document.querySelectorAll('[data-shader]').forEach(function (btn) {
    btn.addEventListener('click', function () {
      setShader(btn.getAttribute('data-shader') || 'off');
    });
  });

  setInterval(function () {
    if (activeShader !== 'grain' || !gCtx || !grainCanvas) return;
    var w = grainCanvas.width, h = grainCanvas.height;
    var img = gCtx.createImageData(w, h);
    var d = img.data;
    for (var i = 0; i < d.length; i += 4) {
      var v = (Math.random() * 255) | 0;
      d[i] = v; d[i + 1] = v; d[i + 2] = v; d[i + 3] = 70;
    }
    gCtx.putImageData(img, 0, 0);
  }, 90);

  /* Turbo, Zen, and Quake Terminal Toggles */
  var btnTurbo = document.getElementById('btn-turbo');
  if (btnTurbo) {
    btnTurbo.addEventListener('click', function () {
      var on = osStage.classList.toggle('turbo');
      btnTurbo.classList.toggle('on', on);
      if (wbTurboPill) wbTurboPill.style.display = on ? 'inline-block' : 'none';
    });
  }

  var btnZen = document.getElementById('btn-zen');
  if (btnZen) {
    btnZen.addEventListener('click', function () {
      var on = osStage.classList.toggle('zen');
      btnZen.classList.toggle('on', on);
    });
  }

  var osQuake = document.getElementById('os-quake');
  var btnQuake = document.getElementById('btn-quake');
  var closeQuake = document.getElementById('close-quake');
  if (btnQuake && osQuake) {
    btnQuake.addEventListener('click', function () {
      var on = osQuake.classList.toggle('open');
      btnQuake.classList.toggle('on', on);
    });
  }
  if (closeQuake && osQuake) {
    closeQuake.addEventListener('click', function () {
      osQuake.classList.remove('open');
      if (btnQuake) btnQuake.classList.remove('on');
    });
  }

  /* Neovim Group Tabs */
  var codeBuffers = {
    hardware: '-- modules/hardware.lua — <0.2ms zero-fork sysfs hardware probe\nhl.config({\n    xwayland = { enabled = true, force_zero_scaling = true },\n    cursor   = { no_hardware_cursors = M.is_vm and 1 or 2 },\n    opengl   = { nvidia_anti_flicker = true },\n})\n\nif M.has_nvidia then\n    hl.env("LIBVA_DRIVER_NAME", "nvidia")\n    hl.env("__GLX_VENDOR_LIBRARY_NAME", "nvidia")\n    if M.is_hybrid then\n        hl.env("AQ_FORCE_LINEAR_BLIT", "0") -- Optimus external HDMI/DP fix\n    end\nelseif M.is_vm then\n    hl.env("WLR_RENDERER_ALLOW_SOFTWARE", "1")\n    hl.env("AQ_NO_MODIFIERS", "1")\nend',
    animations: '-- modules/animations.lua — 3 damped physical springs + 29 leaves\nhl.curve("silkSpring",   { type = "spring", mass = 1.0, stiffness = 175, damping = 22 })\nhl.curve("drawerSpring", { type = "spring", mass = 0.9, stiffness = 210, damping = 24 })\nhl.curve("snappySpring", { type = "spring", mass = 0.8, stiffness = 320, damping = 28 })\n\nhl.animation({ leaf = "windowsIn",  enabled = true, speed = 4.6, spring = "silkSpring", style = "popin 84%" })\nhl.animation({ leaf = "workspaces", enabled = true, speed = 5.2, bezier = "workspaceGlide", style = "slidefade 18%" })\nhl.animation({ leaf = "specialWorkspace", enabled = true, speed = 4.8, spring = "drawerSpring", style = "slidefadevert -14%" })',
    openapp: '#!/usr/bin/env bash\n# scripts/open-app.sh — Universal Linux & Windows App Launcher\nrun_appimage() {\n    local target="$1"; shift || true\n    chmod +x "$target" 2>/dev/null || true\n    if [ -e /dev/fuse ] && ldconfig -p 2>/dev/null | grep -q "libfuse\\.so\\.2"; then\n        launch_detached "$target" "$@"\n    else\n        APPIMAGE_EXTRACT_AND_RUN=1 launch_detached "$target" --appimage-extract-and-run "$@"\n    fi\n}'
  };
  var nvimCodeView = document.getElementById('nvim-code-view');
  document.querySelectorAll('#nvim-tabs .wtab').forEach(function (tab) {
    tab.addEventListener('click', function () {
      document.querySelectorAll('#nvim-tabs .wtab').forEach(function (t) { t.classList.remove('active'); });
      tab.classList.add('active');
      var key = tab.getAttribute('data-code');
      if (nvimCodeView && codeBuffers[key]) nvimCodeView.textContent = codeBuffers[key];
    });
  });

  /* Safe Math Evaluator (for Kitty, OpenCode Browser & ⌘K Palette) */
  function safeMath(raw) {
    var s = raw.replace(/^=/, '').trim();
    if (!s || !/^[0-9+\-*/().%\s]+$/.test(s) || !/[0-9]/.test(s)) return null;
    try {
      var v = Function('"use strict";return (' + s + ')')();
      return (typeof v === 'number' && isFinite(v)) ? v : null;
    } catch (_e) {
      return null;
    }
  }

  /* Interactive Kitty Terminal inside Workspace 1 */
  var kittyForm = document.getElementById('kitty-form');
  var kittyInput = document.getElementById('kitty-input');
  var kittyOut = document.getElementById('kitty-out');

  function appendKitty(html) {
    if (!kittyOut) return;
    var row = document.createElement('div');
    row.innerHTML = html;
    kittyOut.appendChild(row);
    kittyOut.scrollTop = kittyOut.scrollHeight;
  }

  if (kittyForm && kittyInput) {
    kittyForm.addEventListener('submit', function (ev) {
      ev.preventDefault();
      var raw = kittyInput.value.trim();
      if (!raw) return;
      kittyInput.value = '';
      var esc = raw.replace(/[&<>]/g, function (c) { return ({ '&': '&amp;', '<': '&lt;', '>': '&gt;' })[c]; });
      appendKitty('<b style="color:#fff">❯</b> ' + esc);

      var low = raw.toLowerCase();
      if (low === 'clear') {
        kittyOut.innerHTML = '';
        return;
      }
      if (low === 'help') {
        appendKitty('Commands: <b style="color:#fff">doctor</b> · <b style="color:#fff">fastfetch</b> · <b style="color:#fff">hw</b> · <b style="color:#fff">calc &lt;expr&gt;</b> · <b style="color:#fff">shader &lt;noir|grain|crt|eink|oled|off&gt;</b> · <b style="color:#fff">ws &lt;1-4&gt;</b> · <b style="color:#fff">zen</b> · <b style="color:#fff">turbo</b> · <b style="color:#fff">clear</b>');
      } else if (low === 'doctor' || low === 'doctor.sh') {
        appendKitty('✓ 10/10 suites · 192/192 smoke checks · 0 Python · OpenCode browser ready');
      } else if (low === 'fastfetch') {
        appendKitty('◆ <b>OS</b>: Arch / Universal Linux  ·  <b>WM</b>: Hyprland 0.56 (Lua)<br>◆ <b>Shell</b>: zsh + starship (&lt;15ms)  ·  <b>Term</b>: kitty --single-instance<br>◆ <b>Theme</b>: Monochrome + OpenCode (#131010 / #fdfcfc)');
      } else if (low === 'hw' || low === 'hardware.sh') {
        appendKitty('Chassis: hybrid-optimus · GPUs: Intel + NVIDIA · XWayland: zero-scale · Profile: balanced');
      } else if (low === 'zen') {
        if (btnZen) btnZen.click();
        appendKitty('Toggled distraction-free Zen framing.');
      } else if (low === 'turbo') {
        if (btnTurbo) btnTurbo.click();
        appendKitty('Toggled 0ms Low-Latency Turbo / Game mode.');
      } else if (low.indexOf('shader ') === 0) {
        var sh = low.slice(7).trim();
        setShader(sh);
        appendKitty('Applied GLSL screen shader: <b style="color:#fff">' + sh + '</b>');
      } else if (low.indexOf('ws ') === 0) {
        var n = Math.max(1, Math.min(4, parseInt(low.slice(3), 10) || 1));
        setWorkspace(n - 1);
        appendKitty('Switched to workspace ' + n);
      } else if (low.indexOf('calc ') === 0 || /^[=0-9]/.test(low)) {
        var expr = low.indexOf('calc ') === 0 ? raw.slice(5) : raw;
        var ans = safeMath(expr);
        appendKitty(ans !== null ? ('= <b style="color:#fff">' + ans.toLocaleString() + '</b>') : 'Invalid math expression');
      } else {
        appendKitty('<span style="color:#9a9a9a">open-app.sh → ready to launch "' + esc + '" (type <b>help</b> for simulator commands)</span>');
      }
    });
  }

  /* Interactive OpenCode Browser inside Workspace 2 */
  var simOcInput = document.getElementById('sim-oc-input');
  var simOcForm = document.getElementById('sim-oc-form');
  var simOcOut = document.getElementById('sim-oc-out');
  var simOcMode = document.getElementById('sim-oc-mode');

  if (simOcInput) {
    simOcInput.addEventListener('focus', function () { if (simOcMode) simOcMode.textContent = 'INSERT'; });
    simOcInput.addEventListener('blur', function () { if (simOcMode) simOcMode.textContent = 'NORMAL'; });
    simOcInput.addEventListener('input', function () {
      var v = safeMath(simOcInput.value);
      if (simOcOut) simOcOut.textContent = v !== null ? ('= ' + v.toLocaleString()) : 'RUN ↵';
    });
  }
  if (simOcForm) {
    simOcForm.addEventListener('submit', function (ev) {
      ev.preventDefault();
      var q = simOcInput ? simOcInput.value.trim() : '';
      if (!q) return;
      var v = safeMath(q);
      if (v !== null) {
        copyText(String(v), 'Copied ' + v + ' from OpenCode bar');
        return;
      }
      window.open('startpage.html', '_blank', 'noopener');
    });
  }

  /* Live Waybar Sparkline, Clock, Pomodoro & 8-Core btop++ Telemetry */
  var glyphs = ['▁', '▂', '▃', '▄', '▅', '▆', '▇', '█'];
  var sparkHist = [2, 3, 1, 4, 2, 3];
  var pomoSec = 24 * 60 + 59;
  var pomoRunning = true;
  if (wbPomo) {
    wbPomo.addEventListener('click', function () {
      pomoRunning = !pomoRunning;
      showToast(pomoRunning ? 'Pomodoro resumed' : 'Pomodoro paused');
    });
  }

  var btopCoresEl = document.getElementById('btop-cores');
  function renderBtop() {
    if (!btopCoresEl) return;
    var html = '';
    for (var c = 0; c < 8; c++) {
      var pct = 6 + ((Math.sin(Date.now() * 0.002 + c * 1.1) + 1) * 19) | 0;
      var filled = Math.round((pct / 100) * 16);
      var bar = '█'.repeat(filled) + '░'.repeat(16 - filled);
      html += '<div style="display:flex;justify-content:space-between;font-size:11.5px">' +
              '<span style="color:#8a8a8a">C' + c + '</span>' +
              '<span style="color:#eaeaea">' + bar + '</span>' +
              '<span style="color:#fff;width:36px;text-align:right">' + pct + '%</span></div>';
    }
    btopCoresEl.innerHTML = html;
  }
  renderBtop();

  function pad2(n) { return n < 10 ? '0' + n : '' + n; }
  setInterval(function () {
    var d = new Date();
    if (wbClock) wbClock.textContent = pad2(d.getHours()) + ':' + pad2(d.getMinutes());
    if (simOcClock) simOcClock.textContent = pad2(d.getHours()) + ':' + pad2(d.getMinutes()) + ':' + pad2(d.getSeconds());

    var load = 8 + ((Math.random() * 14) | 0);
    sparkHist.push(Math.min(7, Math.floor(load / 4)));
    if (sparkHist.length > 6) sparkHist.shift();
    if (wbSpark) {
      wbSpark.textContent = sparkHist.map(function (i) { return glyphs[i]; }).join('') + ' ' + load + '%';
    }

    if (pomoRunning && pomoSec > 0) pomoSec--;
    if (wbPomo) {
      wbPomo.textContent = (pomoRunning ? '◎ ' : '⏸ ') + pad2(Math.floor(pomoSec / 60)) + ':' + pad2(pomoSec % 60);
    }
    renderBtop();
  }, 1000);

  /* ── 4. OpenCode Browser Theme Target Switcher & Swatches ──────────────── */
  var ocSnippets = {
    firefox: {
      path: '~/.config/browser/opencode/firefox/userChrome.css',
      code: '/* OpenCode Firefox / Zen / LibreWolf / Floorp Chrome */\n:root {\n  --toolbar-bgcolor: #1b1818 !important;\n  --urlbar-box-bgcolor: #131010 !important;\n  --tab-selected-bgcolor: #201d1d !important;\n  --tab-border-radius: 4px !important;\n}\n.tabbrowser-tab[selected="true"] .tab-background {\n  border: 1px solid #3d3838 !important;\n  border-bottom: 2px solid #ffffff !important;\n}'
    },
    chromium: {
      path: '~/.config/browser/opencode/chromium-theme/manifest.json',
      code: '{\n  "manifest_version": 3,\n  "name": "OpenCode Monochrome Theme",\n  "theme": {\n    "colors": {\n      "frame": [19, 16, 16],\n      "toolbar": [32, 29, 29],\n      "tab_text": [253, 252, 252],\n      "omnibox_background": [19, 16, 16],\n      "omnibox_text": [253, 252, 252]\n    }\n  }\n}'
    },
    tui: {
      path: '~/.config/opencode/themes/monochrome.json',
      code: '{\n  "$schema": "https://opencode.ai/theme.json",\n  "theme": {\n    "primary": "#ffffff",\n    "text": "#fdfcfc",\n    "textMuted": "#7f7a7a",\n    "background": "#131010",\n    "backgroundPanel": "#1b1818",\n    "backgroundElement": "#201d1d",\n    "border": "#3d3838"\n  }\n}'
    }
  };

  var ocFileLabel = document.getElementById('oc-file-label');
  var ocCodeBox = document.getElementById('oc-code-box');
  document.querySelectorAll('#oc-target-tabs .oc-tab').forEach(function (tab) {
    tab.addEventListener('click', function () {
      document.querySelectorAll('#oc-target-tabs .oc-tab').forEach(function (t) { t.classList.remove('active'); });
      tab.classList.add('active');
      var item = ocSnippets[tab.getAttribute('data-octab')];
      if (item && ocFileLabel && ocCodeBox) {
        ocFileLabel.textContent = item.path;
        ocCodeBox.textContent = item.code;
      }
    });
  });

  document.querySelectorAll('.oc-swatch').forEach(function (sw) {
    sw.addEventListener('click', function () {
      var hex = sw.getAttribute('data-hex');
      if (hex) copyText(hex, 'Copied OpenCode token ' + hex);
    });
  });

  /* ── 5. "Works on Any PC" Live Hardware + Distro Configurator ──────────── */
  var selectedGpu = 'nvidia';
  var selectedDistro = 'arch';

  var gpuConfigs = {
    nvidia: {
      title: 'sysfs probe → PCI 0x10de (NVIDIA Desktop / Workstation)',
      lua: '-- Auto-applied by modules/hardware.lua (<0.2ms)\nhl.env("LIBVA_DRIVER_NAME", "nvidia")\nhl.env("__GLX_VENDOR_LIBRARY_NAME", "nvidia")\nhl.env("GBM_BACKEND", "nvidia-drm")\nhl.env("NVD_BACKEND", "direct")\nhl.config({\n    cursor   = { no_hardware_cursors = 2 }, -- auto HW cursor safety\n    opengl   = { nvidia_anti_flicker = true },\n    xwayland = { force_zero_scaling = true },\n})'
    },
    optimus: {
      title: 'sysfs probe → PCI 0x8086 + 0x10de (Hybrid Optimus Laptop)',
      lua: '-- Dual-GPU Laptop: keeps external HDMI/DisplayPort Fluid\nhl.env("LIBVA_DRIVER_NAME", "nvidia")\nhl.env("__GLX_VENDOR_LIBRARY_NAME", "nvidia")\nhl.env("AQ_FORCE_LINEAR_BLIT", "0") -- prevents dGPU multi-monitor stall\nhl.bind("switch:on:Lid Switch", hl.dsp.exec_cmd("loginctl lock-session"))\n-- + Super+I powerprofilesctl (performance · balanced · power-saver)'
    },
    amd: {
      title: 'sysfs probe → PCI 0x1002 (AMD Radeon RDNA / GCN)',
      lua: '-- Native Mesa RADV + zero-frame hardware cursor & direct scanout\nhl.config({\n    render   = { direct_scanout = 2, async_commit = true },\n    cursor   = { no_hardware_cursors = 2, enable_hyprcursor = true },\n    misc     = { vfr = true, vrr = 2 },\n    xwayland = { force_zero_scaling = true },\n})'
    },
    intel: {
      title: 'sysfs probe → PCI 0x8086 (Intel Arc / Iris Xe)',
      lua: '-- Auto-enables Intel iHD VA-API media driver + 1:1 trackpad gestures\nhl.env("LIBVA_DRIVER_NAME", "iHD")\nhl.config({\n    render   = { direct_scanout = 2, async_commit = true },\n    cursor   = { no_hardware_cursors = 2 },\n    xwayland = { force_zero_scaling = true },\n})'
    },
    vm: {
      title: 'sysfs probe → VirtIO / QEMU / VMware / VirtualBox Hypervisor',
      lua: '-- Automatic 60 FPS Virtual Machine & Cloud fallback\nhl.env("WLR_RENDERER_ALLOW_SOFTWARE", "1")\nhl.env("AQ_NO_MODIFIERS", "1")\nhl.config({\n    cursor     = { no_hardware_cursors = 1 },\n    decoration = { blur = { enabled = false }, shadow = { enabled = false } },\n    misc       = { vrr = 0 },\n})'
    }
  };

  var distroCmds = {
    arch: '# install.sh → Native Arch / CachyOS / EndeavourOS / Manjaro pipeline\nsudo pacman -S --needed --noconfirm - < packages.txt',
    apt: '# install.sh → Automatic Debian / Ubuntu / Pop!_OS fallback\nsudo apt-get install -y hyprland waybar rofi kitty thunar zathura imv mpv flatpak ...',
    dnf: '# install.sh → Automatic Fedora / Nobara fallback\nsudo dnf install -y --skip-unavailable hyprland waybar rofi-wayland kitty zathura imv ...',
    zypper: '# install.sh → Automatic openSUSE Tumbleweed fallback\nsudo zypper --non-interactive install hyprland waybar rofi-wayland kitty zathura imv ...'
  };

  var pcProbeTitle = document.getElementById('pc-probe-title');
  var pcProbeCode = document.getElementById('pc-probe-code');

  function updatePcLab() {
    var g = gpuConfigs[selectedGpu] || gpuConfigs.nvidia;
    var d = distroCmds[selectedDistro] || distroCmds.arch;
    if (pcProbeTitle) pcProbeTitle.textContent = g.title;
    if (pcProbeCode) pcProbeCode.textContent = g.lua + '\n\n' + d;
  }

  document.querySelectorAll('#gpu-chips .hw-chip').forEach(function (c) {
    c.addEventListener('click', function () {
      document.querySelectorAll('#gpu-chips .hw-chip').forEach(function (b) { b.classList.remove('active'); });
      c.classList.add('active');
      selectedGpu = c.getAttribute('data-gpu') || 'nvidia';
      updatePcLab();
    });
  });

  document.querySelectorAll('#distro-chips .hw-chip').forEach(function (c) {
    c.addEventListener('click', function () {
      document.querySelectorAll('#distro-chips .hw-chip').forEach(function (b) { b.classList.remove('active'); });
      c.classList.add('active');
      selectedDistro = c.getAttribute('data-distro') || 'arch';
      updatePcLab();
    });
  });
  updatePcLab();

  /* ── 6. 20-Wallpaper Procedural Studio + Lightbox + Simulator Sync ─────── */
  var wallpapers = [
    { file: 'mono-live-01-orbital.gif', title: 'Bauhaus Celestial Orrery', badge: 'LIVE GIF · 24fps', cat: 'live' },
    { file: 'mono-live-02-pulse.gif', title: 'Breathing Singularity', badge: 'LIVE GIF · 24fps', cat: 'live' },
    { file: 'mono-live-03-lissajous.gif', title: 'P4 Harmonic Oscilloscope', badge: 'LIVE GIF · 30fps', cat: 'live' },
    { file: 'mono-11.jpg', title: 'Gravitational Singularity', badge: '4K Master', cat: 'physics' },
    { file: 'mono-12.jpg', title: 'Liquid Mercury Ferrofluid', badge: 'Blinn-Phong', cat: 'physics' },
    { file: 'mono-06.jpg', title: 'Topological Horizon', badge: '16K Master', cat: 'bauhaus' },
    { file: 'mono-07.jpg', title: 'Total Solar Eclipse', badge: '4K Master', cat: 'physics' },
    { file: 'mono-14.jpg', title: 'Swiss Bauhaus Compass', badge: 'Vector Grid', cat: 'bauhaus' },
    { file: 'mono-10.jpg', title: 'Anisotropic Silk Ribbon', badge: 'Procedural', cat: 'physics' },
    { file: 'mono-13.jpg', title: 'Noir Rain & Bokeh', badge: 'Atmospheric', cat: 'physics' },
    { file: 'mono-01.jpg', title: 'Obsidian Gradient I', badge: 'Minimal', cat: 'bauhaus' },
    { file: 'mono-02.jpg', title: 'Architectural Monolith', badge: 'Brutalist', cat: 'bauhaus' },
    { file: 'mono-03.jpg', title: 'Concentric Interference', badge: 'Optical', cat: 'physics' },
    { file: 'mono-04.jpg', title: 'Isometric Strata', badge: 'Bauhaus', cat: 'bauhaus' },
    { file: 'mono-05.jpg', title: 'Lunar Terminator', badge: '4K Ready', cat: 'physics' },
    { file: 'mono-08.jpg', title: 'Fine Grain Carbon', badge: 'OLED Safe', cat: 'bauhaus' },
    { file: 'mono-09.jpg', title: 'Subtle Grid Matrix', badge: 'Engineering', cat: 'bauhaus' }
  ];

  var wallGrid = document.getElementById('wall-grid');
  var osWallpaper = document.getElementById('os-wallpaper');
  var lbModal = document.getElementById('lightbox-modal');
  var lbImg = document.getElementById('lightbox-img');
  var lbCaption = document.getElementById('lightbox-caption');
  var lbApply = document.getElementById('lightbox-apply');
  var currentLbFile = 'mono-11.jpg';

  function applyWallpaperToSim(file, title) {
    if (osWallpaper) {
      osWallpaper.style.opacity = '0.12';
      setTimeout(function () {
        osWallpaper.src = 'wallpapers/' + file;
        osWallpaper.style.opacity = '0.64';
      }, 140);
    }
    showToast('Applied ' + (title || file) + ' to Live OS');
  }

  function renderWallpapers(filter) {
    if (!wallGrid) return;
    wallGrid.innerHTML = '';
    wallpapers.filter(function (w) {
      return filter === 'all' || w.cat === filter;
    }).forEach(function (w) {
      var card = document.createElement('div');
      card.className = 'wall-card';
      card.innerHTML =
        '<div class="wall-thumb-wrap">' +
          '<img class="wall-thumb" loading="lazy" src="wallpapers/' + w.file + '" alt="' + w.title + '">' +
          '<span class="wall-badge">' + w.badge + '</span>' +
        '</div>' +
        '<div class="wall-meta">' +
          '<div><div class="wall-name">' + w.title + '</div><div class="wall-sub">' + w.file + '</div></div>' +
          '<button type="button" class="wall-apply">Apply ↑</button>' +
        '</div>';

      card.querySelector('.wall-thumb-wrap').addEventListener('click', function () {
        currentLbFile = w.file;
        if (lbImg) lbImg.src = 'wallpapers/' + w.file;
        if (lbCaption) lbCaption.textContent = w.title + ' (' + w.file + ')';
        if (lbModal) lbModal.classList.add('open');
      });

      card.querySelector('.wall-apply').addEventListener('click', function () {
        applyWallpaperToSim(w.file, w.title);
        var liveSec = document.getElementById('live-os');
        if (liveSec) liveSec.scrollIntoView({ behavior: 'smooth' });
      });

      wallGrid.appendChild(card);
    });
  }
  renderWallpapers('all');

  document.querySelectorAll('#wall-filters .hw-chip').forEach(function (btn) {
    btn.addEventListener('click', function () {
      document.querySelectorAll('#wall-filters .hw-chip').forEach(function (b) { b.classList.remove('active'); });
      btn.classList.add('active');
      renderWallpapers(btn.getAttribute('data-wfilter') || 'all');
    });
  });

  if (lbApply) {
    lbApply.addEventListener('click', function () {
      applyWallpaperToSim(currentLbFile);
      if (lbModal) lbModal.classList.remove('open');
      var liveSec = document.getElementById('live-os');
      if (liveSec) liveSec.scrollIntoView({ behavior: 'smooth' });
    });
  }
  var lbClose = document.getElementById('lightbox-close');
  if (lbClose) lbClose.addEventListener('click', function () { lbModal.classList.remove('open'); });
  if (lbModal) {
    lbModal.addEventListener('click', function (ev) {
      if (ev.target === lbModal) lbModal.classList.remove('open');
    });
  }

  /* ── 7. Live 172-Keybind Explorer ──────────────────────────────────────── */
  var fallbackBinds = [
    { combo: 'Super + Return', submap: '', description: 'Terminal (<3ms shared GPU glyph cache)' },
    { combo: 'Super + D', submap: '', description: 'App launcher (Wayland, X11, Flatpak, AppImage, CLI)' },
    { combo: 'Super + Shift + O', submap: '', description: 'Universal opener (any file, URL, AppImage, Flatpak, .exe)' },
    { combo: 'Super + B', submap: '', description: 'Browser (auto-detects any installed browser)' },
    { combo: 'Super + Alt + B', submap: '', description: 'Open browser on OpenCode startpage' },
    { combo: 'Super + E', submap: '', description: 'File manager (auto-fallback across PCs)' },
    { combo: 'Super + ;', submap: '', description: 'Command palette & live C++20 math calculator' },
    { combo: 'Super + I', submap: '', description: 'Settings menu (PC hardware, OpenCode browser, net, sound)' },
    { combo: 'Super + F12', submap: '', description: 'Toggle Turbo / Game mode (0ms latency, direct scanout)' },
    { combo: 'Super + ,', submap: '', description: 'Cycle GLSL screen shader (Noir / 35mm / CRT / E-Ink / OLED)' },
    { combo: 'Super + Z', submap: '', description: 'Toggle distraction-free Zen mode' },
    { combo: 'Super + W', submap: '', description: 'Wallpaper picker (120fps Bezier transition)' }
  ];

  var allBinds = fallbackBinds.slice();
  var kbTbody = document.getElementById('kb-tbody');
  var kbSearch = document.getElementById('kb-search');
  var kbCount = document.getElementById('kb-count');

  function formatModCombo(item) {
    if (item.combo) return item.combo;
    var mods = [];
    var m = item.modmask || 0;
    if (m & 64) mods.push('Super');
    if (m & 4) mods.push('Ctrl');
    if (m & 8) mods.push('Alt');
    if (m & 1) mods.push('Shift');
    mods.push(item.key || '');
    return mods.filter(Boolean).join(' + ');
  }

  function renderBinds(q) {
    if (!kbTbody) return;
    var needle = (q || '').toLowerCase().trim();
    var filtered = allBinds.filter(function (b) {
      var combo = formatModCombo(b).toLowerCase();
      var sub = (b.submap || '').toLowerCase();
      var desc = (b.description || '').toLowerCase();
      return !needle || combo.indexOf(needle) !== -1 || sub.indexOf(needle) !== -1 || desc.indexOf(needle) !== -1;
    });

    if (kbCount) kbCount.textContent = filtered.length + ' shortcuts';
    var html = '';
    filtered.slice(0, 180).forEach(function (b) {
      var combo = formatModCombo(b).replace(/[&<>]/g, '');
      var sub = (b.submap || 'GLOBAL').replace(/[&<>]/g, '');
      var desc = (b.description || '').replace(/[&<>]/g, '');
      html += '<tr>' +
        '<td><span class="keycap">' + combo + '</span></td>' +
        '<td style="color:var(--ink-muted)">' + sub + '</td>' +
        '<td>' + desc + '</td>' +
      '</tr>';
    });
    kbTbody.innerHTML = html;
  }
  renderBinds('');

  fetch('keybinds.json').then(function (r) {
    return r.ok ? r.json() : null;
  }).then(function (data) {
    if (Array.isArray(data) && data.length > 0) {
      allBinds = data;
      renderBinds(kbSearch ? kbSearch.value : '');
    }
  }).catch(function () {});

  if (kbSearch) {
    kbSearch.addEventListener('input', function () {
      renderBinds(kbSearch.value);
    });
  }

  /* ── 8. Global ⌘K / Ctrl+K Command Palette Modal ───────────────────────── */
  var cmdkModal = document.getElementById('cmdk-modal');
  var openCmdkBtn = document.getElementById('open-cmdk');
  var palInput = document.getElementById('pal-input');
  var palList = document.getElementById('pal-list');
  var palMathBar = document.getElementById('pal-math-bar');

  var paletteActions = [
    { label: '▶ Jump · Interactive Live Hyprland Desktop', run: function () { location.hash = '#live-os'; } },
    { label: '󰖟 Open · Standalone OpenCode Browser Startpage', run: function () { window.open('startpage.html', '_blank', 'noopener'); } },
    { label: '󰓅 Jump · Works on Any PC (GPU & Distro Matrix)', run: function () { location.hash = '#any-pc'; } },
    { label: '󰀻 Jump · Opens Any App & File Engine', run: function () { location.hash = '#any-app'; } },
    { label: '✦ Jump · 20-Wallpaper C++20 Procedural Studio', run: function () { location.hash = '#studio'; } },
    { label: '⌨ Jump · Search All 172 Keybinds', run: function () { location.hash = '#keybinds'; } },
    { label: '📋 Copy · One-Line Git Clone & Install Command', run: function () { copyText('git clone https://github.com/esoexe/hyprland-monochrome.git && cd hyprland-monochrome && ./install.sh', 'Copied install command!'); } },
    { label: '◐ Shader · Apply P4 Studio CRT Scanlines to Live OS', run: function () { location.hash = '#live-os'; setShader('crt'); } },
    { label: '◈ Shader · Apply 35mm Film Grain to Live OS', run: function () { location.hash = '#live-os'; setShader('grain'); } },
    { label: '✦ Wallpaper · Apply Live Celestial Orrery GIF', run: function () { location.hash = '#live-os'; applyWallpaperToSim('mono-live-01-orbital.gif', 'Bauhaus Orrery'); } },
    { label: '↗ GitHub · Open esoexe/hyprland-monochrome Repository', run: function () { window.open('https://github.com/esoexe/hyprland-monochrome', '_blank', 'noopener'); } }
  ];

  function renderPalette(filter) {
    if (!palList) return;
    var q = (filter || '').toLowerCase().trim();
    var mv = safeMath(q);
    if (palMathBar) {
      if (mv !== null) {
        palMathBar.style.display = 'block';
        palMathBar.textContent = '= ' + mv.toLocaleString() + '   (press Enter to copy)';
      } else {
        palMathBar.style.display = 'none';
      }
    }

    palList.innerHTML = '';
    paletteActions.filter(function (a) {
      return !q || mv !== null || a.label.toLowerCase().indexOf(q) !== -1;
    }).forEach(function (act, idx) {
      var li = document.createElement('li');
      li.className = 'pal-item' + (idx === 0 ? ' active' : '');
      li.innerHTML = '<span>' + act.label + '</span><span style="opacity:0.6">↵</span>';
      li.addEventListener('click', function () {
        cmdkModal.classList.remove('open');
        act.run();
      });
      palList.appendChild(li);
    });
  }

  function openPalette() {
    if (!cmdkModal) return;
    cmdkModal.classList.add('open');
    if (palInput) {
      palInput.value = '';
      renderPalette('');
      setTimeout(function () { palInput.focus(); }, 20);
    }
  }

  if (openCmdkBtn) openCmdkBtn.addEventListener('click', openPalette);
  if (cmdkModal) {
    cmdkModal.addEventListener('click', function (ev) {
      if (ev.target === cmdkModal) cmdkModal.classList.remove('open');
    });
  }
  if (palInput) {
    palInput.addEventListener('input', function () { renderPalette(palInput.value); });
    palInput.addEventListener('keydown', function (ev) {
      if (ev.key === 'Enter') {
        var mv = safeMath(palInput.value);
        if (mv !== null) {
          copyText(String(mv), 'Copied ' + mv);
          cmdkModal.classList.remove('open');
          return;
        }
        var first = palList ? palList.querySelector('.pal-item') : null;
        if (first) first.click();
      }
    });
  }

  document.addEventListener('keydown', function (ev) {
    if ((ev.metaKey || ev.ctrlKey) && ev.key.toLowerCase() === 'k') {
      ev.preventDefault();
      if (cmdkModal && cmdkModal.classList.contains('open')) cmdkModal.classList.remove('open');
      else openPalette();
    } else if (ev.key === 'Escape') {
      if (cmdkModal) cmdkModal.classList.remove('open');
      if (lbModal) lbModal.classList.remove('open');
      if (osQuake) osQuake.classList.remove('open');
    }
  });
})();
