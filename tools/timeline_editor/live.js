/*
 * The link between the editor page (index.html) and the engine.
 *
 * The page says what it wants (playing or not; if not, at which moment); the engine is what
 * actually happens. This keeps the two the same, whatever goes on in between:
 *
 *  - the picture is fetched frame by frame, each with the engine's state at that frame, so the
 *    picture and the time shown with it cannot drift apart, and a dead connection is an error
 *    we see (a video stream in an <img> just froze on its last frame);
 *  - if the engine is not where the page wants it (the engine was restarted, a command was
 *    lost), the page's wish is sent again;
 *  - if the editor itself was restarted (python3 tools/timeline_editor.py ...), the page loads
 *    again from it: what is saved (it never pushes an old copy back into a new editor);
 *  - while the editor or the engine is away, the page says so instead of showing a stale picture.
 *
 * Live.start({
 *   img,              the <img> the picture goes in
 *   wanted(),         -> { playing, t }: t (the timeline's seconds) is where it should be when not playing
 *   resync(),         async: after the engine was restarted (the editor gives it the timeline itself)
 *   reload(),         the editor was restarted: the page starts again from what it has
 *   onEngine(e),      every frame: the engine's state ({ t, paused, running, ... })
 *   message(text),    a problem to show (null: none)
 * })
 * Live.seek(t), Live.play(fromT), Live.pause(): the page's commands (use these, not fetch:
 * Live has to know a command is on its way before it calls the engine "not where it should be").
 */
window.Live = (function () {
  const live = { engine: null, frames: 0, session: null, resyncs: 0, corrections: 0, problem: null };
  let options = null, lastCommand = 0, lastCorrection = 0, needResync = false, busy = false, shown;

  async function post(path, body) {
    const r = await fetch(path, { method: 'POST', headers: { 'Content-Type': 'application/json' }, body: JSON.stringify(body) });
    return r.json();
  }
  const sleep = (ms) => new Promise((resolve) => setTimeout(resolve, ms));

  function problem(text) {
    if (text === live.problem) return;
    live.problem = text;
    options.img.style.opacity = text ? 0.35 : 1;
    options.message(text);
  }

  live.seek = (t) => { lastCommand = performance.now(); return post('/api/seek', { t }); };
  live.pause = async () => {
    lastCommand = performance.now();
    await post('/api/pause', { paused: true });
    await post('/api/mute', { muted: true });
  };
  live.play = async (fromT) => {
    lastCommand = performance.now();
    if (fromT !== undefined) await post('/api/seek', { t: fromT });
    await post('/api/mute', { muted: false });
    await post('/api/pause', { paused: false });
    lastCommand = performance.now();
  };

  /* the engine put where the page wants it */
  async function correct() {
    const w = options.wanted();
    live.corrections++;
    lastCorrection = performance.now();
    if (w.playing) await live.play();
    else { await live.pause(); await live.seek(w.t); }
  }

  async function check(e) {
    live.engine = e;
    if (e.gaveUp) {
      problem('The engine keeps stopping, so the editor gave up starting it (its output is in /tmp/timeline_editor_engine.log). Fix the cause and restart the editor.');
      return;
    }
    if (e.alive === false || typeof e.t !== 'number') {
      problem('The engine stopped: starting it again…');
      needResync = true;           /* a new engine: it is given back where we were */
      return;
    }
    if (busy) return;
    const now = performance.now();
    if (needResync) {
      busy = true;
      try { await options.resync(); live.resyncs++; await correct(); needResync = false; } finally { busy = false; }
      return;
    }
    const w = options.wanted();
    const off = e.paused !== !w.playing || (!w.playing && Math.abs(e.t - w.t) > 0.12) || (w.playing && e.volume === 0);
    if (off && now - lastCommand > 700 && now - lastCorrection > 700) {
      busy = true;
      try { await correct(); } finally { busy = false; }
      return;
    }
    if (!off || w.playing) problem(null);
    options.onEngine(e);
  }

  async function pictures() {
    let after = -1;
    for (;;) {
      try {
        const r = await fetch('/frame?after=' + after, { cache: 'no-store' });
        const session = r.headers.get('X-Session');
        if (live.session !== null && session !== live.session) { options.reload(); return; }      /* the editor was restarted */
        live.session = session;
        const engine = JSON.parse(r.headers.get('X-Engine') || '{}');
        if (r.status === 200) {
          after = parseInt(r.headers.get('X-Frame'), 10);
          const url = URL.createObjectURL(await r.blob());
          const old = shown;
          shown = url;
          options.img.src = url;
          if (old) setTimeout(() => URL.revokeObjectURL(old), 1000);
          live.frames++;
        }
        await check(engine);
      } catch (error) {
        problem('The editor is not running. Start it again (python3 tools/timeline_editor.py …): this page carries on by itself.');
        await sleep(700);
      }
    }
  }

  live.start = (o) => { options = o; pictures(); };
  return live;
})();
