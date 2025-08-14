(function(){
  const cfg = window.TELEPAINT || {};
  const files = cfg.files || ["session/aljoscha.json","session/freundin.json"];
  const colors = cfg.colors || ["#111","#d22"];
  const iterations = cfg.iterations || 200000;
  const hash = cfg.hash || "SHA-256";

  const cvs = document.getElementById("c");
  const ctx = cvs.getContext("2d");
  const statusEl = document.getElementById("status");
  const intervalInput = document.getElementById("interval");
  const refreshBtn = document.getElementById("refresh");
  const changePassBtn = document.getElementById("change-pass");

  const modal = document.getElementById("modal");
  const pwInput = document.getElementById("pw");
  const okBtn = document.getElementById("ok");
  const cancelBtn = document.getElementById("cancel");

  let keyMaterial = null;   // CryptoKey from password
  let cachedPassword = "";  // kept only in RAM

  function setStatus(s){ statusEl.textContent = s; }

  function b64ToBytes(b64){
    // atob handles base64 without URL-safe; replace URL-safe if present
    b64 = b64.replace(/-/g, '+').replace(/_/g, '/');
    const bin = atob(b64);
    const bytes = new Uint8Array(bin.length);
    for (let i=0;i<bin.length;i++) bytes[i] = bin.charCodeAt(i);
    return bytes;
  }
  function bytesToB64(bytes){
    let bin = "";
    for (let i=0;i<bytes.length;i++) bin += String.fromCharCode(bytes[i]);
    return btoa(bin);
  }

  async function importPassword(pass){
    const enc = new TextEncoder();
    return await crypto.subtle.importKey(
      "raw", enc.encode(pass), {name:"PBKDF2"}, false, ["deriveBits","deriveKey"]
    );
  }
  async function deriveKey(saltBytes){
    if (!keyMaterial) return null;
    return await crypto.subtle.deriveKey(
      { name: "PBKDF2", salt: saltBytes, iterations, hash },
      keyMaterial,
      { name: "AES-GCM", length: 256 },
      false,
      ["decrypt"]
    );
  }

  async function decryptEnvelope(env){
    // env: {v, alg, salt, iv, ct}
    if (!env || !env.alg || !env.ct || !env.iv || !env.salt) throw new Error("invalid envelope");
    if (!keyMaterial) throw new Error("no key material");
    const salt = b64ToBytes(env.salt);
    const iv = b64ToBytes(env.iv);
    const ct = b64ToBytes(env.ct);
    const key = await deriveKey(salt);
    const plain = await crypto.subtle.decrypt({name:"AES-GCM", iv}, key, ct);
    const dec = new TextDecoder().decode(new Uint8Array(plain));
    return JSON.parse(dec);
  }

  async function fetchJSON(path){
    const u = `${path}?t=${Date.now()}`;
    const r = await fetch(u, {cache:"no-store"});
    if (!r.ok) return null;
    const data = await r.json();
    // Detect encrypted envelope
    if (data && data.alg && data.ct && data.iv && data.salt){
      if (!keyMaterial){
        // ask for password once
        await ensurePassword();
      }
      try {
        return await decryptEnvelope(data);
      } catch(e){
        console.error("Decrypt failed:", e);
        setStatus("Entschlüsselung fehlgeschlagen");
        return null;
      }
    }
    // plaintext json
    return data;
  }

  function drawStroke(s, color){
    if(!s || !Array.isArray(s.points) || s.points.length < 2) return;
    ctx.strokeStyle = s.color || color;
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
    setStatus("Lade...");
    ctx.clearRect(0,0,cvs.width,cvs.height);
    const datasets = await Promise.all(files.map(f=>fetchJSON(f).catch(()=>null)));
    datasets.forEach((d, i) => d && drawFrame(d, colors[i % colors.length]));
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

  changePassBtn.addEventListener("click", async ()=>{
    await askPassword(true);
    tick();
  });

  function showModal(){
    modal.classList.remove("hidden");
    pwInput.value = "";
    pwInput.focus();
  }
  function hideModal(){ modal.classList.add("hidden"); }

  async function askPassword(force=false){
    return new Promise((resolve)=>{
      if (!force && keyMaterial){ return resolve(true); }
      showModal();
      const onOk = async ()=>{
        cachedPassword = pwInput.value || "";
        if (cachedPassword){
          keyMaterial = await importPassword(cachedPassword);
        } else {
          keyMaterial = null;
        }
        cleanup();
        hideModal();
        resolve(true);
      };
      const onCancel = ()=>{ cleanup(); hideModal(); resolve(false); };
      function cleanup(){
        okBtn.removeEventListener("click", onOk);
        cancelBtn.removeEventListener("click", onCancel);
        pwInput.removeEventListener("keydown", onEnter);
      }
      function onEnter(e){ if (e.key === "Enter") onOk(); }
      okBtn.addEventListener("click", onOk);
      cancelBtn.addEventListener("click", onCancel);
      pwInput.addEventListener("keydown", onEnter);
    });
  }

  async function ensurePassword(){
    // Show once, but user can cancel to try plaintext files
    await askPassword(false);
  }

  // Autosize canvas for DPR
  function resize(){
    const rect = cvs.getBoundingClientRect();
    const dpr = window.devicePixelRatio || 1;
    cvs.width = Math.round(rect.width * dpr);
    cvs.height = Math.round(rect.height * dpr);
    ctx.setTransform(dpr,0,0,dpr,0,0);
  }
  window.addEventListener("resize", ()=>{ resize(); tick(); });

  resize();
  start();
})();