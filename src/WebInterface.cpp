#include "WebInterface.h"
#include "config.h"
#include "CategoryManager.h"
#include "StorageStats.h"
#include "ShoppingList.h"
#include <LittleFS.h>
#include <Preferences.h>
#include <ArduinoJson.h>
#include <ElegantOTA.h>
#include <time.h>

static const char INDEX_HTML[] = R"RAW(<!DOCTYPE html>
<html lang="de">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0">
<meta name="theme-color" content="#3b82f6">
<meta name="apple-mobile-web-app-capable" content="yes">
<meta name="apple-mobile-web-app-title" content="Lager">
<link rel="manifest" href="/manifest.json">
<link rel="apple-touch-icon" href="/icon.svg">
<title>Lebensmittel-Scanner</title>
<style>
  :root {
    --bg:#111827;--surface:#1f2937;--border:#374151;
    --text:#f9fafb;--muted:#9ca3af;
    --ok:#10b981;--warn:#f59e0b;--danger:#ef4444;--accent:#3b82f6;
  }
  *{box-sizing:border-box;margin:0;padding:0;}
  body{background:var(--bg);color:var(--text);font-family:system-ui,sans-serif;}
  nav{background:var(--surface);border-bottom:1px solid var(--border);display:flex;}
  nav button{background:none;border:none;color:var(--muted);padding:.9rem 1.4rem;
             cursor:pointer;font-size:.9rem;border-bottom:2px solid transparent;}
  nav button.active{color:var(--text);border-bottom-color:var(--accent);}
  .panel{display:none;} .panel.active{display:block;}
  .stats{display:flex;gap:1rem;padding:1.25rem 1.5rem;flex-wrap:wrap;}
  .stat-card{background:var(--surface);border:1px solid var(--border);
             border-radius:.75rem;padding:1rem 1.5rem;min-width:130px;}
  .stat-card .label{font-size:.75rem;color:var(--muted);margin-bottom:.25rem;}
  .stat-card .value{font-size:2rem;font-weight:700;}
  .toolbar{padding:.75rem 1.5rem;display:flex;gap:.75rem;flex-wrap:wrap;align-items:center;}
  input[type=search],input[type=text],select{
    background:var(--surface);border:1px solid var(--border);color:var(--text);
    padding:.5rem .75rem;border-radius:.5rem;font-size:.9rem;outline:none;}
  input[type=search]{flex:1;min-width:180px;}
  input[type=search]:focus,input[type=text]:focus,select:focus{border-color:var(--accent);}
  select{cursor:pointer;}
  .btn{background:var(--accent);color:white;border:none;padding:.5rem 1rem;
       border-radius:.5rem;cursor:pointer;font-size:.9rem;}
  .btn:hover{opacity:.85;} .btn-danger{background:var(--danger);}
  .btn-ok{background:var(--ok);} .btn-sm{padding:.25rem .6rem;font-size:.8rem;}
  .btn-ghost{background:none;border:1px solid var(--border);color:var(--muted);
             padding:.4rem .8rem;border-radius:.5rem;cursor:pointer;font-size:.85rem;}
  .btn-ghost:hover{border-color:var(--accent);color:var(--accent);}
  .table-wrap{overflow-x:auto;padding:0 1.5rem 2rem;}
  table{width:100%;border-collapse:collapse;}
  thead{background:var(--surface);position:sticky;top:0;z-index:1;}
  th{padding:.75rem 1rem;text-align:left;font-size:.8rem;color:var(--muted);
     text-transform:uppercase;letter-spacing:.05em;border-bottom:1px solid var(--border);}
  td{padding:.75rem 1rem;border-bottom:1px solid var(--border);font-size:.9rem;vertical-align:middle;}
  tr:hover td{background:rgba(255,255,255,.02);}
  .pill{display:inline-block;padding:.15rem .5rem;border-radius:999px;font-size:.75rem;font-weight:600;}
  .pill-ok{background:#064e3b;color:var(--ok);}
  .pill-warn{background:#78350f;color:var(--warn);}
  .pill-danger{background:#7f1d1d;color:var(--danger);}
  .cat-badge{display:inline-block;padding:.15rem .55rem;border-radius:.4rem;
             font-size:.75rem;font-weight:600;color:#fff;}
  .add-form{margin:0 1.5rem 2rem;background:var(--surface);
            border:1px solid var(--border);border-radius:.75rem;padding:1.25rem;}
  .add-form h3{font-size:.95rem;margin-bottom:1rem;}
  .form-row{display:flex;gap:.75rem;flex-wrap:wrap;align-items:flex-end;}
  .form-row label{display:flex;flex-direction:column;gap:.3rem;
                  font-size:.8rem;color:var(--muted);flex:1;min-width:140px;}
  .form-row label input,.form-row label select{font-size:.9rem;}
  .req{color:var(--danger);}
  .empty{text-align:center;color:var(--muted);padding:3rem;}
  #invLoading,#cpLoading{text-align:center;color:var(--muted);padding:2rem;}
  .mono{font-family:monospace;font-size:.8rem;color:var(--muted);}
  .cat-group-header td{background:var(--surface);font-size:.8rem;
                       color:var(--muted);padding:.4rem 1rem;font-weight:600;}
  @media(max-width:640px){
    .stats{gap:.5rem;} .stat-card{min-width:100px;}
    .table-wrap{padding:0 .75rem 2rem;} .toolbar{padding:.5rem .75rem;}
    th,td{padding:.5rem .4rem;}
  }
</style>
</head>
<body>
<nav>
  <button class="active" onclick="switchTab('inv',this)">&#x1F4E6; Inventar</button>
  <button onclick="switchTab('cp',this)">&#x1F4CB; Vorlagen</button>
  <button onclick="switchTab('cats',this)">&#x1F3F7; Kategorien</button>
  <button onclick="switchTab('wifi',this)">&#x1F4F6; WiFi</button>
  <button onclick="switchTab('mqtt',this)">&#x1F4E1; MQTT</button>
  <button onclick="switchTab('shop',this)">&#x1F6D2; Einkauf</button>
  <button onclick="switchTab('stats',this)">&#x1F4CA; Statistik</button>
  <button onclick="switchTab('system',this)">&#x1F4BE; System</button>
  <button onclick="location.href='/update'" style="margin-left:auto">&#x2B06; OTA</button>
</nav>
<div id="inv" class="panel active">
  <div class="stats">
    <div class="stat-card"><div class="label">Gesamt</div>
      <div class="value" id="statTotal">&ndash;</div></div>
    <div class="stat-card"><div class="label">Bald ablaufend (&le;7 Tage)</div>
      <div class="value" style="color:var(--warn)" id="statExpiring">&ndash;</div></div>
    <div class="stat-card"><div class="label">Abgelaufen</div>
      <div class="value" style="color:var(--danger)" id="statExpired">&ndash;</div></div>
  </div>
  <div class="toolbar">
    <input type="search" id="searchInput" placeholder="Produkt suchen&hellip;" oninput="renderInv()">
    <select id="filterSelect" onchange="renderInv()">
      <option value="all">Alle Status</option>
      <option value="expiring">Bald ablaufend</option>
      <option value="expired">Abgelaufen</option>
      <option value="ok">OK</option>
    </select>
    <select id="catFilterSelect" onchange="renderInv()">
      <option value="all">Alle Kategorien</option>
    </select>
    <button class="btn-ghost" onclick="loadInv()">&#x21BB;</button>
    <button class="btn btn-sm" onclick="exportCSV()">CSV</button>
  </div>
  <div class="table-wrap">
    <div id="invLoading">Lade&hellip;</div>
    <table id="invTable" style="display:none">
      <thead><tr>
        <th onclick="sortBy('name')" style="cursor:pointer">Produkt &#x2195;</th>
        <th>Marke</th>
        <th onclick="sortBy('expiry')" style="cursor:pointer">MHD &#x2195;</th>
        <th onclick="sortBy('daysLeft')" style="cursor:pointer">Verbleibend &#x2195;</th>
        <th>Menge</th><th>Barcode</th><th></th>
      </tr></thead>
      <tbody id="invBody"></tbody>
    </table>
    <div id="invEmpty" class="empty" style="display:none">Keine Eintr&auml;ge.</div>
  </div>
</div>
<div id="cp" class="panel">
  <div class="toolbar" style="padding-top:1.25rem">
    <span style="font-size:.9rem;color:var(--muted)">
      Vorlagen erscheinen am Ger&auml;t unter
      <strong style="color:var(--text)">&bdquo;Aus Liste w&auml;hlen&ldquo;</strong>
      &mdash; erst Kategorie, dann Produkt.
    </span>
    <button class="btn-ghost" onclick="loadCP()">&#x21BB;</button>
  </div>
  <div class="table-wrap">
    <div id="cpLoading">Lade&hellip;</div>
    <table id="cpTable" style="display:none">
      <thead><tr>
        <th>Kategorie</th><th>Produktname</th>
        <th>Marke</th><th>MHD (Tage)</th><th>Barcode</th><th></th>
      </tr></thead>
      <tbody id="cpBody"></tbody>
    </table>
    <div id="cpEmpty" class="empty" style="display:none">Noch keine Vorlagen.</div>
  </div>
  <div class="add-form">
    <h3>Neue Vorlage hinzuf&uuml;gen</h3>
    <div class="form-row">
      <label>Kategorie <span class="req">*</span><select id="cpCategory"></select></label>
      <label>Produktname <span class="req">*</span>
        <input type="text" id="cpName" placeholder="z.B. Rindfleisch Hack" maxlength="60">
      </label>
      <label>Marke
        <input type="text" id="cpBrand" placeholder="z.B. Metzger Schmidt" maxlength="40">
      </label>
      <label>Standard-MHD (Tage) <span style="color:var(--muted)">(0 = manuell)</span>
        <input type="number" id="cpDefaultDays" placeholder="z.B. 180" min="0" max="3650" value="0" style="width:120px">
      </label>
      <label>Barcode <span style="color:var(--muted)">(optional)</span>
        <input type="text" id="cpBarcode" placeholder="EAN13 / leer lassen" maxlength="30">
      </label>
      <button class="btn btn-ok" onclick="addCP()">Hinzuf&uuml;gen</button>
    </div>
    <div id="cpMsg" style="margin-top:.75rem;font-size:.85rem"></div>
  </div>
</div>
<div id="cats" class="panel">
  <div class="toolbar" style="padding-top:1.25rem">
    <span style="font-size:.9rem;color:var(--muted)">
      Max. 12 Kategorien &mdash; Reihenfolge bestimmt die Tab-Anzeige am Ger&auml;t.
    </span>
    <button class="btn-ghost" onclick="loadCats()">&#x21BB;</button>
  </div>
  <div class="table-wrap">
    <div id="catsLoading">Lade&hellip;</div>
    <table id="catsTable" style="display:none">
      <thead><tr><th>Farbe</th><th>Name</th><th></th></tr></thead>
      <tbody id="catsBody"></tbody>
    </table>
    <div id="catsEmpty" class="empty" style="display:none">Keine Kategorien.</div>
  </div>
  <div class="add-form">
    <h3>Neue Kategorie</h3>
    <div class="form-row">
      <label>Name <span class="req">*</span>
        <input type="text" id="catName" placeholder="z.B. Milchprodukte" maxlength="20">
      </label>
      <label>Farbe <span class="req">*</span>
        <div id="colorPicker" style="display:flex;gap:.4rem;flex-wrap:wrap;margin-top:.3rem"></div>
        <input type="hidden" id="catBg" value="0">
        <input type="hidden" id="catFg" value="65535">
      </label>
      <button class="btn btn-ok" onclick="addCat()">Hinzuf&uuml;gen</button>
    </div>
    <div id="catMsg" style="margin-top:.75rem;font-size:.85rem"></div>
  </div>
</div>
<div id="wifi" class="panel">
  <div style="padding:1.5rem;max-width:480px">
    <h2 style="margin-bottom:1rem">WLAN konfigurieren</h2>
    <p style="color:var(--muted);margin-bottom:1.5rem;font-size:.9rem">
      Zugangsdaten werden auf dem Ger&auml;t gespeichert. Nach dem Speichern startet das Ger&auml;t neu.
    </p>
    <div id="wifiCurrent" style="background:var(--surface);border:1px solid var(--border);
         border-radius:.75rem;padding:1rem;margin-bottom:1.5rem;font-size:.9rem">
      Aktuell: <span id="wifiCurrentSsid" style="color:var(--accent)">&hellip;</span>
    </div>
    <div style="display:flex;flex-direction:column;gap:.75rem">
      <label style="font-size:.85rem;color:var(--muted)">WLAN-Name (SSID)
        <input type="text" id="wifiSsid" placeholder="Mein WLAN" maxlength="64"
               style="display:block;width:100%;margin-top:.3rem">
      </label>
      <label style="font-size:.85rem;color:var(--muted)">Passwort
        <input type="text" id="wifiPw" placeholder="Passwort" maxlength="64"
               style="display:block;width:100%;margin-top:.3rem">
      </label>
      <button class="btn btn-ok" onclick="saveWifi()">Speichern &amp; Neustart</button>
    </div>
    <div id="wifiMsg" style="margin-top:.75rem;font-size:.85rem"></div>
  </div>
</div>
<div id="mqtt" class="panel">
  <div style="padding:1.5rem;max-width:480px">
    <h2 style="margin-bottom:1rem">MQTT konfigurieren</h2>
    <p style="color:var(--muted);margin-bottom:1.5rem;font-size:.9rem">
      Ereignisse werden an den Broker gesendet:<br>
      <code style="color:var(--accent)">{prefix}/eingelagert</code> bei jeder Einlagerung<br>
      <code style="color:var(--accent)">{prefix}/warnung</code> st&uuml;ndlich wenn Artikel ablaufen
    </p>
    <div style="display:flex;flex-direction:column;gap:.75rem">
      <label style="font-size:.85rem;color:var(--muted)">Broker-Host (IP oder Hostname)
        <input type="text" id="mqttHost" placeholder="192.168.1.100" maxlength="64"
               style="display:block;width:100%;margin-top:.3rem">
      </label>
      <label style="font-size:.85rem;color:var(--muted)">Port
        <input type="number" id="mqttPort" value="1883" min="1" max="65535"
               style="display:block;width:120px;margin-top:.3rem">
      </label>
      <label style="font-size:.85rem;color:var(--muted)">Topic-Prefix
        <input type="text" id="mqttPrefix" value="lebensmittel" maxlength="40"
               style="display:block;width:100%;margin-top:.3rem">
      </label>
      <div style="display:flex;gap:.75rem;flex-wrap:wrap">
        <button class="btn btn-ok" onclick="saveMqtt()">Speichern</button>
        <button class="btn" style="background:var(--surface);color:var(--muted);border:1px solid var(--border)"
                onclick="clearMqtt()">MQTT deaktivieren</button>
      </div>
    </div>
    <div id="mqttMsg" style="margin-top:.75rem;font-size:.85rem"></div>
    <hr style="border-color:var(--border);margin:1.5rem 0">
    <h2 style="margin-bottom:1rem">Telegram-Bot</h2>
    <p style="color:var(--muted);margin-bottom:1rem;font-size:.9rem">
      Benachrichtigung bei Einlagerung, Auslagerung und st&uuml;ndlichen Warnungen.
    </p>
    <div style="display:flex;flex-direction:column;gap:.75rem">
      <label style="font-size:.85rem;color:var(--muted)">Bot-Token
        <input type="text" id="tgToken" placeholder="1234567890:ABC..." maxlength="80"
               style="display:block;width:100%;margin-top:.3rem">
      </label>
      <label style="font-size:.85rem;color:var(--muted)">Chat-ID
        <input type="text" id="tgChatId" placeholder="-100123456789" maxlength="30"
               style="display:block;width:240px;margin-top:.3rem">
      </label>
      <div style="display:flex;gap:.75rem;flex-wrap:wrap">
        <button class="btn btn-ok" onclick="saveTelegram()">Speichern</button>
        <button class="btn-ghost" onclick="clearTelegram()">Deaktivieren</button>
      </div>
    </div>
    <div id="tgMsg" style="margin-top:.75rem;font-size:.85rem"></div>
  </div>
</div>
<div id="shop" class="panel">
  <div class="toolbar" style="padding-top:1.25rem">
    <span style="font-size:.9rem;color:var(--muted)">Wird automatisch bef&uuml;llt wenn Artikel ausgelagert werden.</span>
    <button class="btn-ghost" onclick="loadShop()">&#x21BB;</button>
    <button class="btn btn-sm" onclick="clearBought()">Gekaufte l&ouml;schen</button>
  </div>
  <div class="table-wrap">
    <div id="shopLoading">Lade&hellip;</div>
    <ul id="shopList" style="list-style:none;padding:0 1.5rem 1rem;display:none"></ul>
    <div id="shopEmpty" class="empty" style="display:none">Einkaufsliste ist leer.</div>
  </div>
  <div class="add-form">
    <h3>Manuell hinzuf&uuml;gen</h3>
    <div class="form-row" style="margin-top:.75rem">
      <label>Artikel<input type="text" id="shopName" placeholder="z.B. Hackfleisch" maxlength="60"></label>
      <label>Kategorie<input type="text" id="shopCat" placeholder="z.B. Fleisch" maxlength="30"></label>
      <button class="btn btn-ok" onclick="addShopItem()">Hinzuf&uuml;gen</button>
    </div>
    <div id="shopMsg" style="margin-top:.75rem;font-size:.85rem"></div>
  </div>
</div>
<div id="stats" class="panel">
  <div class="toolbar" style="padding-top:1.25rem">
    <span id="statsSummary" style="font-size:.9rem;color:var(--muted)"></span>
    <button class="btn-ghost" onclick="loadStats()">&#x21BB;</button>
    <button class="btn btn-sm" onclick="exportStats()">CSV</button>
  </div>
  <div class="table-wrap">
    <div id="statsLoading">Lade&hellip;</div>
    <table id="statsTable" style="display:none">
      <thead><tr>
        <th onclick="sortStats('name')" style="cursor:pointer">Produkt &#x2195;</th>
        <th>Kategorie</th>
        <th onclick="sortStats('addedDate')" style="cursor:pointer">Eingelagert &#x2195;</th>
        <th onclick="sortStats('removedDate')" style="cursor:pointer">Ausgelagert &#x2195;</th>
        <th onclick="sortStats('storageDays')" style="cursor:pointer">Lagerdauer &#x2195;</th>
      </tr></thead>
      <tbody id="statsBody"></tbody>
    </table>
    <div id="statsEmpty" class="empty" style="display:none">Noch keine Auslagerungen aufgezeichnet.</div>
  </div>
</div>
<div id="system" class="panel">
  <div style="padding:1.5rem;max-width:580px">
    <h2 style="margin-bottom:1rem">Backup &amp; Restore</h2>
    <p style="color:var(--muted);font-size:.9rem;margin-bottom:1rem">
      JSON-Dateien k&ouml;nnen heruntergeladen und sp&auml;ter wieder hochgeladen werden.
    </p>
    <div class="add-form" style="margin-bottom:1rem">
      <h3>Inventar</h3>
      <div style="display:flex;gap:.75rem;flex-wrap:wrap;margin-top:.75rem">
        <a class="btn" href="/api/backup/inventory" download="inventar.json">&#x2B07; Herunterladen</a>
        <label class="btn btn-ok" style="cursor:pointer">&#x2B06; Hochladen
          <input type="file" accept=".json" style="display:none" onchange="uploadBackup('inventory',this)">
        </label>
      </div>
    </div>
    <div class="add-form" style="margin-bottom:1rem">
      <h3>Vorlagen</h3>
      <div style="display:flex;gap:.75rem;flex-wrap:wrap;margin-top:.75rem">
        <a class="btn" href="/api/backup/products" download="vorlagen.json">&#x2B07; Herunterladen</a>
        <label class="btn btn-ok" style="cursor:pointer">&#x2B06; Hochladen
          <input type="file" accept=".json" style="display:none" onchange="uploadBackup('products',this)">
        </label>
      </div>
    </div>
    <div id="backupMsg" style="font-size:.85rem;margin-bottom:1.5rem"></div>
    <hr style="border-color:var(--border);margin:1.5rem 0">
    <h2 style="margin-bottom:1rem">OTA-Passwort</h2>
    <p style="color:var(--muted);font-size:.9rem;margin-bottom:1rem">
      Benutzer: <code style="color:var(--accent)">admin</code>. &Auml;nderung wirkt nach Neustart.
    </p>
    <div style="display:flex;gap:.75rem;align-items:flex-end;flex-wrap:wrap">
      <label style="font-size:.85rem;color:var(--muted)">Neues Passwort
        <input type="password" id="otaPw" maxlength="32"
               style="display:block;width:220px;margin-top:.3rem">
      </label>
      <button class="btn btn-ok" onclick="saveOtaPw()">Speichern</button>
    </div>
    <div id="otaMsg" style="margin-top:.75rem;font-size:.85rem"></div>
    <hr style="border-color:var(--border);margin:1.5rem 0">
    <h2 style="margin-bottom:1rem">Ger&auml;t &amp; Server-Sync</h2>
    <p style="color:var(--muted);font-size:.9rem;margin-bottom:1rem">
      Ger&auml;tename und Raum erscheinen im zentralen Server-Dashboard.
      Server-URL leer lassen f&uuml;r reinen Standalone-Betrieb.
    </p>
    <div style="display:flex;flex-direction:column;gap:.75rem">
      <label style="font-size:.85rem;color:var(--muted)">Ger&auml;tename
        <input type="text" id="devName" placeholder="z.B. Gefrierschrank Keller" maxlength="40"
               style="display:block;width:100%;margin-top:.3rem">
      </label>
      <label style="font-size:.85rem;color:var(--muted)">Raum / Standort
        <input type="text" id="devRoom" placeholder="z.B. Keller" maxlength="30"
               style="display:block;width:260px;margin-top:.3rem">
      </label>
      <label style="font-size:.85rem;color:var(--muted)">Server-URL
        <input type="text" id="devServer" placeholder="http://192.168.1.100:8000" maxlength="80"
               style="display:block;width:100%;margin-top:.3rem">
      </label>
      <button class="btn btn-ok" style="width:fit-content" onclick="saveDevConfig()">Speichern</button>
    </div>
    <div id="devMsg" style="margin-top:.75rem;font-size:.85rem"></div>
  </div>
</div>
<script>
let catColorMap={};  // name→hex, wird nach loadCats() befüllt
function catHex(name){return catColorMap[name]||'#333';}
function switchTab(id,btn){
  document.querySelectorAll('.panel').forEach(p=>p.classList.remove('active'));
  document.querySelectorAll('nav button').forEach(b=>b.classList.remove('active'));
  document.getElementById(id).classList.add('active');btn.classList.add('active');
  if(id==='cp')loadCP();
  if(id==='cats')loadCats();
  if(id==='wifi')loadWifi();
  if(id==='mqtt'){loadMqtt();loadTelegram();}
  if(id==='shop')loadShop();
  if(id==='stats')loadStats();
  if(id==='system'){loadOtaPw();loadDevConfig();}
}
async function loadWifi(){
  try{const r=await fetch('/api/wifi-config');const d=await r.json();
    document.getElementById('wifiCurrentSsid').textContent=d.ssid||'(nicht konfiguriert)';
    if(d.ssid)document.getElementById('wifiSsid').value=d.ssid;}catch(e){}
}
async function saveWifi(){
  const ssid=document.getElementById('wifiSsid').value.trim();
  const pw=document.getElementById('wifiPw').value;
  const msg=document.getElementById('wifiMsg');
  if(!ssid){msg.style.color='var(--danger)';msg.textContent='SSID fehlt!';return;}
  msg.style.color='var(--muted)';msg.textContent='Speichere…';
  try{const r=await fetch('/api/wifi-config',{method:'POST',
    headers:{'Content-Type':'application/json'},body:JSON.stringify({ssid,password:pw})});
    if(r.ok){msg.style.color='var(--ok)';msg.textContent='Gespeichert! Gerät startet neu…';}
    else{const d=await r.json();msg.style.color='var(--danger)';msg.textContent=d.error||'Fehler';}
  }catch(e){msg.style.color='var(--danger)';msg.textContent='Verbindungsfehler';}
}
function esc(s){return String(s).replace(/&/g,'&amp;').replace(/</g,'&lt;').replace(/>/g,'&gt;').replace(/"/g,'&quot;');}
function fmtDate(iso){if(!iso||iso.length<10)return'–';const[y,m,d]=iso.split('-');return d+'.'+m+'.'+y;}
function statusOf(d){return d<0||d<=3?'danger':d<=7?'warn':'ok';}
function daysLabel(d){return d<0?'Abgelaufen':d===0?'Heute!':d===1?'1 Tag':d+' Tage';}
function catBadge(cat){return'<span class="cat-badge" style="background:'+catHex(cat)+'">'+esc(cat)+'</span>';}
let allItems=[],sortKey='daysLeft',sortAsc=true;
async function loadInv(){
  document.getElementById('invLoading').style.display='block';
  document.getElementById('invTable').style.display='none';
  document.getElementById('invEmpty').style.display='none';
  try{const[inv,stats,cats]=await Promise.all([
    fetch('/api/inventory').then(r=>r.json()),
    fetch('/api/stats').then(r=>r.json()),
    fetch('/api/categories').then(r=>r.json())]);
    allItems=inv.items||[];
    document.getElementById('statTotal').textContent=stats.total!=null?stats.total:'–';
    document.getElementById('statExpiring').textContent=stats.expiring!=null?stats.expiring:'–';
    document.getElementById('statExpired').textContent=stats.expired!=null?stats.expired:'–';
    // Kategorie-Filter befüllen
    const csel=document.getElementById('catFilterSelect');
    const prev=csel.value;
    csel.innerHTML='<option value="all">Alle Kategorien</option>';
    cats.forEach(c=>{const o=document.createElement('option');o.value=c.name||c;o.textContent=c.name||c;csel.appendChild(o);});
    if(prev&&prev!='all')csel.value=prev;
    renderInv();}catch(e){document.getElementById('invLoading').textContent='Fehler: '+e.message;}
}
function renderInv(){
  const q=document.getElementById('searchInput').value.toLowerCase();
  const f=document.getElementById('filterSelect').value;
  const cf=document.getElementById('catFilterSelect').value;
  let items=allItems.filter(it=>{
    const ms=!q||it.name.toLowerCase().includes(q)||(it.brand||'').toLowerCase().includes(q)||it.barcode.includes(q);
    if(!ms)return false;
    if(cf&&cf!=='all'&&it.category!==cf)return false;
    const s=statusOf(it.daysLeft);
    if(f==='expiring')return s==='warn';if(f==='expired')return it.daysLeft<0;
    if(f==='ok')return s==='ok';return true;});
  items.sort((a,b)=>{let va=a[sortKey]!=null?a[sortKey]:'',vb=b[sortKey]!=null?b[sortKey]:'';
    if(typeof va==='string'){va=va.toLowerCase();vb=vb.toLowerCase();}
    return sortAsc?(va<vb?-1:va>vb?1:0):(va>vb?-1:va<vb?1:0);});
  document.getElementById('invLoading').style.display='none';
  if(items.length===0){document.getElementById('invTable').style.display='none';
    document.getElementById('invEmpty').style.display='block';return;}
  document.getElementById('invEmpty').style.display='none';
  document.getElementById('invTable').style.display='table';
  const tbody=document.getElementById('invBody');tbody.innerHTML='';
  items.forEach(it=>{const sc=statusOf(it.daysLeft);const tr=document.createElement('tr');
    tr.innerHTML='<td style="font-weight:500">'+esc(it.name)+'</td>'+
      '<td style="color:var(--muted);font-size:.8rem">'+esc(it.brand||'–')+'</td>'+
      '<td style="color:var(--'+(sc==='ok'?'ok':sc==='warn'?'warn':'danger')+')">'+fmtDate(it.expiry)+'</td>'+
      '<td><span class="pill pill-'+sc+'">'+daysLabel(it.daysLeft)+'</span></td>'+
      '<td>'+it.qty+'</td><td class="mono">'+esc(it.barcode)+'</td>'+
      '<td><button class="btn btn-danger btn-sm" onclick="deleteItem(\''+esc(it.barcode)+'\',\''+esc(it.expiry)+'\')">Löschen</button></td>';
    tbody.appendChild(tr);});
}
function sortBy(k){if(sortKey===k)sortAsc=!sortAsc;else{sortKey=k;sortAsc=true;}renderInv();}
async function deleteItem(barcode,expiry){
  if(!confirm('Löschen?\n'+barcode+'\nMHD: '+expiry))return;
  const r=await fetch('/api/item?barcode='+encodeURIComponent(barcode)+'&expiry='+encodeURIComponent(expiry),{method:'DELETE'});
  if(r.ok){allItems=allItems.filter(it=>!(it.barcode===barcode&&it.expiry===expiry));renderInv();}
  else alert('Fehler beim Löschen');
}
function exportCSV(){
  const rows=[['Produkt','Marke','MHD','Verbleibend','Menge','Barcode']];
  allItems.forEach(it=>rows.push([it.name,it.brand||'',it.expiry,it.daysLeft,it.qty,it.barcode]));
  const csv=rows.map(r=>r.map(c=>'"'+String(c).replace(/"/g,'""')+'"').join(',')).join('\n');
  const a=document.createElement('a');
  a.href='data:text/csv;charset=utf-8,'+encodeURIComponent(csv);
  a.download='inventar_'+new Date().toISOString().slice(0,10)+'.csv';a.click();
}
let allCP=[],categories=[];
async function loadCP(){
  document.getElementById('cpLoading').style.display='block';
  document.getElementById('cpTable').style.display='none';
  document.getElementById('cpEmpty').style.display='none';
  try{
    const[cpData,catData]=await Promise.all([
      fetch('/api/custom-products').then(r=>r.json()),
      fetch('/api/categories').then(r=>r.json())]);
    allCP=cpData.products||[];
    categories=catData.map(c=>c.name||c);
    catColorMap={};catData.forEach(c=>{if(c.name)catColorMap[c.name]=rgb565toHex(c.bg||0);});
    buildCategorySelect();renderCP();
  }catch(e){document.getElementById('cpLoading').textContent='Fehler: '+e.message;}
}
function buildCategorySelect(){
  const sel=document.getElementById('cpCategory');sel.innerHTML='';
  categories.forEach(c=>{const o=document.createElement('option');o.value=c;o.textContent=c;sel.appendChild(o);});
}
function renderCP(){
  document.getElementById('cpLoading').style.display='none';
  if(allCP.length===0){document.getElementById('cpTable').style.display='none';
    document.getElementById('cpEmpty').style.display='block';return;}
  document.getElementById('cpEmpty').style.display='none';
  document.getElementById('cpTable').style.display='table';
  const bycat={};
  allCP.forEach((p,idx)=>{const c=p.category||'Sonstiges';if(!bycat[c])bycat[c]=[];bycat[c].push(Object.assign({},p,{_idx:idx}));});
  const tbody=document.getElementById('cpBody');tbody.innerHTML='';
  categories.forEach(cat=>{if(!bycat[cat]||bycat[cat].length===0)return;
    const hdr=document.createElement('tr');hdr.className='cat-group-header';
    hdr.innerHTML='<td colspan="6">'+catBadge(cat)+'&nbsp;'+esc(cat)+' ('+bycat[cat].length+')</td>';
    tbody.appendChild(hdr);
    bycat[cat].forEach(p=>{const tr=document.createElement('tr');
      const days=p.defaultDays>0?'<span class="pill pill-ok">'+p.defaultDays+' Tage</span>':'<span style="color:var(--muted)">manuell</span>';
      tr.innerHTML='<td>'+catBadge(p.category)+'</td><td style="font-weight:500">'+esc(p.name)+'</td>'+
        '<td style="color:var(--muted)">'+esc(p.brand||'–')+'</td>'+
        '<td>'+days+'</td>'+
        '<td class="mono">'+esc(p.barcode||'–')+'</td>'+
        '<td><button class="btn btn-danger btn-sm" onclick="deleteCP('+p._idx+')">L&ouml;schen</button></td>';
      tbody.appendChild(tr);});});
}
async function addCP(){
  const name=document.getElementById('cpName').value.trim();
  const brand=document.getElementById('cpBrand').value.trim();
  const barcode=document.getElementById('cpBarcode').value.trim();
  const category=document.getElementById('cpCategory').value;
  const defaultDays=parseInt(document.getElementById('cpDefaultDays').value)||0;
  const msg=document.getElementById('cpMsg');
  if(!name){msg.style.color='var(--danger)';msg.textContent='Name ist Pflichtfeld.';return;}
  const r=await fetch('/api/custom-products',{method:'POST',
    headers:{'Content-Type':'application/json'},
    body:JSON.stringify({name,brand,barcode,category,defaultDays})});
  const data=await r.json();
  if(r.ok){msg.style.color='var(--ok)';
    msg.textContent='✓ "'+name+'" ('+category+(defaultDays>0?', MHD: '+defaultDays+' Tage':'')+') hinzugefügt.';
    document.getElementById('cpName').value='';document.getElementById('cpBrand').value='';
    document.getElementById('cpBarcode').value='';document.getElementById('cpDefaultDays').value='0';
    loadCP();
  }else{msg.style.color='var(--danger)';msg.textContent='Fehler: '+(data.error||r.status);}
}
async function deleteCP(index){
  if(!confirm('Vorlage löschen?'))return;
  const r=await fetch('/api/custom-products?index='+index,{method:'DELETE'});
  if(r.ok)loadCP();else alert('Fehler beim Löschen');
}

// ── Kategorien-Verwaltung ─────────────────────────────────────
let allCats=[],colorPresets=[];
async function loadCats(){
  document.getElementById('catsLoading').style.display='block';
  document.getElementById('catsTable').style.display='none';
  try{
    const[catsData,presetsData]=await Promise.all([
      fetch('/api/categories').then(r=>r.json()),
      fetch('/api/color-presets').then(r=>r.json())]);
    allCats=catsData;colorPresets=presetsData;
    buildColorPicker();renderCats();
  }catch(e){document.getElementById('catsLoading').textContent='Fehler: '+e.message;}
}
function rgb565toHex(v){
  const r=(v>>11&0x1F)<<3;const g=(v>>5&0x3F)<<2;const b=(v&0x1F)<<3;
  return'#'+[r,g,b].map(x=>x.toString(16).padStart(2,'0')).join('');
}
function buildColorPicker(){
  const div=document.getElementById('colorPicker');div.innerHTML='';
  colorPresets.forEach((p,i)=>{
    const btn=document.createElement('button');
    btn.type='button';
    btn.title=p.label;
    btn.style.cssText='width:28px;height:28px;border-radius:50%;border:3px solid transparent;cursor:pointer;background:'+p.hex;
    btn.onclick=()=>{
      div.querySelectorAll('button').forEach(b=>b.style.borderColor='transparent');
      btn.style.borderColor='#fff';
      document.getElementById('catBg').value=p.bg;
      document.getElementById('catFg').value=p.fg;
    };
    div.appendChild(btn);
  });
  // Ersten Preset vorauswählen
  if(colorPresets.length>0){
    div.querySelector('button').style.borderColor='#fff';
    document.getElementById('catBg').value=colorPresets[0].bg;
    document.getElementById('catFg').value=colorPresets[0].fg;
  }
}
function renderCats(){
  document.getElementById('catsLoading').style.display='none';
  if(allCats.length===0){document.getElementById('catsTable').style.display='none';
    document.getElementById('catsEmpty').style.display='block';return;}
  document.getElementById('catsEmpty').style.display='none';
  document.getElementById('catsTable').style.display='table';
  const tbody=document.getElementById('catsBody');tbody.innerHTML='';
  allCats.forEach((c,i)=>{
    const hex=rgb565toHex(c.bg);
    const tr=document.createElement('tr');
    tr.innerHTML='<td><span style="display:inline-block;width:20px;height:20px;border-radius:4px;background:'+hex+'"></span></td>'+
      '<td style="font-weight:500">'+esc(c.name)+'</td>'+
      '<td><button class="btn btn-danger btn-sm" onclick="deleteCat('+i+')">L&ouml;schen</button></td>';
    tbody.appendChild(tr);});
}
async function addCat(){
  const name=document.getElementById('catName').value.trim();
  const bg=parseInt(document.getElementById('catBg').value)||0;
  const fg=parseInt(document.getElementById('catFg').value)||65535;
  const msg=document.getElementById('catMsg');
  if(!name){msg.style.color='var(--danger)';msg.textContent='Name ist Pflichtfeld.';return;}
  if(allCats.length>=12){msg.style.color='var(--danger)';msg.textContent='Maximum 12 Kategorien.';return;}
  const newList=[...allCats,{name,bg,fg}];
  const r=await fetch('/api/categories',{method:'POST',
    headers:{'Content-Type':'application/json'},body:JSON.stringify(newList)});
  const data=await r.json();
  if(r.ok){msg.style.color='var(--ok)';msg.textContent='✓ "'+name+'" hinzugefügt.';
    document.getElementById('catName').value='';loadCats();
  }else{msg.style.color='var(--danger)';msg.textContent='Fehler: '+(data.error||r.status);}
}
async function deleteCat(i){
  if(!confirm('Kategorie "'+allCats[i].name+'" löschen?\nAlle Vorlagen dieser Kategorie werden unsichtbar!'))return;
  const newList=allCats.filter((_,idx)=>idx!==i);
  const r=await fetch('/api/categories',{method:'POST',
    headers:{'Content-Type':'application/json'},body:JSON.stringify(newList)});
  if(r.ok)loadCats();else alert('Fehler');
}

// ── Einkaufsliste ─────────────────────────────────────────────
async function loadShop(){
  document.getElementById('shopLoading').style.display='block';
  document.getElementById('shopList').style.display='none';
  try{
    const d=await fetch('/api/shopping-list').then(r=>r.json());
    const items=d.items||[];
    document.getElementById('shopLoading').style.display='none';
    if(items.length===0){document.getElementById('shopEmpty').style.display='block';return;}
    document.getElementById('shopEmpty').style.display='none';
    const ul=document.getElementById('shopList');ul.innerHTML='';ul.style.display='block';
    items.forEach((it,i)=>{
      const li=document.createElement('li');
      li.style.cssText='display:flex;align-items:center;gap:.75rem;padding:.6rem 0;border-bottom:1px solid var(--border)';
      li.innerHTML='<input type="checkbox"'+(it.bought?' checked':'')+'onchange="toggleBought('+i+',this)" style="width:18px;height:18px;cursor:pointer">'+
        '<span style="flex:1;'+(it.bought?'text-decoration:line-through;color:var(--muted)':'')+'">'
        +esc(it.name)+(it.category?'<span style="font-size:.75rem;color:var(--muted);margin-left:.4rem">'+esc(it.category)+'</span>':'')+'</span>'+
        '<button class="btn-ghost btn-sm" onclick="deleteShopItem('+i+')" style="padding:.2rem .5rem">&#x2715;</button>';
      ul.appendChild(li);});
  }catch(e){document.getElementById('shopLoading').textContent='Fehler: '+e.message;}
}
async function toggleBought(i,cb){
  await fetch('/api/shopping-list/'+i,{method:'PATCH'});
  loadShop();
}
async function deleteShopItem(i){
  await fetch('/api/shopping-list/'+i,{method:'DELETE'});loadShop();
}
async function clearBought(){
  await fetch('/api/shopping-list/bought',{method:'DELETE'});loadShop();
}
async function addShopItem(){
  const name=document.getElementById('shopName').value.trim();
  const cat=document.getElementById('shopCat').value.trim();
  const msg=document.getElementById('shopMsg');
  if(!name){msg.style.color='var(--danger)';msg.textContent='Name fehlt.';return;}
  const r=await fetch('/api/shopping-list',{method:'POST',
    headers:{'Content-Type':'application/json'},body:JSON.stringify({name,category:cat})});
  if(r.ok){document.getElementById('shopName').value='';msg.style.color='var(--ok)';
    msg.textContent='✓ Hinzugefügt.';loadShop();}
  else{msg.style.color='var(--danger)';msg.textContent='Fehler.';}
}

// ── Gerät & Server-Sync ───────────────────────────────────────
async function loadDevConfig(){
  try{const d=await fetch('/api/device-config').then(r=>r.json());
    document.getElementById('devName').value=d.name||'';
    document.getElementById('devRoom').value=d.room||'';
    document.getElementById('devServer').value=d.serverUrl||'';}catch(e){}
}
async function saveDevConfig(){
  const name=document.getElementById('devName').value.trim();
  const room=document.getElementById('devRoom').value.trim();
  const serverUrl=document.getElementById('devServer').value.trim();
  const msg=document.getElementById('devMsg');
  const r=await fetch('/api/device-config',{method:'POST',
    headers:{'Content-Type':'application/json'},body:JSON.stringify({name,room,serverUrl})});
  if(r.ok){msg.style.color='var(--ok)';msg.textContent='✓ Gespeichert. Server-Sync aktiv nach Neustart.';}
  else{msg.style.color='var(--danger)';msg.textContent='Fehler.';}
}

// ── Statistik ─────────────────────────────────────────────────
let allStats=[],statSortKey='removedDate',statSortAsc=false;
async function loadStats(){
  document.getElementById('statsLoading').style.display='block';
  document.getElementById('statsTable').style.display='none';
  try{
    const d=await fetch('/api/storage-stats').then(r=>r.json());
    allStats=d.stats||[];
    // Zusammenfassung
    const avg=allStats.length?Math.round(allStats.reduce((s,x)=>s+x.storageDays,0)/allStats.length):0;
    document.getElementById('statsSummary').textContent=allStats.length+' Auslagerungen, Ø '+avg+' Tage';
    renderStats();
  }catch(e){document.getElementById('statsLoading').textContent='Fehler: '+e.message;}
}
function renderStats(){
  document.getElementById('statsLoading').style.display='none';
  if(allStats.length===0){document.getElementById('statsTable').style.display='none';
    document.getElementById('statsEmpty').style.display='block';return;}
  document.getElementById('statsEmpty').style.display='none';
  document.getElementById('statsTable').style.display='table';
  let items=[...allStats];
  items.sort((a,b)=>{let va=a[statSortKey]??'',vb=b[statSortKey]??'';
    if(typeof va==='string'){va=va.toLowerCase();vb=vb.toLowerCase();}
    return statSortAsc?(va<vb?-1:va>vb?1:0):(va>vb?-1:va<vb?1:0);});
  const tbody=document.getElementById('statsBody');tbody.innerHTML='';
  items.forEach(s=>{const tr=document.createElement('tr');
    tr.innerHTML='<td style="font-weight:500">'+esc(s.name)+'</td>'+
      '<td>'+catBadge(s.category)+'</td>'+
      '<td class="mono">'+esc(s.addedDate||'–')+'</td>'+
      '<td class="mono">'+esc(s.removedDate||'–')+'</td>'+
      '<td><span class="pill pill-ok">'+s.storageDays+' Tage</span></td>';
    tbody.appendChild(tr);});
}
function sortStats(k){if(statSortKey===k)statSortAsc=!statSortAsc;else{statSortKey=k;statSortAsc=true;}renderStats();}
function exportStats(){
  const rows=[['Produkt','Kategorie','Eingelagert','Ausgelagert','Lagerdauer (Tage)']];
  allStats.forEach(s=>rows.push([s.name,s.category||'',s.addedDate||'',s.removedDate||'',s.storageDays]));
  const csv=rows.map(r=>r.map(c=>'"'+String(c).replace(/"/g,'""')+'"').join(',')).join('\n');
  const a=document.createElement('a');
  a.href='data:text/csv;charset=utf-8,'+encodeURIComponent(csv);
  a.download='lagerdauer_'+new Date().toISOString().slice(0,10)+'.csv';a.click();
}

// ── Backup / Restore ─────────────────────────────────────────
async function uploadBackup(type,input){
  const file=input.files[0];if(!file)return;
  const msg=document.getElementById('backupMsg');
  msg.style.color='var(--muted)';msg.textContent='Lade hoch…';
  try{
    const text=await file.text();
    const r=await fetch('/api/backup/'+type,{method:'POST',
      headers:{'Content-Type':'application/json'},body:text});
    if(r.ok){msg.style.color='var(--ok)';msg.textContent='✓ Wiederhergestellt!';}
    else{msg.style.color='var(--danger)';msg.textContent='Fehler beim Hochladen';}
  }catch(e){msg.style.color='var(--danger)';msg.textContent='Verbindungsfehler';}
  input.value='';
}

// ── OTA-Passwort ─────────────────────────────────────────────
async function loadOtaPw(){
  try{const d=await fetch('/api/ota-config').then(r=>r.json());
    document.getElementById('otaPw').value=d.password||'';}catch(e){}
}
async function saveOtaPw(){
  const pw=document.getElementById('otaPw').value;
  const msg=document.getElementById('otaMsg');
  if(!pw){msg.style.color='var(--danger)';msg.textContent='Passwort darf nicht leer sein.';return;}
  const r=await fetch('/api/ota-config',{method:'POST',
    headers:{'Content-Type':'application/json'},body:JSON.stringify({password:pw})});
  if(r.ok){msg.style.color='var(--ok)';msg.textContent='✓ Gespeichert. Wirkung nach Neustart.';}
  else{msg.style.color='var(--danger)';msg.textContent='Fehler';}
}

// ── Telegram ─────────────────────────────────────────────────
async function loadTelegram(){
  try{const d=await fetch('/api/telegram-config').then(r=>r.json());
    document.getElementById('tgToken').value=d.token||'';
    document.getElementById('tgChatId').value=d.chatId||'';}catch(e){}
}
async function saveTelegram(){
  const token=document.getElementById('tgToken').value.trim();
  const chatId=document.getElementById('tgChatId').value.trim();
  const msg=document.getElementById('tgMsg');
  msg.style.color='var(--muted)';msg.textContent='Speichere…';
  const r=await fetch('/api/telegram-config',{method:'POST',
    headers:{'Content-Type':'application/json'},body:JSON.stringify({token,chatId})});
  if(r.ok){msg.style.color='var(--ok)';msg.textContent='✓ Gespeichert.';}
  else{msg.style.color='var(--danger)';msg.textContent='Fehler';}
}
async function clearTelegram(){
  document.getElementById('tgToken').value='';document.getElementById('tgChatId').value='';
  await saveTelegram();
}

async function loadMqtt(){
  try{const r=await fetch('/api/mqtt-config');const d=await r.json();
    document.getElementById('mqttHost').value=d.host||'';
    document.getElementById('mqttPort').value=d.port||1883;
    document.getElementById('mqttPrefix').value=d.prefix||'lebensmittel';
  }catch(e){}
}
async function saveMqtt(){
  const host=document.getElementById('mqttHost').value.trim();
  const port=parseInt(document.getElementById('mqttPort').value)||1883;
  const prefix=document.getElementById('mqttPrefix').value.trim()||'lebensmittel';
  const msg=document.getElementById('mqttMsg');
  msg.style.color='var(--muted)';msg.textContent='Speichere…';
  try{const r=await fetch('/api/mqtt-config',{method:'POST',
    headers:{'Content-Type':'application/json'},body:JSON.stringify({host,port,prefix})});
    if(r.ok){msg.style.color='var(--ok)';msg.textContent='✓ Gespeichert. Wirkung nach nächstem Verbindungsaufbau.';}
    else{msg.style.color='var(--danger)';msg.textContent='Fehler';}
  }catch(e){msg.style.color='var(--danger)';msg.textContent='Verbindungsfehler';}
}
async function clearMqtt(){
  if(!confirm('MQTT deaktivieren (Host leeren)?'))return;
  document.getElementById('mqttHost').value='';
  await saveMqtt();
}

loadInv();
if('serviceWorker' in navigator)navigator.serviceWorker.register('/sw.js');
</script>
</body>
</html>)RAW";

WebInterface::WebInterface(Inventory &inventory, CustomProducts &customProducts,
                           StorageStats &stats, ShoppingList &shopping)
    : _server(80), _inventory(inventory), _customProducts(customProducts),
      _stats(stats), _shopping(shopping) {}

void WebInterface::begin() {
    _server.on("/", HTTP_GET, [](AsyncWebServerRequest *req) {
        req->send(200, "text/html; charset=utf-8", INDEX_HTML);
    });

    // ── PWA ──────────────────────────────────────────────────
    _server.on("/manifest.json", HTTP_GET, [](AsyncWebServerRequest *req) {
        req->send(200, "application/manifest+json", R"({
"name":"Lebensmittel Scanner","short_name":"Lager",
"start_url":"/","display":"standalone",
"background_color":"#111827","theme_color":"#3b82f6",
"icons":[{"src":"/icon.svg","sizes":"any","type":"image/svg+xml","purpose":"any maskable"}]
})");
    });
    _server.on("/icon.svg", HTTP_GET, [](AsyncWebServerRequest *req) {
        req->send(200, "image/svg+xml",
            "<svg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 100 100'>"
            "<rect width='100' height='100' rx='18' fill='#3b82f6'/>"
            "<rect x='18' y='38' width='64' height='44' rx='6' fill='white' opacity='.93'/>"
            "<rect x='28' y='22' width='44' height='22' rx='6' fill='white' opacity='.7'/>"
            "<rect x='47' y='38' width='6' height='44' fill='#3b82f6' opacity='.4'/>"
            "<rect x='18' y='59' width='64' height='6' fill='#3b82f6' opacity='.4'/>"
            "</svg>");
    });
    _server.on("/sw.js", HTTP_GET, [](AsyncWebServerRequest *req) {
        req->send(200, "application/javascript",
            "const C='lager-v1';\n"
            "self.addEventListener('install',e=>{e.waitUntil(caches.open(C).then(c=>c.addAll(['/'])));self.skipWaiting();});\n"
            "self.addEventListener('activate',e=>{e.waitUntil(caches.keys().then(k=>Promise.all(k.filter(n=>n!==C).map(n=>caches.delete(n)))))});\n"
            "self.addEventListener('fetch',e=>{\n"
            "  if(e.request.method!=='GET'||e.request.url.includes('/api/'))return;\n"
            "  e.respondWith(fetch(e.request).then(r=>{const rc=r.clone();caches.open(C).then(c=>c.put(e.request,rc));return r;}).catch(()=>caches.match(e.request)));\n"
            "});\n"
        );
    });

    // ── Einkaufsliste ─────────────────────────────────────────
    _server.on("/api/shopping-list", HTTP_GET, [this](AsyncWebServerRequest *req) {
        JsonDocument doc;
        JsonArray arr = doc["items"].to<JsonArray>();
        for (const auto &it : _shopping.items()) {
            JsonObject o = arr.add<JsonObject>();
            o["name"]     = it.name;
            o["category"] = it.category;
            o["date"]     = it.addedDate;
            o["bought"]   = it.bought;
        }
        doc["count"] = _shopping.count();
        String out; serializeJson(doc, out);
        req->send(200, "application/json", out);
    });
    _server.on("/api/shopping-list", HTTP_POST,
        [](AsyncWebServerRequest *req) {},
        nullptr,
        [this](AsyncWebServerRequest *req, uint8_t *data, size_t len, size_t index, size_t total) {
            JsonDocument doc;
            if (deserializeJson(doc, (const char*)data, len)) {
                req->send(400, "application/json", "{\"error\":\"JSON\"}"); return;
            }
            String name = doc["name"] | "";
            String cat  = doc["category"] | "";
            if (name.isEmpty()) { req->send(400, "application/json", "{\"error\":\"Name fehlt\"}"); return; }
            bool ok = _shopping.add(name, cat);
            req->send(ok ? 200 : 500, "application/json", ok ? "{\"ok\":true}" : "{\"error\":\"Fehler\"}");
        }
    );
    _server.on("^\\/api\\/shopping-list\\/bought$", HTTP_DELETE, [this](AsyncWebServerRequest *req) {
        _shopping.clearBought();
        req->send(200, "application/json", "{\"ok\":true}");
    });
    _server.on("^\\/api\\/shopping-list\\/([0-9]+)$", HTTP_PATCH, [this](AsyncWebServerRequest *req) {
        int idx = req->pathArg(0).toInt();
        bool ok = _shopping.markBought(idx);
        req->send(ok ? 200 : 404, "application/json", ok ? "{\"ok\":true}" : "{\"error\":\"Index\"}");
    });
    _server.on("^\\/api\\/shopping-list\\/([0-9]+)$", HTTP_DELETE, [this](AsyncWebServerRequest *req) {
        int idx = req->pathArg(0).toInt();
        bool ok = _shopping.remove(idx);
        req->send(ok ? 200 : 404, "application/json", ok ? "{\"ok\":true}" : "{\"error\":\"Index\"}");
    });

    // ── Gerät & Server-Sync ───────────────────────────────────
    _server.on("/api/device-config", HTTP_GET, [](AsyncWebServerRequest *req) {
        Preferences p; p.begin("sync", true);
        String name = p.getString("name", "Lebensmittel Scanner");
        String room = p.getString("room", "");
        String url  = p.getString("url",  "");
        p.end();
        JsonDocument doc;
        doc["name"]      = name;
        doc["room"]      = room;
        doc["serverUrl"] = url;
        doc["mac"]       = WiFi.macAddress();
        doc["ip"]        = WiFi.localIP().toString();
        String out; serializeJson(doc, out);
        req->send(200, "application/json", out);
    });
    _server.on("/api/device-config", HTTP_POST,
        [](AsyncWebServerRequest *req) {},
        nullptr,
        [](AsyncWebServerRequest *req, uint8_t *data, size_t len, size_t index, size_t total) {
            JsonDocument doc;
            if (deserializeJson(doc, (const char*)data, len)) {
                req->send(400, "application/json", "{\"error\":\"JSON\"}"); return;
            }
            Preferences p; p.begin("sync", false);
            p.putString("name", doc["name"] | "Lebensmittel Scanner");
            p.putString("room", doc["room"] | "");
            p.putString("url",  doc["serverUrl"] | "");
            p.end();
            req->send(200, "application/json", "{\"ok\":true}");
        }
    );

    // ── Inventar ─────────────────────────────────────────────

    _server.on("/api/inventory", HTTP_GET, [this](AsyncWebServerRequest *req) {
        auto items = _inventory.items();
        std::sort(items.begin(), items.end(), [](const InventoryItem &a, const InventoryItem &b) {
            return a.expiryDate < b.expiryDate;
        });
        JsonDocument doc;
        JsonArray arr = doc["items"].to<JsonArray>();
        time_t now = time(nullptr);
        for (const auto &it : items) {
            JsonObject obj = arr.add<JsonObject>();
            obj["barcode"]  = it.barcode;
            obj["name"]     = it.name;
            obj["brand"]    = it.brand;
            obj["category"] = it.category;
            obj["expiry"]   = it.expiryDate;
            obj["added"]    = it.addedDate;
            obj["qty"]      = it.quantity;
            if (it.expiryDate.length() >= 10) {
                struct tm tm = {};
                tm.tm_year = it.expiryDate.substring(0,4).toInt()-1900;
                tm.tm_mon  = it.expiryDate.substring(5,7).toInt()-1;
                tm.tm_mday = it.expiryDate.substring(8,10).toInt();
                obj["daysLeft"] = (int)(difftime(mktime(&tm),now)/86400.0);
            }
        }
        doc["count"] = items.size();
        String out; serializeJson(doc,out);
        req->send(200,"application/json",out);
    });

    _server.on("/api/item", HTTP_DELETE, [this](AsyncWebServerRequest *req) {
        if (!req->hasParam("barcode") || !req->hasParam("expiry")) {
            req->send(400,"application/json","{\"error\":\"Fehlende Parameter\"}"); return;
        }
        bool ok = _inventory.removeItem(req->getParam("barcode")->value(),
                                        req->getParam("expiry")->value());
        req->send(ok?200:404,"application/json",ok?"{\"ok\":true}":"{\"error\":\"Nicht gefunden\"}");
    });

    _server.on("/api/stats", HTTP_GET, [this](AsyncWebServerRequest *req) {
        JsonDocument doc;
        doc["total"]    = _inventory.count();
        doc["expiring"] = _inventory.expiringIn(WARNING_DAYS);
        doc["expired"]  = _inventory.expiredCount();
        String out; serializeJson(doc,out);
        req->send(200,"application/json",out);
    });

    // ── Kategorien ───────────────────────────────────────────

    // GET: gibt Objekte mit name, bg, fg, hex zurück
    _server.on("/api/categories", HTTP_GET, [](AsyncWebServerRequest *req) {
        JsonDocument doc;
        JsonArray arr = doc.to<JsonArray>();
        for (const auto &c : g_categories) {
            JsonObject o = arr.add<JsonObject>();
            o["name"] = c.name;
            o["bg"]   = c.bgColor;
            o["fg"]   = c.textColor;
        }
        String out; serializeJson(doc, out);
        req->send(200, "application/json", out);
    });

    // GET: verfügbare Farbpalette
    _server.on("/api/color-presets", HTTP_GET, [](AsyncWebServerRequest *req) {
        JsonDocument doc;
        JsonArray arr = doc.to<JsonArray>();
        for (int i = 0; i < NUM_COLOR_PRESETS; i++) {
            JsonObject o = arr.add<JsonObject>();
            o["label"] = COLOR_PRESETS[i].label;
            o["bg"]    = COLOR_PRESETS[i].bg;
            o["fg"]    = COLOR_PRESETS[i].fg;
            o["hex"]   = COLOR_PRESETS[i].hex;
        }
        String out; serializeJson(doc, out);
        req->send(200, "application/json", out);
    });

    // POST: setzt komplette Kategorienliste (max. 8)
    _server.on("/api/categories", HTTP_POST,
        [](AsyncWebServerRequest *req) {},
        nullptr,
        [](AsyncWebServerRequest *req, uint8_t *data, size_t len, size_t index, size_t total) {
            JsonDocument doc;
            if (deserializeJson(doc, (const char*)data, len)) {
                req->send(400, "application/json", "{\"error\":\"Ungültiges JSON\"}"); return;
            }
            JsonArray arr = doc.as<JsonArray>();
            if (!arr) {
                req->send(400, "application/json", "{\"error\":\"Array erwartet\"}"); return;
            }
            std::vector<CategoryDef> newCats;
            for (JsonObject o : arr) {
                CategoryDef c;
                c.name      = o["name"].as<String>(); c.name.trim();
                c.bgColor   = o["bg"].as<uint16_t>();
                c.textColor = o["fg"].as<uint16_t>();
                if (!c.name.isEmpty() && (int)newCats.size() < 12)
                    newCats.push_back(c);
            }
            if (newCats.empty()) {
                req->send(400, "application/json", "{\"error\":\"Min. 1 Kategorie\"}"); return;
            }
            g_categories = newCats;
            bool ok = categoryManager.save();
            req->send(ok ? 200 : 500, "application/json",
                      ok ? "{\"ok\":true}" : "{\"error\":\"Speichern fehlgeschlagen\"}");
        }
    );

    // ── Eigene Produkte ───────────────────────────────────────

    _server.on("/api/custom-products", HTTP_GET, [this](AsyncWebServerRequest *req) {
        JsonDocument doc;
        JsonArray arr = doc["products"].to<JsonArray>();
        for (const auto &p : _customProducts.items()) {
            JsonObject obj = arr.add<JsonObject>();
            obj["name"]        = p.name;
            obj["brand"]       = p.brand;
            obj["barcode"]     = p.barcode;
            obj["category"]    = p.category;
            obj["defaultDays"] = p.defaultDays;
        }
        doc["count"] = _customProducts.count();
        String out; serializeJson(doc, out);
        req->send(200, "application/json", out);
    });

    _server.on("/api/custom-products", HTTP_POST,
        [](AsyncWebServerRequest *req) {},
        nullptr,
        [this](AsyncWebServerRequest *req, uint8_t *data, size_t len, size_t index, size_t total) {
            JsonDocument doc;
            if (deserializeJson(doc, (const char*)data, len)) {
                req->send(400, "application/json", "{\"error\":\"Ungültiges JSON\"}"); return;
            }
            CustomProduct p;
            p.name        = doc["name"].as<String>(); p.name.trim();
            p.brand       = doc["brand"].as<String>();
            p.barcode     = doc["barcode"].as<String>();
            p.category    = doc["category"].as<String>();
            p.defaultDays = doc["defaultDays"] | 0;
            if (p.name.isEmpty()) {
                req->send(400, "application/json", "{\"error\":\"Name erforderlich\"}"); return;
            }
            if (p.category.isEmpty()) p.category = "Sonstiges";
            if (p.defaultDays < 0) p.defaultDays = 0;
            bool ok = _customProducts.add(p);
            req->send(ok ? 200 : 500, "application/json",
                      ok ? "{\"ok\":true}" : "{\"error\":\"Speichern fehlgeschlagen\"}");
        }
    );

    _server.on("/api/custom-products", HTTP_DELETE, [this](AsyncWebServerRequest *req) {
        if (!req->hasParam("index")) {
            req->send(400,"application/json","{\"error\":\"index fehlt\"}"); return;
        }
        bool ok = _customProducts.remove(req->getParam("index")->value().toInt());
        req->send(ok?200:404,"application/json",ok?"{\"ok\":true}":"{\"error\":\"Nicht gefunden\"}");
    });

    // ── WiFi-Konfiguration ────────────────────────────────────

    _server.on("/api/wifi-config", HTTP_GET, [](AsyncWebServerRequest *req) {
        Preferences prefs;
        prefs.begin("wifi", true);
        String ssid = prefs.getString("ssid", "");
        prefs.end();
        req->send(200, "application/json", "{\"ssid\":\"" + ssid + "\"}");
    });

    _server.on("/api/wifi-config", HTTP_POST,
        [](AsyncWebServerRequest *req) {},
        nullptr,
        [](AsyncWebServerRequest *req, uint8_t *data, size_t len, size_t index, size_t total) {
            JsonDocument doc;
            if (deserializeJson(doc,(const char*)data,len)) {
                req->send(400,"application/json","{\"error\":\"Ungültiges JSON\"}"); return;
            }
            String ssid = doc["ssid"] | "";
            String pw   = doc["password"] | "";
            if (ssid.isEmpty()) {
                req->send(400,"application/json","{\"error\":\"SSID fehlt\"}"); return;
            }
            Preferences prefs;
            prefs.begin("wifi", false);
            prefs.putString("ssid",     ssid);
            prefs.putString("password", pw);
            prefs.end();
            req->send(200,"application/json","{\"ok\":true,\"restart\":true}");
            delay(400);
            ESP.restart();
        }
    );

    // ── Lagerdauer-Statistik ─────────────────────────────────

    _server.on("/api/storage-stats", HTTP_GET, [this](AsyncWebServerRequest *req) {
        JsonDocument doc;
        JsonArray arr = doc["stats"].to<JsonArray>();
        for (const auto &s : _stats.items()) {
            JsonObject o = arr.add<JsonObject>();
            o["name"]        = s.name;
            o["category"]    = s.category;
            o["addedDate"]   = s.addedDate;
            o["removedDate"] = s.removedDate;
            o["storageDays"] = s.storageDays;
        }
        doc["count"] = _stats.count();
        String out; serializeJson(doc, out);
        req->send(200, "application/json", out);
    });

    // ── Backup / Restore ─────────────────────────────────────

    _server.on("/api/backup/inventory", HTTP_GET, [](AsyncWebServerRequest *req) {
        req->send(LittleFS, INVENTORY_FILE, "application/json");
    });
    _server.on("/api/backup/products", HTTP_GET, [](AsyncWebServerRequest *req) {
        req->send(LittleFS, CUSTOM_PRODUCTS_FILE, "application/json");
    });
    _server.on("/api/backup/inventory", HTTP_POST,
        [](AsyncWebServerRequest *req) {},
        nullptr,
        [this](AsyncWebServerRequest *req, uint8_t *data, size_t len, size_t index, size_t total) {
            static File _upFile;
            if (index == 0) _upFile = LittleFS.open("/inv_upload.tmp", "w");
            if (_upFile) _upFile.write(data, len);
            if (index + len == total) {
                if (_upFile) _upFile.close();
                LittleFS.remove(INVENTORY_FILE);
                LittleFS.rename("/inv_upload.tmp", INVENTORY_FILE);
                _inventory.load();
                req->send(200, "application/json", "{\"ok\":true}");
            }
        }
    );
    _server.on("/api/backup/products", HTTP_POST,
        [](AsyncWebServerRequest *req) {},
        nullptr,
        [this](AsyncWebServerRequest *req, uint8_t *data, size_t len, size_t index, size_t total) {
            static File _upFile2;
            if (index == 0) _upFile2 = LittleFS.open("/cp_upload.tmp", "w");
            if (_upFile2) _upFile2.write(data, len);
            if (index + len == total) {
                if (_upFile2) _upFile2.close();
                LittleFS.remove(CUSTOM_PRODUCTS_FILE);
                LittleFS.rename("/cp_upload.tmp", CUSTOM_PRODUCTS_FILE);
                _customProducts.load();
                req->send(200, "application/json", "{\"ok\":true}");
            }
        }
    );

    // ── OTA-Passwort ─────────────────────────────────────────

    _server.on("/api/ota-config", HTTP_GET, [](AsyncWebServerRequest *req) {
        Preferences p; p.begin("ota", true);
        String pw = p.getString("password", "lebensmittel");
        p.end();
        req->send(200, "application/json", "{\"password\":\"" + pw + "\"}");
    });
    _server.on("/api/ota-config", HTTP_POST,
        [](AsyncWebServerRequest *req) {},
        nullptr,
        [](AsyncWebServerRequest *req, uint8_t *data, size_t len, size_t index, size_t total) {
            JsonDocument doc;
            if (deserializeJson(doc, (const char*)data, len)) {
                req->send(400, "application/json", "{\"error\":\"JSON Fehler\"}"); return;
            }
            String pw = doc["password"] | "";
            if (pw.isEmpty()) { req->send(400, "application/json", "{\"error\":\"Leer\"}"); return; }
            Preferences p; p.begin("ota", false);
            p.putString("password", pw);
            p.end();
            req->send(200, "application/json", "{\"ok\":true}");
        }
    );

    // ── Telegram-Konfiguration ───────────────────────────────

    _server.on("/api/telegram-config", HTTP_GET, [](AsyncWebServerRequest *req) {
        Preferences p; p.begin("telegram", true);
        String token  = p.getString("token",  "");
        String chatId = p.getString("chatid", "");
        p.end();
        JsonDocument doc; doc["token"] = token; doc["chatId"] = chatId;
        String out; serializeJson(doc, out);
        req->send(200, "application/json", out);
    });
    _server.on("/api/telegram-config", HTTP_POST,
        [](AsyncWebServerRequest *req) {},
        nullptr,
        [](AsyncWebServerRequest *req, uint8_t *data, size_t len, size_t index, size_t total) {
            JsonDocument doc;
            if (deserializeJson(doc, (const char*)data, len)) {
                req->send(400, "application/json", "{\"error\":\"JSON Fehler\"}"); return;
            }
            String token  = doc["token"]  | "";
            String chatId = doc["chatId"] | "";
            Preferences p; p.begin("telegram", false);
            p.putString("token",  token);
            p.putString("chatid", chatId);
            p.end();
            req->send(200, "application/json", "{\"ok\":true}");
        }
    );

    // ── MQTT-Konfiguration ────────────────────────────────────

    _server.on("/api/mqtt-config", HTTP_GET, [](AsyncWebServerRequest *req) {
        Preferences prefs; prefs.begin("mqtt", true);
        String host   = prefs.getString("host",   "");
        uint16_t port = prefs.getUShort("port",   1883);
        String prefix = prefs.getString("prefix", "lebensmittel");
        prefs.end();
        JsonDocument doc;
        doc["host"]   = host;
        doc["port"]   = port;
        doc["prefix"] = prefix;
        String out; serializeJson(doc, out);
        req->send(200, "application/json", out);
    });

    _server.on("/api/mqtt-config", HTTP_POST,
        [](AsyncWebServerRequest *req) {},
        nullptr,
        [](AsyncWebServerRequest *req, uint8_t *data, size_t len, size_t index, size_t total) {
            JsonDocument doc;
            if (deserializeJson(doc, (const char*)data, len)) {
                req->send(400, "application/json", "{\"error\":\"Ungültiges JSON\"}"); return;
            }
            String host   = doc["host"]   | "";
            uint16_t port = doc["port"]   | 1883;
            String prefix = doc["prefix"] | "lebensmittel";
            Preferences prefs; prefs.begin("mqtt", false);
            prefs.putString("host",   host);
            prefs.putUShort("port",   port);
            prefs.putString("prefix", prefix);
            prefs.end();
            req->send(200, "application/json", "{\"ok\":true}");
        }
    );

    _server.onNotFound([](AsyncWebServerRequest *req) { req->send(404,"text/plain","Not found"); });

    // OTA-Update unter /update (Benutzer: admin, PW aus NVS)
    {
        Preferences p; p.begin("ota", true);
        String pw = p.getString("password", "lebensmittel");
        p.end();
        ElegantOTA.begin(&_server, "admin", pw.c_str());
    }

    _server.begin();
    Serial.println("[Web] Server gestartet auf Port 80");
}

void WebInterface::loop() {
    ElegantOTA.loop();
}
