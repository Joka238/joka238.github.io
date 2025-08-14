(function(){
  const cfg = window.TELEPAINT || {};
  const files = cfg.files || ["session/aljoscha.json","session/freundin.json"];
  const colors = cfg.colors || ["#111","#d22"];
  const cvs = document.getElementById("c");
  const ctx = cvs.getContext("2d");
  const statusEl = document.getElementById("status");
  const intervalInput = document.getElementById("interval");
  const refreshBtn = document.getElementById("refresh");
  const passInput = document.getElementById("password");
  let keyCache = null;

  function setStatus(s){ statusEl.textContent = s; }

  async function deriveKey(password, salt){
    const enc = new TextEncoder();
    const keyMaterial = await window.crypto.subtle.importKey(
      "raw", enc.encode(password), {name: "PBKDF2"}, false, ["deriveKey"]
    );
    return window.crypto.subtle.deriveKey(
      { name: "PBKDF2", salt: salt, iterations: 200000, hash: "SHA-256" },
      keyMaterial, { name: "AES-GCM", length: 256 }, false, ["decrypt"]
    );
  }

  async function decryptPayload(payloadB64, saltB64, ivB64, password){
    const salt = Uint8Array.from(atob(saltB64), c => c.charCodeAt(0));
    const iv = Uint8Array.from(atob(ivB64), c => c.charCodeAt(0));
    const ct = Uint8Array.from(atob(payloadB64), c => c.charCodeAt(0));
    const key = keyCache || await deriveKey(password, salt);
    keyCache = key;
    const ptBuf = await window.crypto.subtle.decrypt({name:"AES-GCM", iv: iv}, key, ct);
    const dec = new TextDecoder();
    return dec.decode(ptBuf);
  }

  async function fetchAndDecrypt(path, password){
    try {
      const u = `${path}?t=${Date.now()}`;
      const r = await fetch(u, {cache:"no-store"});
      if(!r.ok) return null;
      const obj = await r.json();
      if(!obj.ct || !obj.salt || !obj.iv){
        console.warn("No encryption fields in", path);
        return null;
      }
      const plain = await decryptPayload(obj.ct, obj.salt, obj.iv, password);
      return JSON.parse(plain);
    } catch(e){
      console.error("decrypt fail", e);
      return null;
    }
  }

  function drawStroke(s, color){
    if(!Array.isArray(s.points) || s.points.length < 2) return;
    ctx.strokeStyle = color;
    ctx.lineWidth = (s.width || 3);
    ctx.lineCap = "round";
    ctx.lineJoin = "round";
    ctx.beginPath();
    for(let i=1;i<s.points.length;i++){
      const [x1, y1] = s.points[i-1];
      const [x2, y2] = s.points[i];
      ctx.moveTo(x1 * cvs.width, y1 * cvs.height);
      ctx.lineTo(x2 * cvs.width, y2 * cvs.height);
    }
    ctx.stroke();
  }

  function drawFrame(data, color){
    if(!data || !Array.isArray(data.strokes)) return;
    for(const s of data.strokes){ drawStroke(s, color); }
  }

  async function tick(){
    const password = passInput.value;
    if(!password){ setStatus("Bitte Passwort eingeben"); return; }
    setStatus("Lade...");
    ctx.clearRect(0,0,cvs.width,cvs.height);
    const datasets = await Promise.all(files.map(f => fetchAndDecrypt(f, password)));
    datasets.forEach((d, i) => drawFrame(d, colors[i % colors.length]));
    setStatus("Aktualisiert: " + new Date().toLocaleTimeString());
  }

  let t = null;
  function start(){
    if(t) clearInterval(t);
    const sec = Math.max(5, Number(intervalInput.value)|0);
    t = setInterval(tick, sec*1000);
    tick();
  }

  refreshBtn.addEventListener("click", tick);
  intervalInput.addEventListener("change", start);

  function resize(){
    const rect = cvs.getBoundingClientRect();
    const dpr = window.devicePixelRatio || 1;
    cvs.width = Math.round(rect.width * dpr);
    cvs.height = Math.round(rect.height * dpr);
    ctx.setTransform(dpr,0,0,dpr,0,0);
    tick();
  }
  window.addEventListener("resize", resize);

  resize();
  start();
})();