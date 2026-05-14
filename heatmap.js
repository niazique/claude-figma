(function () {
  const ENDPOINT = 'https://script.google.com/macros/s/AKfycbyKO1QjgHcnKkoREMWp0zPbKjeWUHiXeP5HY4UDFthzcgqJw2tZaWwMRDTPC_xU9RWA/exec';

  // — трекинг кликов —
  document.addEventListener('click', function (e) {
    if (e.target.closest('#versionBadge')) return;
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

    badge.addEventListener('click', async function () {
      const existing = document.getElementById('heatmapCanvas');
      if (existing) { existing.remove(); return; }

      badge.textContent = '…';

      let clicks;
      try {
        const res = await fetch(ENDPOINT);
        clicks = await res.json();
      } catch (e) {
        badge.textContent = '!';
        setTimeout(() => badge.textContent = badge.dataset.v, 2000);
        return;
      }

      badge.textContent = badge.dataset.v;

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
        cursor:        'crosshair',
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

      // счётчик
      ctx.globalCompositeOperation = 'source-over';
      ctx.fillStyle = 'rgba(255,255,255,0.55)';
      ctx.font = '13px -apple-system, sans-serif';
      ctx.fillText(clicks.length + ' кликов', 16, pageH - 16);

      document.body.appendChild(canvas);

      canvas.addEventListener('click', () => canvas.remove());
      document.addEventListener('keydown', function handler(e) {
        if (e.key === 'Escape') { canvas.remove(); document.removeEventListener('keydown', handler); }
      });
    });
  });
})();
