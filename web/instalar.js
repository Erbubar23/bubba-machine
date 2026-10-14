// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Erbubar23
//
// La página del instalador (instalar.html), plug and play: conectas el FM-1 y la página hace el
// resto. Pide MIDI, lee el FM-1 (y lo vuelve a buscar cuando se conecta o se desconecta), elige la
// versión más nueva de Bubba Machine y la baja ya armada (web/fm1/*.fwsc, el mismo paquete de la
// release). Queda un botón: Instalar. Al terminar, si la página está en español, carga el español
// en la pantalla (web/fm1_consola.js). La escritura la hace web/fm1_update.js.
//
// El V15 de M-VAVE solo hace falta para volver al firmware de fábrica o para una versión sin
// paquete publicado (se arma en el navegador, web/fm1_pack.js). De dónde sale, en orden: lo que el
// navegador ya guardó (IndexedDB) → fm1/local/FM-1.fwsc (tools/fm1/servir.py) → a mano.
import { Actualizador, revisarPaquete, FalloFM1, V15_SHA256, sha256 } from "./fm1_update.js?v=5";
import { armarPaquete } from "./fm1_pack.js?v=5";
import { enviar, esperar } from "./fm1_consola.js?v=1";

const EN = new URLSearchParams(location.search).get("lang") === "en";
const T = {
  es: {
    pidiendo: "Chrome está pidiendo permiso para MIDI: acepta el aviso junto a la barra de direcciones.",
    nomidi: "Este navegador no tiene Web MIDI: usa Chrome o Edge en una computadora (no en el teléfono).",
    bloqueado: "Chrome bloqueó el MIDI para esta página. Pulsa el ícono junto a la dirección (candado o ajustes), pon «Dispositivos MIDI» en Permitir y recarga.",
    sinperm: "Sin permiso para MIDI. Pulsa «Permitir MIDI» y acepta.",
    buscando: "Buscando el FM-1… conéctalo y préndelo.",
    hay: (t, n) => n ? `Conectado: FM-1 con ${n} (${t})` : `Conectado: ${t}`, otaq: (t) => `Conectado en modo actualización (${t}): una instalación quedó a medias.`,
    otromodelo: (t) => `Contestó ${t}: no es un FM-1.`,
    vistos: "Puertos MIDI que ve la página:", ninguno: "Ningún puerto MIDI. Revisa que el cable sea de datos (no solo de carga).",
    est: { ok: "es el FM-1", otro: "otro aparato (no se toca)", sinsalida: "sin salida MIDI", ocupado: "OCUPADO por otro programa: ciérralo (DAW, M-UPGRADE, otra pestaña) y reconecta el cable", mudo: "no contestó", filtrado: "contestó" },
    v15: "Buscando el FM-1 V15 de M-VAVE…", nostock: "Ese archivo no es el FM-1 V15 de M-VAVE.", faltav15: "Falta el FM-1 V15 de M-VAVE (abajo).",
    listo: (n) => `${n}: listo para instalar`, cargando: "Descargando…", revisando: "Armando el paquete…", danada: "la descarga llegó dañada: ",
    fabrica: "Firmware de fábrica (M-VAVE V15)", fabricad: "Recupera el firmware original del aparato.",
    instalar: (n) => `Instalar ${n}`, recuperar: "Recuperar el firmware de fábrica", continuar: "Continuar la instalación",
    pasos: { inicio: "Empezando…", revisa: "Paso 1 de 2: el FM-1 revisa el paquete…", cargador: "Esperando al cargador de actualización…", escribe: "Paso 2 de 2: escribiendo. No desconectes.", reinicio: "Reiniciando…", listo: "Listo." },
    hecho: (t) => `Listo: el FM-1 arrancó con ${t}.`, distinto: (t, p) => `Se escribió, pero el FM-1 dice ${t} en vez de ${p}.`,
    seguro: (n, d) => `Se va a instalar ${n} en ${d}.\n\nEs firmware beta, sin garantía. Lo guardado en el firmware actual se puede perder.\nNo desconectes el cable ni cierres la página hasta que diga «Listo».\n\n¿Instalar ahora?`,
    seguroF: (d) => `Se va a instalar el firmware de fábrica de M-VAVE (V15) en ${d}.\nNo desconectes el cable hasta que diga «Listo».\n\n¿Seguir?`,
    seguroC: (n) => `Se va a terminar de escribir ${n}.\nNo desconectes el cable hasta que diga «Listo».\n\n¿Seguir?`,
    idioma: "Cargando el español en la pantalla…", idiomaok: (t) => `Listo: el FM-1 arrancó con ${t}, con la pantalla en español.`,
    idiomamal: (e) => `Bubba Machine quedó instalada, pero el español no se cargó (${e}). Se puede cargar después en el emulador o con tools/fm1/idioma.py.`,
  },
  en: {
    pidiendo: "Chrome is asking for MIDI permission: accept the prompt next to the address bar.",
    nomidi: "This browser has no Web MIDI: use Chrome or Edge on a computer (not a phone).",
    bloqueado: "Chrome blocked MIDI for this page. Click the icon next to the address (lock or settings), set \"MIDI devices\" to Allow and reload.",
    sinperm: "No MIDI permission. Press \"Allow MIDI\" and accept.",
    buscando: "Looking for the FM-1… connect it and switch it on.",
    hay: (t, n) => n ? `Connected: FM-1 running ${n} (${t})` : `Connected: ${t}`, otaq: (t) => `Connected in update mode (${t}): an install was left halfway.`,
    otromodelo: (t) => `${t} answered: not an FM-1.`,
    vistos: "MIDI ports the page sees:", ninguno: "No MIDI ports. Check that the cable carries data (not charge only).",
    est: { ok: "it is the FM-1", otro: "another device (left alone)", sinsalida: "no MIDI output", ocupado: "BUSY in another app: close it (DAW, M-UPGRADE, another tab) and reconnect the cable", mudo: "did not answer", filtrado: "answered" },
    v15: "Looking for M-VAVE's FM-1 V15…", nostock: "That file is not M-VAVE's FM-1 V15.", faltav15: "M-VAVE's FM-1 V15 is missing (below).",
    listo: (n) => `${n}: ready to install`, cargando: "Downloading…", revisando: "Building the package…", danada: "the download is damaged: ",
    fabrica: "Factory firmware (M-VAVE V15)", fabricad: "Brings back the device's original firmware.",
    instalar: (n) => `Install ${n}`, recuperar: "Restore the factory firmware", continuar: "Resume the install",
    pasos: { inicio: "Starting…", revisa: "Step 1 of 2: the FM-1 checks the package…", cargador: "Waiting for the update loader…", escribe: "Step 2 of 2: writing. Don't unplug.", reinicio: "Restarting…", listo: "Done." },
    hecho: (t) => `Done: the FM-1 started with ${t}.`, distinto: (t, p) => `Written, but the FM-1 reports ${t} instead of ${p}.`,
    seguro: (n, d) => `${n} will be installed on ${d}.\n\nThis is beta firmware, with no warranty. What the current firmware saved may be lost.\nDon't unplug the cable or close the page until it says "Done".\n\nInstall now?`,
    seguroF: (d) => `M-VAVE's factory firmware (V15) will be installed on ${d}.\nDon't unplug the cable until it says "Done".\n\nContinue?`,
    seguroC: (n) => `${n} will finish being written.\nDon't unplug the cable until it says "Done".\n\nContinue?`,
    idioma: "Loading Spanish on the screen…", idiomaok: (t) => `Done: the FM-1 started with ${t}, with the screen in Spanish.`,
    idiomamal: (e) => `Bubba Machine is installed, but Spanish did not load (${e}). You can load it later.`,
  },
}[EN ? "en" : "es"];

