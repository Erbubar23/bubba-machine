// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Erbubar23
//
// La página del instalador (instalar.html). Todo lo que pueda hacer sola lo hace sola: pide MIDI,
// reconoce el FM-1 (y lo vuelve a buscar cuando se conecta o se desconecta), consigue el FM-1 V15
// de M-VAVE y arma el paquete en el navegador (web/fm1_pack.js). A la persona le queda pulsar
// «Instalar» y confirmar; la escritura la hace web/fm1_update.js.
//
// De dónde sale el V15, en orden: lo que el navegador ya guardó (IndexedDB) → fm1/local/FM-1.fwsc
// (solo existe con tools/fm1/servir.py, en esta computadora) → elegirlo o arrastrarlo a mano.
import { Actualizador, revisarPaquete, FalloFM1, V15_SHA256, sha256 } from "./fm1_update.js";
import { armarPaquete } from "./fm1_pack.js";

const EN = new URLSearchParams(location.search).get("lang") === "en";
const T = {
  es: {
    pidiendo: "Pidiendo permiso para MIDI…", nomidi: "Este navegador no tiene Web MIDI: usa Chrome o Edge en una computadora.",
    sinperm: "Sin permiso para MIDI con SysEx. Pulsa el botón para pedirlo de nuevo.", permitir: "Permitir MIDI",
    buscando: "Buscando… conecta el FM-1 y préndelo.",
    hay: (t) => `${t}: listo para instalar`, otaq: (t) => `${t}: en modo actualización (una instalación quedó a medias)`,
    otromodelo: (t) => `${t}: no es un FM-1`,
    revisando: "Armando el paquete…", v15: "Buscando el FM-1 V15 de M-VAVE…", nostock: "Ese archivo no es el FM-1 V15 de M-VAVE.",
    faltav15: "Falta el FM-1 V15 de M-VAVE (abajo).",
    armado: (n) => `${n}: listo`, otro: "Otro archivo (app.bin o un .fwsc armado)…", cargando: "Descargando…", danada: "la descarga llegó dañada: ",
    instalar: "Instalar Bubba Machine", continuar: "Continuar la instalación",
    pasos: { inicio: "Empezando…", revisa: "Paso 1 de 2: el FM-1 revisa el paquete…", cargador: "Esperando al cargador de actualización…", escribe: "Paso 2 de 2: escribiendo. No desconectes.", reinicio: "Reiniciando…", listo: "Listo." },
    hecho: (t) => `Listo: el FM-1 arrancó con ${t}.`, distinto: (t, p) => `Se escribió, pero el FM-1 dice ${t} en vez de ${p}.`,
    seguro: (n, d) => `Se va a instalar ${n} en ${d}.\n\nEs firmware en pruebas, sin garantía. Lo guardado en el firmware actual se puede perder.\nNo desconectes el cable ni cierres la página hasta que diga «Listo».\n\n¿Instalar ahora?`,
    seguroV: (d) => `Se va a instalar el firmware de fábrica de M-VAVE (V15) en ${d}.\nNo desconectes el cable hasta que diga «Listo».\n\n¿Seguir?`,
    seguroC: (n) => `Se va a terminar de escribir ${n}.\nNo desconectes el cable hasta que diga «Listo».\n\n¿Seguir?`,
    fabrica: "firmware de fábrica (V15)",
  },
  en: {
    pidiendo: "Asking for MIDI permission…", nomidi: "This browser has no Web MIDI: use Chrome or Edge on a computer.",
    sinperm: "No MIDI SysEx permission. Press the button to ask again.", permitir: "Allow MIDI",
    buscando: "Searching… connect the FM-1 and switch it on.",
    hay: (t) => `${t}: ready to install`, otaq: (t) => `${t}: in update mode (an install was left halfway)`,
    otromodelo: (t) => `${t}: not an FM-1`,
    revisando: "Building the package…", v15: "Looking for M-VAVE's FM-1 V15…", nostock: "That file is not M-VAVE's FM-1 V15.",
    faltav15: "M-VAVE's FM-1 V15 is missing (below).",
    armado: (n) => `${n}: ready`, otro: "Another file (app.bin or a built .fwsc)…", cargando: "Downloading…", danada: "the download is damaged: ",
    instalar: "Install Bubba Machine", continuar: "Resume the install",
    pasos: { inicio: "Starting…", revisa: "Step 1 of 2: the FM-1 checks the package…", cargador: "Waiting for the update loader…", escribe: "Step 2 of 2: writing. Don't unplug.", reinicio: "Restarting…", listo: "Done." },
    hecho: (t) => `Done: the FM-1 started with ${t}.`, distinto: (t, p) => `Written, but the FM-1 reports ${t} instead of ${p}.`,
    seguro: (n, d) => `${n} will be installed on ${d}.\n\nThis is firmware in testing, with no warranty. What the current firmware saved may be lost.\nDon't unplug the cable or close the page until it says "Done".\n\nInstall now?`,
    seguroV: (d) => `M-VAVE's factory firmware (V15) will be installed on ${d}.\nDon't unplug the cable until it says "Done".\n\nContinue?`,
    seguroC: (n) => `${n} will finish being written.\nDon't unplug the cable until it says "Done".\n\nContinue?`,
    fabrica: "factory firmware (V15)",
  },
}[EN ? "en" : "es"];

