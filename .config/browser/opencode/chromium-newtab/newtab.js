(function () {
  var clockEl = document.getElementById('clock-txt');
  var dateEl = document.getElementById('date-txt');
  var tzEl = document.getElementById('tz-pill');
  var input = document.getElementById('cmd-input');
  var form = document.getElementById('cmd-form');
  var modeEl = document.getElementById('vim-mode');

  function pad(n) { return n < 10 ? '0' + n : '' + n; }
  function tick() {
    var d = new Date();
    clockEl.textContent = pad(d.getHours()) + ':' + pad(d.getMinutes()) + ':' + pad(d.getSeconds());
    dateEl.textContent = d.getFullYear() + '-' + pad(d.getMonth() + 1) + '-' + pad(d.getDate());
  }
  try { tzEl.textContent = Intl.DateTimeFormat().resolvedOptions().timeZone || 'LOCAL'; } catch (_e) {}
  tick();
  setInterval(tick, 1000);

  input.addEventListener('focus', function () { modeEl.textContent = 'INSERT'; });
  input.addEventListener('blur', function () { modeEl.textContent = 'NORMAL'; });

  form.addEventListener('submit', function (ev) {
    ev.preventDefault();
    var q = input.value.trim();
    if (!q) return;
    if (/^:[0-9]+(\/.*)?$/.test(q)) { window.location.href = 'http://localhost' + q; return; }
    var routes = {
      '/gh ': 'https://github.com/search?q=',
      '/aw ': 'https://wiki.archlinux.org/index.php?search=',
      '/aur ': 'https://aur.archlinux.org/packages?K=',
      '/yt ': 'https://www.youtube.com/results?search_query=',
      '/oc ': 'https://opencode.ai/docs?q='
    };
    for (var prefix in routes) {
      if (q.indexOf(prefix) === 0) {
        window.location.href = routes[prefix] + encodeURIComponent(q.slice(prefix.length).trim());
        return;
      }
    }
    if (/^[a-zA-Z][a-zA-Z0-9+.-]*:\/\//.test(q)) { window.location.href = q; return; }
    if (/^[^\s]+\.[a-zA-Z]{2,}(\/.*)?$/.test(q)) { window.location.href = 'https://' + q; return; }
    window.location.href = 'https://duckduckgo.com/?q=' + encodeURIComponent(q);
  });

  document.addEventListener('keydown', function (ev) {
    if (document.activeElement !== input && ev.key === '/') {
      ev.preventDefault();
      input.focus();
    } else if (document.activeElement !== input && /^[1-4]$/.test(ev.key)) {
      var link = document.querySelector('[data-key="' + ev.key + '"]');
      if (link) window.location.href = link.href;
    }
  });
})();