if (EN) {
  document.documentElement.lang = "en";
  const tx = {
    title: "Installer for the FM-1", sub: "Over USB-C, from Chrome or Edge (Web MIDI).",
    nav: '<a href="en/index.html">Home</a><a href="emulador.html?lang=en">Emulator</a><a href="en/docs/install.html">Guide</a><a href="https://github.com/Erbubar23/bubba-machine">Code</a><a href="instalar.html" class="lang">Español</a>',
    h1: "Install on the M-VAVE FM-1",
    intro: "Connect the FM-1 over USB (data cable) and switch it on. The page finds it by itself and gets the newest Bubba Machine ready: just press <b>Install</b>. Nothing to download.",
    es: "When done, the screen in Spanish (loads by itself)",
    despues: 'Next: <a href="sonidos.html?lang=en">Vital sounds and DX7 banks</a> for BITTAL, B-MINI, BUNO-6 and BX-7, or the <a href="en/docs/manual.html">manual</a>.',
    h_dev: "1. Your FM-1", permitir: "Allow MIDI", h_que: "2. What do you want to install?",
    faltah: "This needs M-VAVE's factory firmware (FM-1 V15)",
    faltawhy: "Going back to the factory firmware (or installing an old version) uses M-VAVE's original file, which this site does not publish. Download it and drop it here, or choose it. It is asked only once: the browser remembers it. Installing Bubba Machine does not need it.",
    bajar: "Download FM-1.fwsc", h_go: "3. Install",
    risk: "Unofficial firmware, <b>beta</b>, with no warranty (GPL-3.0, sections 15 and 16). What the current firmware saved (X0X patterns, for example) may be lost, and until v1 a Bubba Machine update may erase what you saved in it. Don't unplug the cable or close the page while it writes. You can always go back to the factory firmware by choosing it above.",
    mas: "More options", h_otro: "Install another file",
    otrowhy: "An app.bin you built (it is built on top of V15) or an already built .fwsc. Once chosen it appears as an option above.",
    h_help: "If something goes wrong",
    help: "<li><b>The FM-1 doesn't show up:</b> under \"Your FM-1\" you can see which MIDI ports the page found. If it says \"busy\", close the app using it (REAPER or another DAW, M-UPGRADE, another tab with an installer) and unplug and replug the cable.</li><li><b>The install was interrupted:</b> the FM-1 stays in update mode (it shows as \"ota-FM-1\"). Reconnect it: the button changes to \"Resume the install\".</li><li><b>Bubba Machine doesn't start:</b> after two failed starts it enters <b>safe mode</b> (a notice on screen, no sound, USB on): reinstall or go back to the factory firmware from here.</li><li><b>Not even safe mode starts:</b> the FM-1 drops into the chip's own update mode and is rescued from a computer (<a href=\"en/docs/install.html\">install guide</a>, \"If something goes wrong\").</li>",
  };
  for (const el of document.querySelectorAll("[data-t]")) if (tx[el.dataset.t] !== undefined) el.innerHTML = tx[el.dataset.t];
  const es = document.getElementById("es");
  if (es) es.checked = false;                              /* en inglés, la pantalla queda en inglés */
}

