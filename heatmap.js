(function () {
  const ENDPOINT = 'https://script.google.com/a/macros/emergingtravel.com/s/AKfycbxRvriYMQKxOroXhZnc1fHr9BjhpLGzaR0v1c7FOSJZhnutekdzsGn8lzYlG2yLtl6q/exec';

  document.addEventListener('click', function (e) {
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
})();
