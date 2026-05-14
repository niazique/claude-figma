(function () {
  const ENDPOINT = 'https://script.google.com/macros/s/AKfycbyKO1QjgHcnKkoREMWp0zPbKjeWUHiXeP5HY4UDFthzcgqJw2tZaWwMRDTPC_xU9RWA/exec';

  const style = document.createElement('style');
  style.textContent = `
    @keyframes hm-spin { to { transform: rotate(360deg); } }
    #versionBadge .hm-spinner {
      width: 12px;
      height: 12px;
      border: 2px solid rgba(174,174,178,0.3);
      border-top-color: #aeaeb2;
      border-radius: 50%;
      animation: hm-spin 0.2s linear infinite;
    }
    #versionBadge.hm-close {
      font-size: 16px !important;
      line-height: 1;
      padding: 14px;
      margin: -14px;
    }
  `;
  document.head.appendChild(style);

  // — трекинг кликов —
  document.addEventListener('click', function (e) {
    if (e.target.closest('#versionBadge')) return;
    if (document.getElementById('heatmapCanvas')) return;
    const pageW = document.documentElement.scrollWidth;
    const pageH = document.documentElement.scrollHeight;
    const x = e.pageX;
    const y = e.pageY;
    navigator.sendBeacon(ENDPOINT, JSON.stringify({
      x, y,
      xPct: Math.round(x / pageW * 1000) / 10,
      yPct: Math.round(y / pageH * 1000) / 10,
      pageW, pageH,
      path: location.pathname
    }));
  });

  // — хитмап по клику на версию —
  document.addEventListener('DOMContentLoaded', function () {
    const badge = document.getElementById('versionBadge');
    if (!badge) return;

    badge.style.cursor = 'pointer';

    let loading = false;

    function showVersion() {
      badge.classList.remove('hm-close');
      badge.textContent = badge.dataset.v;
    }
    function showLoading() {
      badge.classList.remove('hm-close');
      badge.innerHTML = '<div class="hm-spinner"></div>';
    }
    function showClose() {
      badge.classList.add('hm-close');
      badge.textContent = '×';
    }

    badge.addEventListener('click', async function () {
      const existing = document.getElementById('heatmapCanvas');
      if (existing) {
        existing.remove();
        showVersion();
        return;
      }
      if (loading) return;

      loading = true;
      showLoading();

      let clicks;
      try {
        const res = await fetch(ENDPOINT);
        clicks = await res.json();
      } catch (e) {
        loading = false;
        badge.classList.remove('hm-close');
        badge.textContent = '!';
        setTimeout(showVersion, 2000);
        return;
      }
      loading = false;

      const pageW = document.documentElement.scrollWidth;
      const pageH = document.documentElement.scrollHeight;

      const canvas = document.createElement('canvas');
      canvas.id = 'heatmapCanvas';
      canvas.width = pageW;
      canvas.height = pageH;
      Object.assign(canvas.style, {
        position:      'absolute',
        top:           '0',
        left:          '0',
        width:         pageW + 'px',
        height:        pageH + 'px',
        zIndex:        '9998',
        pointerEvents: 'all',
        cursor:        'default',
      });

      const ctx = canvas.getContext('2d');

      ctx.fillStyle = 'rgba(0,0,0,0.72)';
      ctx.fillRect(0, 0, pageW, pageH);

      ctx.globalCompositeOperation = 'lighter';

      clicks.forEach(function (c) {
        const x = c.xPct / 100 * pageW;
        const y = c.yPct / 100 * pageH;
        const r = 48;
        const g = ctx.createRadialGradient(x, y, 0, x, y, r);
        g.addColorStop(0,   'rgba(255,80,0,0.35)');
        g.addColorStop(0.4, 'rgba(255,160,0,0.15)');
        g.addColorStop(1,   'rgba(255,80,0,0)');
        ctx.fillStyle = g;
        ctx.beginPath();
        ctx.arc(x, y, r, 0, Math.PI * 2);
        ctx.fill();
      });

      ctx.globalCompositeOperation = 'source-over';
      ctx.fillStyle = 'rgba(255,255,255,0.55)';
      ctx.font = '13px -apple-system, sans-serif';
      ctx.fillText(clicks.length + ' кликов', 16, pageH - 16);

      document.body.appendChild(canvas);
      showClose();
    });
  });
})();