// qué firmware es, por la versión que contesta al saludo
const FIRMWARES = { 15: EN ? "the factory firmware (V15)" : "el firmware de fábrica (V15)", 3: "Bubba Machine", 900: "X0X", 910: "Felucca" };

const $ = (id) => document.getElementById(id);
const say = (t, cls = "") => { $("estado").textContent = t; $("estado").className = cls; };
const log = (t) => { $("log").textContent += t + "\n"; $("log").scrollTop = 1e9; };
const line = (id, t, cls) => { $(id + "t").textContent = t; $(id).className = "line " + cls; };

let up = null, acc = null, permiso = "prompt", pedido = false;
let dev = null;                   // lo que contestó el saludo (identidad), o null
let v15 = null, stock = null;     // el V15 (bytes) y su paquete revisado
let opciones = [], sel = null;    // {clave, nombre, detalle, tipo: "app"|"fabrica"|"propio", archivo?, sha256?}
const paquetes = new Map();       // clave -> paquete revisado (o un error)
const bins = new Map();           // archivo -> bytes de app.bin
let ultimo = null, ultimoNombre = "";   // lo último que se empezó a escribir (para «Continuar»)
let writing = false;

// ------------------------------------------------------------------ pantalla ---
function refresh() {
  const go = $("go");
  const pkg = paqueteElegido();
  $("permitir").hidden = !!up || permiso === "denied" || !navigator.requestMIDIAccess;
  go.classList.toggle("fabrica", !!sel && sel.tipo === "fabrica" && !(dev && dev.ota));
  if (dev && dev.ota) {
    go.textContent = T.continuar;
    go.disabled = writing || !(ultimo || pkg);
  } else {
    go.textContent = !sel ? "…" : sel.tipo === "fabrica" ? T.recuperar : T.instalar(sel.nombre);
    go.disabled = writing || !up || !dev || dev.model !== "FM-1" || !pkg;
  }
  for (const el of document.querySelectorAll("#opciones input")) el.disabled = writing;
  $("fstock").disabled = $("fown").disabled = writing;
}

