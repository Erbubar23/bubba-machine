// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Erbubar23
//
// La página del instalador (instalar.html): elegir archivos, armar el paquete en el navegador
// sobre el FM-1 V15 de M-VAVE (web/fm1_pack.js) e instalarlo (web/fm1_update.js).
import { Actualizador, revisarPaquete, FalloFM1, V15_SHA256, sha256 } from "./fm1_update.js";
import { armarPaquete } from "./fm1_pack.js";

const EN = new URLSearchParams(location.search).get("lang") === "en";
const T = {
  es: {
    pidiendo: "Pidiendo permiso para MIDI…", nomidi: "Este navegador no tiene Web MIDI: usa Chrome o Edge en una computadora.",
    sinperm: "Sin permiso para MIDI con SysEx: el instalador no puede hablar con el FM-1.",
    buscando: "Buscando…", nohay: "No aparece un FM-1. Revisa el cable (de datos), que esté prendido y que ningún otro programa use su MIDI.",
    hay: (t) => `Conectado: ${t}`, otaq: (t) => `Conectado en modo actualización: ${t}. Elige los archivos y pulsa «Continuar».`,
    revisando: "Revisando…", stockok: "FM-1 V15 de M-VAVE, comprobado.", nostock: "Ese no es el FM-1 V15 de M-VAVE: descarga el del enlace.",
    faltav15: "Primero elige el FM-1 V15 de M-VAVE (arriba).", sinarmar: "Sin armar.",
    armado: (i, s) => `${i} · paquete armado y revisado (sha256 ${s.slice(0, 12)}…)`,
    otro: "Otro archivo (app.bin o un .fwsc armado)…", cargando: "Descargando…", danada: "la descarga llegó dañada: ",
    pasos: { inicio: "Empezando…", revisa: "Paso 1 de 2: el FM-1 revisa el paquete…", cargador: "Esperando al cargador de actualización…", escribe: "Paso 2 de 2: escribiendo. No desconectes.", reinicio: "Reiniciando…", listo: "Listo." },
    hecho: (t) => `Listo: el FM-1 arrancó con ${t}.`, distinto: (t, p) => `Se escribió, pero el FM-1 dice ${t} en vez de ${p}.`,
    seguro: "¿Escribir en el FM-1 ahora? No desconectes el cable hasta que diga «Listo».",
  },
  en: {
    pidiendo: "Asking for MIDI permission…", nomidi: "This browser has no Web MIDI: use Chrome or Edge on a computer.",
    sinperm: "No MIDI SysEx permission: the installer can't talk to the FM-1.",
    buscando: "Searching…", nohay: "No FM-1 found. Check the (data) cable, that it is switched on and that no other app uses its MIDI.",
    hay: (t) => `Connected: ${t}`, otaq: (t) => `Connected in update mode: ${t}. Choose the files and press "Resume".`,
    revisando: "Checking…", stockok: "M-VAVE's FM-1 V15, verified.", nostock: "That is not M-VAVE's FM-1 V15: download the one from the link.",
    faltav15: "First choose M-VAVE's FM-1 V15 (above).", sinarmar: "Not built.",
    armado: (i, s) => `${i} · package built and checked (sha256 ${s.slice(0, 12)}…)`,
    otro: "Another file (app.bin or a built .fwsc)…", cargando: "Downloading…", danada: "the download is damaged: ",
    pasos: { inicio: "Starting…", revisa: "Step 1 of 2: the FM-1 checks the package…", cargador: "Waiting for the update loader…", escribe: "Step 2 of 2: writing. Don't unplug.", reinicio: "Restarting…", listo: "Done." },
    hecho: (t) => `Done: the FM-1 started with ${t}.`, distinto: (t, p) => `Written, but the FM-1 reports ${t} instead of ${p}.`,
    seguro: "Write to the FM-1 now? Don't unplug the cable until it says \"Done\".",
  },
}[EN ? "en" : "es"];