if (EN) {
  document.documentElement.lang = "en";
  const tx = {
    title: "Installer for the FM-1", sub: "Over USB-C, from Chrome or Edge (Web MIDI).",
    nav: '<a href="en/index.html">Home</a><a href="emulador.html?lang=en">Emulator</a><a href="en/docs/install.html">Guide</a><a href="https://github.com/Erbubar23/bubba-machine">Code</a><a href="instalar.html" class="lang">Español</a>',
    h1: "Install Bubba Machine on the M-VAVE FM-1",
    intro: "Connect the FM-1 over USB (data cable) and switch it on. The page recognizes it by itself; when the button lights up, press it.",
    w_dev: "FM-1", w_pkg: "Firmware",
    faltah: "M-VAVE's factory firmware (FM-1 V15) is missing",
    faltawhy: "Bubba Machine is built on that file here, in your browser, and installed with M-VAVE's official loader (this site can't publish it). Download it and drop it here, or choose it. It is asked only once: the browser remembers it.",
    bajar: "Download FM-1.fwsc", instalar: "Install Bubba Machine",
    risk: "Unofficial firmware, <b>in testing</b>, with no warranty (GPL-3.0, sections 15 and 16). What the current firmware saved (X0X patterns, for example) may be lost. Don't unplug the cable or close the page while it writes. You can always go back to the factory firmware (below, \"More options\").",
    mas: "More options", h_ver: "Version", h_back: "Back to the factory firmware",
    backwhy: "Installs M-VAVE's FM-1 V15 as is, with its official loader.", volver: "Back to the factory firmware",
    h_help: "If something goes wrong",
    help: "<li><b>The install was interrupted:</b> the FM-1 stays in update mode (it shows as \"ota-FM-1\"). Reconnect it: the button changes to \"Resume the install\".</li><li><b>Bubba Machine doesn't start:</b> after two failed starts it enters <b>safe mode</b> (a notice on screen, no sound, USB on): reinstall or go back to the factory firmware from here.</li><li><b>Not even safe mode starts:</b> the FM-1 drops into the chip's own update mode and is rescued from a computer (<a href=\"en/docs/install.html\">install guide</a>, \"If something goes wrong\").</li>",
  };
  for (const el of document.querySelectorAll("[data-t]")) if (tx[el.dataset.t] !== undefined) el.innerHTML = tx[el.dataset.t];
}

const $ = (id) => document.getElementById(id);
const say = (t, cls = "") => { $("estado").textContent = t; $("estado").className = cls; };
const log = (t) => { $("log").textContent += t + "\n"; $("log").scrollTop = 1e9; };
const line = (id, t, cls) => { $(id + "t").textContent = t; $(id).className = "line " + cls; };

let up = null, sinPermiso = false;
let dev = null;                   // lo que contestó el saludo (identidad), o null
let v15 = null, stock = null;     // el V15 (bytes) y su paquete revisado
let bubba = null, nombre = "";    // el paquete de Bubba Machine, revisado, y cómo se llama
let ultimo = null;                // el último paquete que se empezó a escribir (para «Continuar»)
let writing = false, versiones = [], appBytes = null, appFwsc = null;

// ------------------------------------------------------------------ botones ---
function refresh() {
  const go = $("go");
  if (!up) {
    go.textContent = T.permitir;
    go.disabled = !sinPermiso;
  } else if (dev && dev.ota) {
    go.textContent = T.continuar;
    go.disabled = writing || !(ultimo || bubba);
  } else {
    go.textContent = T.instalar;
    go.disabled = writing || !dev || dev.model !== "FM-1" || !bubba;
  }
  $("volver").disabled = writing || !up || !dev || dev.ota || dev.model !== "FM-1" || !stock;
  $("fstock").disabled = $("fown").disabled = $("ver").disabled = writing;
}

// ---------------------------------------------------------------------- MIDI ---
async function midi() {
  if (up) return up;
  if (!navigator.requestMIDIAccess) { line("dev", T.nomidi, "bad"); return null; }
  line("dev", T.pidiendo, "info");
  try {
    const access = await navigator.requestMIDIAccess({ sysex: true });
    up = new Actualizador(access, log);
    sinPermiso = false;
    access.onstatechange = () => { clearTimeout(detectar.t); detectar.t = setTimeout(detectar, 800); };
    refresh();
    detectar();
    return up;
  } catch (_) {
    sinPermiso = true;
    line("dev", T.sinperm, "bad");
    refresh();
    return null;
  }
}