function pintarOpciones() {
  const box = $("opciones");
  box.textContent = "";
  for (const o of opciones) {
    const lab = document.createElement("label");
    lab.className = "opt" + (sel && sel.clave === o.clave ? " sel" : "");
    const r = document.createElement("input");
    r.type = "radio"; r.name = "que"; r.checked = !!sel && sel.clave === o.clave;
    r.addEventListener("change", () => elegir(o));
    const d = document.createElement("div");
    const b = document.createElement("b"); b.textContent = o.nombre;
    const s = document.createElement("span"); s.textContent = o.detalle || "";
    d.append(b, s);
    lab.append(r, d);
    box.appendChild(lab);
  }
}

// ---------------------------------------------------------------------- MIDI ---
async function estadoPermiso() {
  try {
    const p = await navigator.permissions.query({ name: "midi", sysex: true });
    p.onchange = () => { permiso = p.state; if (p.state === "granted") midi(); else mostrarDev(); };
    return p.state;
  } catch (_) { return "prompt"; }
}

function mostrarDev() {
  if (!navigator.requestMIDIAccess) { line("dev", T.nomidi, "bad"); refresh(); return; }
  if (!up) {
    if (permiso === "denied") line("dev", T.bloqueado, "bad");
    else if (pedido) line("dev", T.pidiendo, "info");
    else line("dev", T.sinperm, "bad");
    $("devinfo").textContent = "";
    refresh();
    return;
  }
  if (!dev) line("dev", T.buscando, "info");
  else if (dev.ota) line("dev", T.otaq(dev.text), "ok");
  else if (dev.model !== "FM-1") line("dev", T.otromodelo(dev.text), "bad");
  else line("dev", T.hay(dev.text, FIRMWARES[dev.version]), "ok");
  const v = up.vistos || [];
  $("devinfo").textContent = dev && !dev.ota && !v.some((x) => x.estado === "ocupado") ? "" :
    !v.length ? T.ninguno : T.vistos + "\n" + v.map((x) => `· ${x.name}: ${T.est[x.estado] || x.estado}${x.id ? " (" + x.id.text + ")" : ""}`).join("\n");
  refresh();
}

async function midi() {
  if (up) return up;
  if (!navigator.requestMIDIAccess) { mostrarDev(); return null; }
  pedido = true;
  mostrarDev();
  try {
    const access = await navigator.requestMIDIAccess({ sysex: true });
    acc = access;
    up = new Actualizador(access, log);
    permiso = "granted";
    access.onstatechange = () => { clearTimeout(detectar.t); detectar.t = setTimeout(detectar, 800); };
    detectar();
    return up;
  } catch (_) {
    pedido = false;
    permiso = await estadoPermiso();
    mostrarDev();
    return null;
  }
}
$("permitir").addEventListener("click", () => midi());

// leer lo conectado: solo el saludo de identidad, no escribe nada. Mientras no haya un FM-1,
// vuelve a mirar cada pocos segundos; nunca mientras escribe.
let buscando = null;
async function detectar() {
  clearTimeout(detectar.poll);
  if (!up || writing) return;
  if (buscando) return buscando;
  buscando = (async () => {
    try { dev = await up.conectado(); } catch (_) { dev = null; }
    if (writing) return;
    mostrarDev();
    if (!dev) detectar.poll = setTimeout(detectar, 3000);
  })();
  try { await buscando; } finally { buscando = null; }
}

// ------------------------------------------------------------------ el V15 ---
const DB = "bubba-instalador";
function guardado(op, valor) {
  return new Promise((ok) => {
    try {
      const r = indexedDB.open(DB, 1);
      r.onupgradeneeded = () => r.result.createObjectStore("archivos");
      r.onerror = () => ok(null);
      r.onsuccess = () => {
        try {
          const tx = r.result.transaction("archivos", op === "get" ? "readonly" : "readwrite");
          const st = tx.objectStore("archivos");
          const q = op === "get" ? st.get("v15") : st.put(valor, "v15");
          q.onsuccess = () => ok(q.result || null);
          q.onerror = () => ok(null);
        } catch (_) { ok(null); }
      };
    } catch (_) { ok(null); }
  });
}