if (EN) {
  document.documentElement.lang = "en";
  const tx = {
    title: "Installer for the FM-1", sub: "Over USB-C, from Chrome or Edge (Web MIDI).",
    nav: '<a href="en/index.html">Home</a><a href="emulador.html?lang=en">Emulator</a><a href="en/docs/install.html">Guide</a><a href="https://github.com/Erbubar23/bubba-machine">Code</a><a href="instalar.html" class="lang">Español</a>',
    h1: "Install Bubba Machine on the M-VAVE FM-1", riskh: "Before you start",
    risk: "<li>This is unofficial firmware, in <b>testing</b>. It may fail and, in the worst case, leave the FM-1 unable to start until it is rescued from a computer. Use it at your own risk; there is no warranty (GPL-3.0, sections 15 and 16).</li><li>It is written by <b>M-VAVE's official update loader</b>, the same one their updater uses. The chip's boot area and the device data are not touched.</li><li>What the current firmware saved (X0X patterns, for example) may be lost.</li><li>You can always go back to the factory firmware from this same page (below).</li><li>Don't unplug the cable or close the page while it writes. If it is interrupted, use \"Resume\".</li>",
    acepto: "I understand the risks and want to continue.", h_dev: "1. Connect the FM-1",
    devwhy: "Connect it with a USB data cable and switch it on. Close any other app using its MIDI (DAW, M-UPGRADE, other tabs). The browser will ask for MIDI with SysEx.",
    buscar: "Find the FM-1", nodev: "Not searched yet.", h_pkg: "2. The files",
    stock: "M-VAVE's factory firmware (FM-1 V15)",
    stockwhy: "always needed: Bubba Machine is installed with its official loader and built on that file, right here in your browser (M-VAVE's parts are not published on this site). It is also how you go back. Download it from M-VAVE and choose it; it is checked to be exactly that file.",
    nostock: "Not chosen.", bubba: "Bubba Machine", bubbawhy: "the version you want to try.", nopkg: "Not built.",
    h_go: "3. Install", instalar: "Install Bubba Machine", volver: "Back to the factory firmware",
    continuar: "Resume an interrupted install", h_help: "If something goes wrong",
    help: "<li><b>Stuck in update mode</b> (it shows as \"ota-FM-1\" in the MIDI list): reconnect, choose the same files and press \"Resume\". It stays in that mode until finished.</li><li><b>Bubba Machine doesn't start:</b> after two failed starts it enters <b>safe mode</b> (a notice on screen, no sound, USB on): reinstall or go back to the factory firmware from here.</li><li><b>Not even safe mode starts:</b> the FM-1 drops into the chip's own update mode and is rescued from a computer (install guide, Rescue section).</li>",
  };
  for (const el of document.querySelectorAll("[data-t]")) if (tx[el.dataset.t] !== undefined) el.innerHTML = tx[el.dataset.t];
}

const $ = (id) => document.getElementById(id);
const say = (t, cls = "") => { $("estado").textContent = t; $("estado").className = cls; };
const log = (t) => { $("log").textContent += t + "\n"; $("log").scrollTop = 1e9; };
const show = (id, t, cls) => { $(id).textContent = t; $(id).className = "dev " + cls; };
let up = null, dev = null, v15 = null, stock = null, bubba = null, ultimo = null, writing = false, versiones = [];
let appBytes = null, appFwsc = null;

function refresh() {
  const a = $("acepto").checked && !writing;
  $("instalar").disabled = !(a && dev && !dev.ota && bubba);
  $("volver").disabled = !(a && dev && !dev.ota && stock);
  $("continuar").disabled = !(a && dev && dev.ota && (ultimo || bubba || stock));
  $("buscar").disabled = writing;
  $("fstock").disabled = $("fown").disabled = $("ver").disabled = writing;
}
$("acepto").addEventListener("change", refresh);