// reconocer lo conectado: solo el saludo de identidad, no escribe nada. Mientras no haya un FM-1,
// vuelve a mirar cada pocos segundos (al prenderse tarda en contestar); nunca mientras escribe.
let buscando = null;
async function detectar() {
  clearTimeout(detectar.poll);
  if (!up || writing) return;
  if (buscando) return buscando;
  buscando = (async () => {
    if (!dev) line("dev", T.buscando, "info");
    try { dev = await up.conectado(); } catch (_) { dev = null; }
    if (writing) return;
    if (!dev) line("dev", T.buscando, "info");
    else if (dev.ota) line("dev", T.otaq(dev.text), "ok");
    else if (dev.model !== "FM-1") line("dev", T.otromodelo(dev.text), "bad");
    else line("dev", T.hay(dev.text), "ok");
    refresh();
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
  $("falta").hidden = false;
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
  await armar();
}
$("fstock").addEventListener("change", (e) => elegirV15(e.target.files[0]));
const zona = $("falta");
zona.addEventListener("dragover", (e) => { e.preventDefault(); zona.classList.add("over"); });
zona.addEventListener("dragleave", () => zona.classList.remove("over"));
zona.addEventListener("drop", (e) => { e.preventDefault(); zona.classList.remove("over"); elegirV15(e.dataTransfer.files[0]); });

// ------------------------------------------------------------- el paquete ---
// app.bin (de la lista o propio) armado sobre el V15, o un .fwsc ya armado
async function armar() {
  bubba = null;
  refresh();
  try {
    if (appFwsc) {
      bubba = await revisarPaquete(appFwsc);
    } else if (appBytes) {
      if (!v15) { line("pkg", T.faltav15, "bad"); return; }
      line("pkg", T.revisando, "info");
      bubba = await revisarPaquete(armarPaquete(v15, appBytes));
    } else {
      return;
    }
    line("pkg", T.armado(nombre || bubba.ident), "ok");
    log(`paquete: ${nombre} · ${bubba.ident} · sha256 ${bubba.sha}`);
  } catch (e) { bubba = null; line("pkg", e.message, "bad"); }
  refresh();
}

async function cargarPropio(f) {
  const raw = new Uint8Array(await f.arrayBuffer());
  appBytes = appFwsc = null;
  nombre = f.name;
  if (/\.fwsc$/i.test(f.name)) appFwsc = raw; else appBytes = raw;
  await armar();
}

async function elegirVersion() {
  appBytes = appFwsc = null;
  bubba = null;
  refresh();
  const v = $("ver").value;
  $("otro").hidden = v !== "otro";
  if (v === "otro") {
    const f = $("fown").files[0];
    if (f) await cargarPropio(f);
    return;
  }
  const item = versiones.find((x) => x.archivo === v);
  if (!item) return;
  nombre = EN ? item.nombre_en || item.nombre : item.nombre;
  nombre = nombre.replace(/\s*\(.*\)$/, "");
  line("pkg", T.cargando, "info");
  try {
    const raw = new Uint8Array(await (await fetch("fm1/" + item.archivo, { cache: "no-cache" })).arrayBuffer());
    if ((await sha256(raw)) !== item.sha256) throw new FalloFM1("paquete", T.danada + item.archivo);
    appBytes = raw;
  } catch (e) { line("pkg", e.message, "bad"); return; }
  await armar();
}
$("ver").addEventListener("change", elegirVersion);
$("fown").addEventListener("change", (e) => { if (e.target.files[0]) cargarPropio(e.target.files[0]); });

// ------------------------------------------------------------- escribir ---
const guard = (e) => { e.preventDefault(); e.returnValue = ""; };
async function correr(pkg, pregunta, fn) {
  if (!pkg || writing) return;
  if (buscando) await buscando;
  if (!confirm(pregunta)) return;
  writing = true; ultimo = pkg; refresh();
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
    if (fin === pkg.ident) say(T.hecho(fin), "ok"); else say(T.distinto(fin, pkg.ident), "bad");
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

$("go").addEventListener("click", async () => {
  if (!up) { await midi(); return; }
  if (dev && dev.ota) {
    const pkg = ultimo || bubba;
    correr(pkg, T.seguroC(pkg === stock ? T.fabrica : nombre), (p, s) => up.continuar(p, s));
  } else {
    correr(bubba, T.seguro(nombre, dev ? dev.text : "FM-1"), (p, s) => up.instalar(p, s));
  }
});
$("volver").addEventListener("click", () => {
  if (up) correr(stock, T.seguroV(dev ? dev.text : "FM-1"), (p, s) => up.instalar(p, s));
});

// ------------------------------------------------------------------ arranque ---
(async () => {
  refresh();
  midi();
  try { versiones = await (await fetch("fm1/versiones.json", { cache: "no-cache" })).json(); } catch (_) { versiones = []; }
  for (const v of versiones) {
    const o = document.createElement("option");
    o.value = v.archivo;
    o.textContent = EN ? v.nombre_en || v.nombre : v.nombre;
    $("ver").appendChild(o);
  }
  const o = document.createElement("option");
  o.value = "otro";
  o.textContent = T.otro;
  $("ver").appendChild(o);
  await buscarV15();
  await elegirVersion();
  if (!v15 && !appFwsc) line("pkg", T.faltav15, "bad");
})();