async function usarV15(raw, recordar) {
  if (!raw || (await sha256(raw)) !== V15_SHA256) return false;
  v15 = raw;
  stock = await revisarPaquete(raw);
  paquetes.clear();
  $("falta").hidden = true;
  if (recordar) await guardado("put", raw);
  return true;
}

async function buscarV15() {
  line("pkg", T.v15, "info");
  const g = await guardado("get");
  if (g && (await usarV15(new Uint8Array(g), false))) return;
  try {
    const r = await fetch("fm1/local/FM-1.fwsc", { cache: "no-cache" });
    if (r.ok && (await usarV15(new Uint8Array(await r.arrayBuffer()), true))) return;
  } catch (_) { /* sin servidor local: se elige a mano */ }
  /* el aviso de que falta solo aparece si se elige algo que lo necesita (preparar) */
}

async function elegirV15(f) {
  if (!f) return;
  line("pkg", T.revisando, "info");
  if (!(await usarV15(new Uint8Array(await f.arrayBuffer()), true))) {
    line("pkg", T.nostock, "bad");
    $("falta").hidden = false;
    refresh();
    return;
  }
  await preparar();
}
$("fstock").addEventListener("change", (e) => elegirV15(e.target.files[0]));
const zona = $("falta");
zona.addEventListener("dragover", (e) => { e.preventDefault(); zona.classList.add("over"); });
zona.addEventListener("dragleave", () => zona.classList.remove("over"));
zona.addEventListener("drop", (e) => { e.preventDefault(); zona.classList.remove("over"); elegirV15(e.dataTransfer.files[0]); });

// ------------------------------------------------------------- los paquetes ---
function paqueteElegido() {
  if (!sel) return null;
  if (sel.tipo === "fabrica") return stock;
  const p = paquetes.get(sel.clave);
  return p && !(p instanceof Error) ? p : null;
}

// arma (o toma de la caché) el paquete de la opción elegida y lo muestra
async function preparar() {
  const o = sel;
  refresh();
  if (!o) return;
  if (o.tipo === "fabrica") {
    if (stock) line("pkg", T.listo(o.nombre), "ok"); else { line("pkg", T.faltav15, "bad"); $("falta").hidden = false; }
    refresh();
    return;
  }
  $("falta").hidden = true;
  let p = paquetes.get(o.clave);
  if (!p) {
    try {
      if (o.tipo === "propio" && o.fwsc) {
        p = await revisarPaquete(o.fwsc);
      } else if (o.tipo === "app" && o.fwsc) {                /* el paquete ya armado: sin V15 */
        line("pkg", T.cargando, "info");
        const raw = new Uint8Array(await (await fetch("fm1/" + o.fwsc, { cache: "no-cache" })).arrayBuffer());
        if ((await sha256(raw)) !== o.fwsc_sha256) throw new FalloFM1("paquete", T.danada + o.fwsc);
        p = await revisarPaquete(raw);
      } else {
        let app = o.tipo === "propio" ? o.bin : bins.get(o.archivo);
        if (!app) {
          line("pkg", T.cargando, "info");
          app = new Uint8Array(await (await fetch("fm1/" + o.archivo, { cache: "no-cache" })).arrayBuffer());
          if ((await sha256(app)) !== o.sha256) throw new FalloFM1("paquete", T.danada + o.archivo);
          bins.set(o.archivo, app);
        }
        if (!v15) { if (sel === o) { line("pkg", T.faltav15, "bad"); $("falta").hidden = false; refresh(); } return; }
        if (sel === o) line("pkg", T.revisando, "info");
        p = await revisarPaquete(armarPaquete(v15, app));
      }
      log(`paquete: ${o.nombre} · ${p.ident} · sha256 ${p.sha}`);
    } catch (e) { p = e instanceof Error ? e : new Error(String(e)); }
    paquetes.set(o.clave, p);
  }
  if (sel !== o) return;
  if (p instanceof Error) line("pkg", p.message, "bad"); else line("pkg", T.listo(o.nombre), "ok");
  refresh();
}

function elegir(o) {
  sel = o;
  pintarOpciones();
  preparar();
}

