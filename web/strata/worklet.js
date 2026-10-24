// SPDX-License-Identifier: GPL-3.0-only
// STRATA's workbench in an AudioWorklet: the playing instance of the synth's DSP (web/strata/strata_web.c). Each
// render quantum asks it for 128 frames (four of its 32-frame blocks) and gives a table being built one step, as the
// FM-1's main loop would. The page sends notes, controllers and parameter values; about 30 times a second the
// worklet sends back the scope, the peak, the voices playing, and after a patch load the values now playing.

class StrataProcessor extends AudioWorkletProcessor {
  constructor() {
    super();
    this.ex = null;
    this.pending = null;                       // a patch to load once no load is in flight
    this.loaded = false;                       // a load went in: send the values when it has swapped
    this.scope = new Float32Array(1024);
    this.scopeW = 0;
    this.last = 0;
    this.port.onmessage = (e) => this.onMessage(e.data);
  }

  async onMessage(m) {
    if (m.type === "load") {
      try {
        const { instance } = await WebAssembly.instantiate(m.wasm, {});
        const ex = instance.exports;
        if (ex._initialize) ex._initialize();
        ex.sw_init();
        this.ex = ex;
        this.port.postMessage({ type: "ready", rate: ex.sw_rate() });
      } catch (err) {
        this.port.postMessage({ type: "error", message: String((err && err.message) || err) });
      }
      return;
    }
    const ex = this.ex;
    if (!ex) return;
    switch (m.type) {
      case "note": ex.sw_note(m.note | 0, m.vel | 0); break;
      case "bend": ex.sw_bend(m.value | 0); break;
      case "cc": ex.sw_cc(m.cc | 0, m.value | 0); break;
      case "set": ex.sw_set(m.id | 0, m.value | 0); break;
      case "tempo": ex.sw_tempo(+m.bpm || 120); break;
      case "panic": ex.sw_panic(); break;
      case "patch": this.pending = m; break;
    }
  }

  loadPending() {
    const ex = this.ex, m = this.pending, mem = ex.memory.buffer;
    new Int16Array(mem, ex.sw_values(), m.values.length).set(m.values);
    const name = new Uint8Array(mem, ex.sw_patch_name(), 17);
    name.fill(0);
    for (let i = 0; i < Math.min(16, m.name.length); i++) name[i] = m.name.charCodeAt(i) & 0xff;
    if (ex.sw_load()) {
      this.pending = null;
      this.loaded = true;
    }
  }

  process(inputs, outputs) {
    const ex = this.ex;
    if (!ex) return true;
    if (this.pending) this.loadPending();
    const out = outputs[0], n = out[0].length;
    ex.sw_render(n);
    const mem = ex.memory.buffer, l = new Float32Array(mem, ex.sw_out_l(), n);
    out[0].set(l);
    if (out[1]) out[1].set(new Float32Array(mem, ex.sw_out_r(), n));
    for (let i = 0; i < n; i++) this.scope[(this.scopeW + i) & 1023] = l[i];
    this.scopeW = (this.scopeW + n) & 1023;
    if (currentTime - this.last >= 1 / 30) {
      this.last = currentTime;
      const msg = { type: "frame", peak: ex.sw_take_peak(), voices: ex.sw_voices(), instances: ex.sw_instances(),
                    scope: new Float32Array(1024) };
      msg.scope.set(this.scope.subarray(this.scopeW));
      msg.scope.set(this.scope.subarray(0, this.scopeW), 1024 - this.scopeW);
      if (this.loaded && !ex.sw_loading()) {
        this.loaded = false;
        ex.sw_current();
        msg.values = Array.from(new Int16Array(mem, ex.sw_values(), ex.sw_param_count()));
      }
      this.port.postMessage(msg, [msg.scope.buffer]);
    }
    return true;
  }
}

registerProcessor("strata", StrataProcessor);