async function midi() {
  if (up) return up;
  if (!navigator.requestMIDIAccess) { say(T.nomidi, "bad"); return null; }
  say(T.pidiendo, "info");
  try {
    up = new Actualizador(await navigator.requestMIDIAccess({ sysex: true }), log);
    say("");
    return up;
  } catch (_) { say(T.sinperm, "bad"); return null; }
}

$("buscar").addEventListener("click", async () => {
  if (!(await midi())) return;
  show("dev", T.buscando, "info");
  dev = await up.conectado();
  if (!dev) show("dev", T.nohay, "bad");
  else show("dev", dev.ota ? T.otaq(dev.text) : T.hay(dev.text), "ok");
  refresh();
});

$("fstock").addEventListener("change", async (e) => {
  v15 = stock = null;
  const f = e.target.files[0];
  if (f) {
    show("stockst", T.revisando, "info");
    const raw = new Uint8Array(await f.arrayBuffer());
    if (await sha256(raw) !== V15_SHA256) show("stockst", T.nostock, "bad");
    else {
      v15 = raw;
      stock = await revisarPaquete(raw);
      show("stockst", T.stockok, "ok");
    }
  }
  await armar();
});

// el paquete de Bubba Machine: app.bin (de la lista o propio) armado sobre el V15, o un .fwsc ya armado
async function armar() {
  bubba = null;
  refresh();
  try {
    if (appFwsc) {
      bubba = await revisarPaquete(appFwsc);
    } else if (appBytes) {
      if (!v15) { show("pkg", T.faltav15, "info"); return; }
      show("pkg", T.revisando, "info");
      bubba = await revisarPaquete(armarPaquete(v15, appBytes));
    } else {
      show("pkg", T.sinarmar, "info");
      return;
    }
    show("pkg", T.armado(bubba.ident, bubba.sha), "ok");
  } catch (e) { bubba = null; show("pkg", e.message, "bad"); }
  refresh();
}

async function cargarPropio(f) {
  const raw = new Uint8Array(await f.arrayBuffer());
  appBytes = appFwsc = null;
  if (/\.fwsc$/i.test(f.name)) appFwsc = raw; else appBytes = raw;
  await armar();
}

async function elegirVersion() {
  appBytes = appFwsc = null;
  const v = $("ver").value;
  $("otro").hidden = v !== "otro";
  if (v === "otro") {
    const f = $("fown").files[0];
    if (f) await cargarPropio(f); else await armar();
    return;
  }
  const item = versiones.find((x) => x.archivo === v);
  if (!item) { await armar(); return; }
  show("pkg", T.cargando, "info");
  try {
    const raw = new Uint8Array(await (await fetch("fm1/" + item.archivo, { cache: "no-cache" })).arrayBuffer());
    if (await sha256(raw) !== item.sha256) throw new FalloFM1("paquete", T.danada + item.archivo);
    appBytes = raw;
  } catch (e) { show("pkg", e.message, "bad"); return; }
  await armar();
}

$("ver").addEventListener("change", elegirVersion);
$("fown").addEventListener("change", (e) => { if (e.target.files[0]) cargarPropio(e.target.files[0]); });

(async () => {
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
  elegirVersion();
})();

const guard = (e) => { e.preventDefault(); e.returnValue = ""; };
async function correr(pkg, fn) {
  if (!pkg || !confirm(T.seguro)) return;
  writing = true; ultimo = pkg; refresh();
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
    dev = { text: fin, ota: false };
    show("dev", T.hay(fin), "ok");
  } catch (e) {
    say(e.message, "bad");
    log("ERROR " + (e.code || "") + ": " + e.message);
  } finally {
    writing = false;
    window.removeEventListener("beforeunload", guard);
    refresh();
  }
}
$("instalar").addEventListener("click", async () => { if (await midi()) correr(bubba, (p, s) => up.instalar(p, s)); });
$("volver").addEventListener("click", async () => { if (await midi()) correr(stock, (p, s) => up.instalar(p, s)); });
$("continuar").addEventListener("click", async () => { if (await midi()) correr(ultimo || bubba || stock, (p, s) => up.continuar(p, s)); });