$("fown").addEventListener("change", async (e) => {
  const f = e.target.files[0];
  if (!f) return;
  const raw = new Uint8Array(await f.arrayBuffer());
  const o = { clave: "propio:" + f.name + ":" + raw.length, nombre: f.name, detalle: EN ? "Your file" : "Tu archivo", tipo: "propio" };
  if (/\.fwsc$/i.test(f.name)) o.fwsc = raw; else o.bin = raw;
  opciones = opciones.filter((x) => x.tipo !== "propio");
  opciones.splice(opciones.length - 1, 0, o);
  elegir(o);
});

// ------------------------------------------------------------- escribir ---
const guard = (e) => { e.preventDefault(); e.returnValue = ""; };
async function correr(pkg, nombre, pregunta, fn) {
  if (!pkg || writing) return;
  if (buscando) await buscando;
  if (!confirm(pregunta)) return;
  writing = true; ultimo = pkg; ultimoNombre = nombre; refresh();
  clearTimeout(detectar.poll);
  window.addEventListener("beforeunload", guard);
  $("bar").value = 0;
  const total = Math.ceil(pkg.image.length / 512);
  const paso = (k, n) => {
    if (T.pasos[k]) say(T.pasos[k], k === "listo" ? "ok" : "info");
    if (k === "revisa") $("bar").value = Math.min(10, n / 5);
    if (k === "escribe") $("bar").value = 10 + Math.min(88, (n / total) * 88);
  };
  try {
    const fin = await fn(pkg, paso);
    $("bar").value = 100;
    if (fin !== pkg.ident) say(T.distinto(fin, pkg.ident), "bad");
    else if (/^Bubba Machine/.test(nombre) && $("es") && $("es").checked && acc) {
      say(T.idioma, "info");                               /* plug and play: el español, solo */
      try {
        if (!(await esperar(acc))) throw new Error("no arrancó a tiempo");
        const pack = new Uint8Array(await (await fetch("lang/es.bmlang", { cache: "no-cache" })).arrayBuffer());
        await enviar(acc, 0x21, pack, (x) => { $("bar").value = 100 * x; });
        $("bar").value = 100;
        say(T.idiomaok(fin), "ok");
      } catch (e) { say(T.idiomamal(e.message || e), "bad"); }
    } else say(T.hecho(fin), "ok");
    if (/^Bubba Machine/.test(nombre)) $("despues").hidden = false;
  } catch (e) {
    say(e.message, "bad");
    log("ERROR " + (e.code || "") + ": " + e.message);
    $("mas").open = true;
  } finally {
    writing = false;
    window.removeEventListener("beforeunload", guard);
    dev = null;
    refresh();
    detectar();
  }
}

$("go").addEventListener("click", () => {
  if (!up) { midi(); return; }
  if (dev && dev.ota) {
    const pkg = ultimo || paqueteElegido();
    const n = ultimo ? ultimoNombre : sel.nombre;
    correr(pkg, n, T.seguroC(n), (p, s) => up.continuar(p, s));
  } else if (sel) {
    const pkg = paqueteElegido();
    const pregunta = sel.tipo === "fabrica" ? T.seguroF(dev ? dev.text : "FM-1") : T.seguro(sel.nombre, dev ? dev.text : "FM-1");
    correr(pkg, sel.nombre, pregunta, (p, s) => up.instalar(p, s));
  }
});

// ------------------------------------------------------------------ arranque ---
(async () => {
  refresh();
  permiso = navigator.requestMIDIAccess ? await estadoPermiso() : "prompt";
  if (permiso !== "denied") midi(); else mostrarDev();
  let versiones = [];
  try { versiones = await (await fetch("fm1/versiones.json", { cache: "no-cache" })).json(); } catch (_) { versiones = []; }
  opciones = versiones.map((v) => ({
    clave: v.archivo, archivo: v.archivo, sha256: v.sha256, tipo: "app", fwsc: v.fwsc, fwsc_sha256: v.fwsc_sha256,
    nombre: EN ? v.nombre_en || v.nombre : v.nombre, detalle: EN ? v.detalle_en || v.detalle : v.detalle,
  }));
  opciones.push({ clave: "fabrica", tipo: "fabrica", nombre: T.fabrica, detalle: T.fabricad });
  sel = opciones[0];
  pintarOpciones();
  await buscarV15();
  await preparar();
})();
