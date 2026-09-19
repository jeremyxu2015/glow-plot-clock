/* Plot Clock 装配指南 · 交互脚本
 * 1) 效果演示：用与固件相同的字形算法在画布上实时"书写"当前时间
 * 2) 侧边导航滚动高亮
 */
(function () {
  "use strict";

  /* ---------------- 演示动画 ---------------- */
  var canvas = document.getElementById("demo");
  if (canvas) {
    var ctx = canvas.getContext("2d");
    var W = canvas.width, H = canvas.height;
    var rad = function (f) { return Math.PI * 2 * f; };

    /* 字符字形定义（与固件 drawDigit 一致，归一化坐标，y 向上）*/
    var defs = {};
    defs[0] = [
      { op: "move", x: 0.5, y: 1 },
      { op: "arc", on: true, cx: 0.5, cy: 0.5, rx: 0.5, ry: 0.5, f0: 0.25, f1: -0.75 }
    ];
    defs[1] = [
      { op: "move", x: 0.25, y: 0.875 },
      { op: "line", on: true, x: 0.5, y: 1 },
      { op: "line", on: true, x: 0.5, y: 0 }
    ];
    defs[2] = [
      { op: "move", x: 0, y: 0.75 },
      { op: "arc", on: true, cx: 0.5, cy: 0.75, rx: 0.5, ry: 0.25, f0: 0.5, f1: -0.125 },
      { op: "arc", on: true, cx: 1, cy: 0, rx: 1, ry: 0.5, f0: 0.375, f1: 0.5 },
      { op: "line", on: true, x: 1, y: 0 }
    ];
    defs[3] = [
      { op: "move", x: 0, y: 0.75 },
      { op: "arc", on: true, cx: 0.5, cy: 0.75, rx: 0.5, ry: 0.25, f0: 0.375, f1: -0.25 },
      { op: "arc", on: true, cx: 0.5, cy: 0.25, rx: 0.5, ry: 0.25, f0: 0.25, f1: -0.375 }
    ];
    defs[4] = [
      { op: "move", x: 1, y: 0.375 },
      { op: "line", on: true, x: 0, y: 0.375 },
      { op: "line", on: true, x: 0.75, y: 1 },
      { op: "line", on: true, x: 0.75, y: 0 }
    ];
    defs[5] = [
      { op: "move", x: 1, y: 1 },
      { op: "line", on: true, x: 0, y: 1 },
      { op: "line", on: true, x: 0, y: 0.5 },
      { op: "line", on: true, x: 0.5, y: 0.5 },
      { op: "arc", on: true, cx: 0.5, cy: 0.25, rx: 0.5, ry: 0.25, f0: 0.25, f1: -0.25 },
      { op: "line", on: true, x: 0, y: 0 }
    ];
    defs[6] = [
      { op: "move", x: 0, y: 0.25 },
      { op: "arc", on: true, cx: 0.5, cy: 0.25, rx: 0.5, ry: 0.25, f0: 0.5, f1: -0.5 },
      { op: "arc", on: true, cx: 1, cy: 0.5, rx: 1, ry: 0.5, f0: 0.5, f1: 0.25 }
    ];
    defs[7] = [
      { op: "move", x: 0, y: 1 },
      { op: "line", on: true, x: 1, y: 1 },
      { op: "line", on: true, x: 0.25, y: 0 }
    ];
    defs[8] = [
      { op: "move", x: 0.5, y: 0.5 },
      { op: "arc", on: true, cx: 0.5, cy: 0.75, rx: 0.5, ry: 0.25, f0: -0.25, f1: 0.75 },
      { op: "arc", on: true, cx: 0.5, cy: 0.25, rx: 0.5, ry: 0.25, f0: 0.25, f1: -0.75 }
    ];
    defs[9] = [
      { op: "move", x: 1, y: 0.75 },
      { op: "arc", on: true, cx: 0.5, cy: 0.75, rx: 0.5, ry: 0.25, f0: 0, f1: 1 },
      { op: "line", on: true, x: 0.75, y: 0 }
    ];
    /* 冒号：两个小圆点 */
    defs[11] = [
      { op: "move", x: 0.5, y: 0.75 },
      { op: "dot", on: true },
      { op: "move", x: 0.5, y: 0.25 },
      { op: "dot", on: true }
    ];

    /* 把 drawDigit 的宏展开成折线数组 */
    function sampleOps(ops, step) {
      var pts = [];
      ops.forEach(function (op) {
        if (op.op === "move" || op.op === "line") {
          pts.push({ x: op.x, y: op.y, on: op.on || false });
        } else if (op.op === "arc") {
          var pos = op.f0, arr = [];
          if (pos < op.f1) {
            for (; pos <= op.f1; pos += step) {
              arr.push({ x: op.cx + Math.cos(rad(pos)) * op.rx, y: op.cy + Math.sin(rad(pos)) * op.ry, on: true });
            }
          } else {
            for (; pos >= op.f1; pos -= step) {
              arr.push({ x: op.cx + Math.cos(rad(pos)) * op.rx, y: op.cy + Math.sin(rad(pos)) * op.ry, on: true });
            }
          }
          pts.push.apply(pts, arr);
        } else if (op.op === "dot") {
          var last = pts[pts.length - 1];
          pts.push({ x: last.x, y: last.y, on: true, dwell: true });
        }
      });
      return pts;
    }

    var STEP = 0.06, CELL_W = 24, CELL_H = 44, GAP = 14, HOME_Y = 172;
    var msPerPt = 34;      // 写字速度（画布演示，对应固件 DRAW_DELAY）
    var DWELL_MS = 340;    // 冒号圆点停留

    /* 生成当前时间的完整书写时间线 */
    function buildTimeline() {
      var d = new Date();
      var h = d.getHours(), m = d.getMinutes();
      var chars = [];
      if (Math.floor(h / 10)) chars.push(Math.floor(h / 10)); // 小时的十位是 0 时省略（与固件一致）
      chars.push(h % 10);
      chars.push(11);             // 冒号
      chars.push(Math.floor(m / 10));
      chars.push(m % 10);

      var total = chars.length;
      var steps = [];
      chars.forEach(function (dgt, i) {
        var ox = (W - (total * CELL_W + (total - 1) * GAP)) / 2 + i * (CELL_W + GAP);
        var pts = sampleOps(defs[dgt], STEP).map(function (p) {
          return { x: ox + p.x * CELL_W, y: HOME_Y - p.y * CELL_H, on: p.on, dwell: !!p.dwell };
        });
        var j, headOn = false;
        for (j = 0; j < pts.length; j++) {
          steps.push({
            x: pts[j].x, y: pts[j].y,
            on: pts[j].on,          // 灯是否已点亮（决定是否留下余辉）
            dur: pts[j].dwell ? DWELL_MS : (headOn ? msPerPt : msPerPt * 0.6),
            glow: pts[j].on,        // 该步作为移动中的光点
            dot: pts[j].dwell
          });
          headOn = pts[j].on;
          if (pts[j].dwell) headOn = false; // 冒号两点之间灯熄
        }
      });

      // 写完后回位（低亮、快速）
      var totalDur = steps.reduce(function (s, p) { return s + p.dur; }, 0);
      return { steps: steps, total: totalDur };
    }

    var trail = [];
    var run = null, running = false, finishedAt = 0;

    function addTrail(step, alphaBoost) {
      trail.push({ x: step.x, y: step.y, age: 0, r: step.dot ? 3.4 : 2.1, on: step.on });
    }

    function drawScene(head) {
      ctx.fillStyle = "#05080f";
      ctx.fillRect(0, 0, W, H);

      var g = ctx.createLinearGradient(0, 0, 0, H);
      g.addColorStop(0, "rgba(124,255,138,0.05)");
      g.addColorStop(1, "rgba(74,222,128,0.10)");
      ctx.fillStyle = g;
      ctx.fillRect(18, 70, W - 36, 156);

      for (var i = 0; i < trail.length; i++) {
        var t = trail[i];
        t.age++;
        var a = Math.max(0, 1 - t.age / 85);
        if (a <= 0) { trail.splice(i, 1); i--; continue; }
        ctx.beginPath();
        ctx.arc(t.x, t.y, t.r, 0, Math.PI * 2);
        ctx.fillStyle = "rgba(140,255,160," + (a * 0.85).toFixed(3) + ")";
        ctx.fill();
      }

      if (head) {
        ctx.save();
        ctx.shadowColor = "#7cff8a";
        ctx.shadowBlur = 24;
        ctx.beginPath();
        ctx.arc(head.x, head.y, head.dot ? 4 : 3.2, 0, Math.PI * 2);
        ctx.fillStyle = head.glow ? "rgba(235,255,238,0.98)" : "rgba(210,235,216,0.4)";
        ctx.fill();
        ctx.restore();
      }
    }

    function step() {
      if (!running) return;
      var tl = run.tl;
      var t = performance.now() - run.t0;

      var acc = 0, head = null, idx = 0;
      for (idx = 0; idx < tl.steps.length; idx++) {
        if (acc + tl.steps[idx].dur > t) { head = tl.steps[idx]; break; }
        acc += tl.steps[idx].dur;
        if (tl.steps[idx].on) addTrail(tl.steps[idx]);
      }

      if (idx >= tl.steps.length) {
        // 写完：余辉渲染 + 熄灯后停留，随后重写（继续请求帧才能触发重启）
        drawScene(null);
        finishedAt = finishedAt || t;
        if (t - finishedAt > 5200) {
          startDemo();
          return;
        }
        requestAnimationFrame(step);
        return;
      }
      drawScene(head);
      requestAnimationFrame(step);
    }

    function startDemo() {
      if (run && run.raf) cancelAnimationFrame(run.raf);
      run = { tl: buildTimeline(), t0: performance.now() };
      finishedAt = 0;
      running = true;
      trail = [];
      requestAnimationFrame(step);
    }

    var io = new IntersectionObserver(function (es) {
      es.forEach(function (e) {
        if (e.isIntersecting) {
          if (!running) startDemo();
        }
      });
    }, { threshold: 0.25 });
    io.observe(canvas);
  }

  /* ---------------- 滚动高亮 ---------------- */
  var sections = [].slice.call(document.querySelectorAll(".step"));
  var navLinks = [].slice.call(document.querySelectorAll(".side-nav a, .topnav a"));
  function setActive(id) {
    navLinks.forEach(function (a) {
      a.classList.toggle("active", a.getAttribute("href") === "#" + id);
    });
  }
  if (sections.length && "IntersectionObserver" in window) {
    var spy = new IntersectionObserver(function (es) {
      es.forEach(function (e) {
        if (e.isIntersecting) setActive(e.target.id);
      });
    }, { rootMargin: "-35% 0px -55% 0px" });
    sections.forEach(function (s) { spy.observe(s); });
  }
})();