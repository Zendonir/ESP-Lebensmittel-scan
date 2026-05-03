#include "WebInterface.h"
#include "config.h"
#include "UIConfig.h"
#include "FontConfig.h"
#include "BarcodeScanner.h"
extern bool g_useNumpad;
#include "CategoryManager.h"
#include "StorageStats.h"
#include "ShoppingList.h"
#include <LittleFS.h>
#include <Preferences.h>
#include <ArduinoJson.h>
#include <Update.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <time.h>

static const char OTA_PAGE_HTML[] = R"HTML(<!DOCTYPE html>
<html lang="de"><head><meta charset="UTF-8">
<title>OTA Update</title>
<style>body{font-family:system-ui;background:#111827;color:#f9fafb;
display:flex;justify-content:center;padding:3rem;}
.box{background:#1f2937;padding:2rem;border-radius:.75rem;max-width:420px;width:100%;}
h2{margin-bottom:1.5rem;}p{color:#9ca3af;font-size:.9rem;margin-bottom:1rem;}
input[type=file]{display:block;margin:1rem 0;color:#f9fafb;}
button{background:#3b82f6;color:white;border:none;padding:.65rem 1.5rem;
border-radius:.5rem;cursor:pointer;width:100%;font-size:1rem;}
a{color:#3b82f6;font-size:.85rem;display:block;margin-top:1rem;text-align:center;}
</style></head><body><div class="box">
<h2>&#x2B06; Firmware-Update</h2>
<p>.bin-Datei aus dem PlatformIO-Build (.pio/build/…/firmware.bin)</p>
<form method="POST" action="/ota" enctype="multipart/form-data">
<input type="file" name="firmware" accept=".bin" required>
<button type="submit">Firmware flashen</button>
</form>
<a href="/">&#x2190; Zur&uuml;ck</a>
</div></body></html>)HTML";

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
  .dtab{background:none;border:1px solid var(--border);color:var(--muted);
        padding:.4rem .9rem;border-radius:.4rem;cursor:pointer;font-size:.85rem;}
  .dtab:hover{border-color:var(--accent);color:var(--accent);}
  .dtab-active{border-color:var(--accent);color:var(--accent);background:rgba(59,130,246,.08);}
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
  <button onclick="switchTab('scanlogs',this)">&#x1F4A0; Scans</button>
  <button onclick="switchTab('design',this)">&#x1F3A8; Design</button>
  <button onclick="location.href='/ota'" style="margin-left:auto">&#x2B06; OTA</button>
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
        <th>Marke</th><th>MHD (Tage)</th><th>Anzahl</th><th>Barcode</th><th></th>
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
      <label>Beschreibung <span style="color:var(--muted)">(optional)</span>
        <textarea id="cpDescription" placeholder="z.B. Vakuumiert, 500g Packungen" maxlength="200" rows="2"
          style="width:100%;resize:vertical;font-size:.9rem;padding:.4rem .6rem;border-radius:.5rem;border:1px solid var(--border);background:var(--surface);color:var(--text)"></textarea>
      </label>
      <label>Standard-MHD (Tage) <span style="color:var(--muted)">(0 = manuell am Ger&auml;t eingeben)</span>
        <input type="number" id="cpDefaultDays" placeholder="z.B. 180" min="0" max="3650" value="0" style="width:120px">
      </label>
      <label style="display:flex;align-items:center;gap:.6rem;cursor:pointer">
        <input type="checkbox" id="cpAskQty" style="width:18px;height:18px;cursor:pointer">
        <span>Anzahl / Portionen beim Einlagern abfragen</span>
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
    <h2 style="margin-bottom:1rem">GitHub OTA – Automatisches Update</h2>
    <p style="color:var(--muted);font-size:.9rem;margin-bottom:1rem">
      Verf&uuml;gbare Releases laden und Firmware direkt flashen.
    </p>
    <div style="display:flex;gap:.75rem;align-items:flex-end;flex-wrap:wrap;margin-bottom:.75rem">
      <label style="font-size:.85rem;color:var(--muted)">Version
        <select id="ghVersionSelect" style="display:block;width:260px;margin-top:.3rem" disabled>
          <option value="">&#x2013; Versionen laden &#x2013;</option>
        </select>
      </label>
      <button class="btn" onclick="loadGithubReleases()">&#x1F504; Laden</button>
      <button class="btn btn-ok" id="ghOtaFlash" onclick="flashGithubRelease()" style="display:none">&#x26A1; Installieren</button>
    </div>
    <span id="ghOtaMsg" style="font-size:.85rem;color:var(--muted)"></span>
    <div id="ghOtaProgress" style="display:none;margin-top:1rem">
      <div style="background:var(--border);border-radius:99px;height:8px;overflow:hidden">
        <div id="ghOtaBar" style="background:var(--accent);height:100%;width:0%;transition:width .3s"></div>
      </div>
      <p id="ghOtaStatus" style="font-size:.8rem;color:var(--muted);margin-top:.4rem"></p>
    </div>
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
               style="display:block;width:100%;margin-top:.3rem"
               oninput="devServerChanged()">
      </label>
      <label style="font-size:.85rem;color:var(--muted)">Haushalt
        <div style="display:flex;gap:.5rem;align-items:center;margin-top:.3rem">
          <select id="devHousehold" style="flex:1"></select>
          <button class="btn" style="width:auto;padding:.4rem .8rem" onclick="loadHouseholds()">&#x21BA;</button>
        </div>
        <span style="font-size:.75rem;color:var(--muted)">Diesem Ger&auml;t zugewiesener Haushalt</span>
      </label>
      <label style="font-size:.85rem;color:var(--muted)">Datumseingabe am Ger&auml;t
        <select id="dateInputMode" style="display:block;margin-top:.3rem;width:200px">
          <option value="drum">Drum Roller (Wischen)</option>
          <option value="numpad">Ziffernblock</option>
        </select>
      </label>
      <button class="btn btn-ok" style="width:fit-content" onclick="saveDevConfig()">Speichern</button>
    </div>
    <div id="devMsg" style="margin-top:.75rem;font-size:.85rem"></div>
    <hr style="border-color:var(--border);margin:1.5rem 0">
    <h2 style="margin-bottom:1rem">Barcode-Scanner</h2>
    <p style="color:var(--muted);font-size:.9rem;margin-bottom:1rem">
      Eingebauter UART-Scanner (GM861) oder externer BLE-HID-Scanner (z.&nbsp;B.&nbsp;Inateck BCST-23).
      Am Scanner muss vorher &bdquo;BLE HID&ldquo; aktiviert werden (QR-Code im Handbuch).
      &Auml;nderungen wirken nach Neustart.
    </p>
    <div style="display:flex;flex-direction:column;gap:.75rem;max-width:440px">
      <label style="font-size:.85rem;color:var(--muted)">Scanner-Typ
        <select id="scannerMode" style="display:block;margin-top:.3rem;width:260px"
                onchange="document.getElementById('btNameRow').style.display=this.value==='bt'?'flex':'none'">
          <option value="uart">Eingebaut (UART / GM861)</option>
          <option value="bt">BLE HID (externer Scanner)</option>
        </select>
      </label>
      <label id="btNameRow" style="font-size:.85rem;color:var(--muted);display:none">Ger&auml;tename-Filter (leer&nbsp;= beliebiger HID-Scanner)
        <input type="text" id="btDevName" maxlength="32" placeholder="z.B. BCST-23"
               style="display:block;width:260px;margin-top:.3rem">
      </label>
      <button class="btn btn-ok" style="width:fit-content" onclick="saveScannerCfg()">Speichern &amp; Neustart</button>
    </div>
    <div id="scannerMsg" style="margin-top:.75rem;font-size:.85rem"></div>
    <hr style="border-color:var(--border);margin:1.5rem 0">
    <h2 style="margin-bottom:1rem">Schriftgr&ouml;&szlig;en am Ger&auml;t</h2>
    <p style="color:var(--muted);font-size:.9rem;margin-bottom:1rem">
      Pixelh&ouml;he der Schrift: 8&thinsp;px, 16&thinsp;px, 24&thinsp;px oder 32&thinsp;px (Vielfaches von 8).
      &Auml;nderungen wirken sofort ohne Neustart.
    </p>
    <div style="display:grid;grid-template-columns:1fr 1fr;gap:.75rem;max-width:440px">
      <label style="font-size:.85rem;color:var(--muted)">&#x1F4D1; &Uuml;berschriften / Titel
        <div style="display:flex;align-items:center;gap:.3rem;margin-top:.3rem">
          <input type="number" id="fszTitle" min="8" max="32" step="1" style="width:70px">
          <span style="color:var(--muted);font-size:.8rem">px</span>
        </div>
      </label>
      <label style="font-size:.85rem;color:var(--muted)">&#x1F4DD; Haupttext / Namen
        <div style="display:flex;align-items:center;gap:.3rem;margin-top:.3rem">
          <input type="number" id="fszBody" min="8" max="32" step="1" style="width:70px">
          <span style="color:var(--muted);font-size:.8rem">px</span>
        </div>
      </label>
      <label style="font-size:.85rem;color:var(--muted)">&#x1F50D; Kleine Hinweistexte
        <div style="display:flex;align-items:center;gap:.3rem;margin-top:.3rem">
          <input type="number" id="fszSmall" min="8" max="32" step="1" style="width:70px">
          <span style="color:var(--muted);font-size:.8rem">px</span>
        </div>
      </label>
      <label style="font-size:.85rem;color:var(--muted)">&#x1F522; Hervorgehobene Zahlen
        <div style="display:flex;align-items:center;gap:.3rem;margin-top:.3rem">
          <input type="number" id="fszValue" min="8" max="32" step="1" style="width:70px">
          <span style="color:var(--muted);font-size:.8rem">px</span>
        </div>
      </label>
      <label style="font-size:.85rem;color:var(--muted)">&#x1F522; Ziffernblock-Tasten
        <div style="display:flex;align-items:center;gap:.3rem;margin-top:.3rem">
          <input type="number" id="fszKey" min="8" max="32" step="1" style="width:70px">
          <span style="color:var(--muted);font-size:.8rem">px</span>
        </div>
      </label>
      <label style="font-size:.85rem;color:var(--muted)">&#x1F518; Button-Beschriftungen
        <div style="display:flex;align-items:center;gap:.3rem;margin-top:.3rem">
          <input type="number" id="fszBtn" min="8" max="32" step="1" style="width:70px">
          <span style="color:var(--muted);font-size:.8rem">px</span>
        </div>
      </label>
    </div>
    <button class="btn btn-ok" style="width:fit-content;margin-top:.75rem" onclick="saveFontSizes()">Speichern</button>
    <div id="fszMsg" style="margin-top:.75rem;font-size:.85rem"></div>
    <hr style="border-color:var(--border);margin:1.5rem 0">
    <h2 style="margin-bottom:1rem">Diagnose</h2>
    <div style="display:flex;gap:.75rem;flex-wrap:wrap;align-items:center">
      <button class="btn" onclick="testBuzzer()">&#x1F50A; Buzzer testen</button>
      <span id="buzzerMsg" style="font-size:.85rem;color:var(--muted)"></span>
    </div>
  </div>
</div>
<div id="scanlogs" class="panel">
  <div class="toolbar" style="padding:1.5rem 1.5rem 0">
    <h2 style="flex:1;margin:0">Scan-Verlauf</h2>
    <button class="btn-ghost" onclick="loadScanlogs()">&#x1F504; Aktualisieren</button>
  </div>
  <div class="table-wrap">
    <table id="scanlogsTable">
      <thead>
        <tr><th>Zeit</th><th>Barcode</th></tr>
      </thead>
      <tbody id="scanlogsTbody">
        <tr><td colspan="2" style="text-align:center;color:var(--muted)">Wird geladen&hellip;</td></tr>
      </tbody>
    </table>
  </div>
  <div id="scanlogsEmpty" class="empty" style="display:none">Noch keine Scans aufgezeichnet</div>
</div>

<div id="design" class="panel">
<div style="padding:1.5rem;max-width:1050px">
<h2 style="margin-bottom:1rem">&#x1F3A8; Design &amp; Konfiguration</h2>

<div style="display:flex;gap:.4rem;flex-wrap:wrap;margin-bottom:1.5rem;padding-bottom:.75rem;border-bottom:1px solid var(--border)">
  <button id="dt-themes"   class="dtab dtab-active" onclick="switchDTab('themes',this)">&#x1F3A8; Themes</button>
  <button id="dt-colors"   class="dtab" onclick="switchDTab('colors',this)">&#x1F308; Farben</button>
  <button id="dt-layout"   class="dtab" onclick="switchDTab('layout',this)">&#x1F4D0; Layout</button>
  <button id="dt-behavior" class="dtab" onclick="switchDTab('behavior',this)">&#x2699;&#xFE0F; Verhalten</button>
  <button id="dt-portex"   class="dtab" onclick="switchDTab('portex',this)">&#x1F4E4; Export/Import</button>
</div>

<div style="display:flex;gap:2rem;flex-wrap:wrap;align-items:flex-start">
<div style="flex:1;min-width:280px">

<div id="dsec-themes">
  <p style="font-size:.8rem;color:var(--muted);margin-bottom:.75rem">Vordefiniertes Thema w&auml;hlen &mdash; danach <strong>Speichern</strong> klicken.</p>
  <div style="display:flex;gap:.5rem;flex-wrap:wrap">
    <button class="btn-ghost" onclick="applyTheme('amoled')">&#x1F311; AMOLED</button>
    <button class="btn-ghost" onclick="applyTheme('ocean')">&#x1F30A; Ozean</button>
    <button class="btn-ghost" onclick="applyTheme('forest')">&#x1F333; Wald</button>
    <button class="btn-ghost" onclick="applyTheme('sunset')">&#x1F305; Sonnenuntergang</button>
    <button class="btn-ghost" onclick="applyTheme('light')">&#x2600;&#xFE0F; Hell</button>
    <button class="btn-ghost" onclick="applyTheme('neon')">&#x26A1; Neon</button>
    <button class="btn-ghost" onclick="applyTheme('candy')">&#x1F36C; Candy</button>
    <button class="btn-ghost" onclick="applyTheme('mocha')">&#x2615; Mocha</button>
  </div>
</div>

<div id="dsec-colors" hidden>
  <p style="font-size:.8rem;color:var(--muted);margin-bottom:.75rem">Alle Anzeigefarben individuell anpassen</p>
  <div id="colorPickers" style="display:grid;grid-template-columns:1fr 1fr;gap:.6rem"></div>
</div>

<div id="dsec-layout" hidden>
  <p style="font-size:.8rem;color:var(--muted);margin-bottom:.75rem">Abmessungen, Abst&auml;nde &amp; Erscheinungsbild</p>
  <div style="display:grid;grid-template-columns:1fr 1fr;gap:.6rem">
    <label style="font-size:.85rem;color:var(--muted)">Button-H&ouml;he (px)
      <input type="number" id="uiTbtnH" min="32" max="80" style="display:block;width:100%;margin-top:.3rem" oninput="updatePreview()">
    </label>
    <label style="font-size:.85rem;color:var(--muted)">Button-Abstand (px)
      <input type="number" id="uiTbtnMargin" min="0" max="20" style="display:block;width:100%;margin-top:.3rem" oninput="updatePreview()">
    </label>
    <label style="font-size:.85rem;color:var(--muted)">Button-Radius
      <input type="number" id="uiBtnRadius" min="0" max="28" style="display:block;width:100%;margin-top:.3rem" oninput="updatePreview()">
    </label>
    <label style="font-size:.85rem;color:var(--muted)">Listen-Radius
      <input type="number" id="uiListRadius" min="0" max="24" style="display:block;width:100%;margin-top:.3rem" oninput="updatePreview()">
    </label>
    <label style="font-size:.85rem;color:var(--muted)">Kategorie-Spalten
      <select id="uiCatCols" style="display:block;width:100%;margin-top:.3rem" onchange="updatePreview()">
        <option value="1">1 Spalte</option>
        <option value="2" selected>2 Spalten</option>
        <option value="3">3 Spalten</option>
      </select>
    </label>
    <label style="font-size:.85rem;color:var(--muted)">Kachel-Abstand (px)
      <input type="number" id="uiCatGap" min="2" max="20" style="display:block;width:100%;margin-top:.3rem" oninput="updatePreview()">
    </label>
    <label style="font-size:.85rem;color:var(--muted)">Zeilen-H&ouml;he (px)
      <input type="number" id="uiListItemH" min="40" max="100" style="display:block;width:100%;margin-top:.3rem" oninput="updatePreview()">
    </label>
    <label style="font-size:.85rem;color:var(--muted)">Kategorieheader-H. (px)
      <input type="number" id="uiCatHdrH" min="30" max="70" style="display:block;width:100%;margin-top:.3rem" oninput="updatePreview()">
    </label>
    <label style="font-size:.85rem;color:var(--muted)">Subheader-H. (px)
      <input type="number" id="uiSubHdrH" min="40" max="80" style="display:block;width:100%;margin-top:.3rem" oninput="updatePreview()">
    </label>
  </div>
  <div style="margin-top:1rem">
    <label style="font-size:.85rem;color:var(--muted);display:block;margin-bottom:.3rem">Helligkeit</label>
    <div style="display:flex;gap:.75rem;align-items:center">
      <input type="range" id="uiBrightness" min="50" max="255" value="220"
             style="flex:1" oninput="document.getElementById('brightVal').textContent=this.value;updatePreview()">
      <span id="brightVal" style="font-size:.85rem;min-width:30px">220</span>
    </div>
  </div>
</div>

<div id="dsec-behavior" hidden>
  <p style="font-size:.8rem;color:var(--muted);margin-bottom:.75rem">Anzeige- &amp; Systemverhalten</p>
  <div style="display:grid;grid-template-columns:1fr 1fr;gap:.6rem">
    <label style="font-size:.85rem;color:var(--muted)">&#x26A0;&#xFE0F; Warnung ab (Tage)
      <input type="number" id="uiWarnDays" min="1" max="60" style="display:block;width:100%;margin-top:.3rem" oninput="updatePreview()">
    </label>
    <label style="font-size:.85rem;color:var(--muted)">&#x1F534; Kritisch ab (Tage)
      <input type="number" id="uiDangerDays" min="1" max="30" style="display:block;width:100%;margin-top:.3rem" oninput="updatePreview()">
    </label>
    <label style="font-size:.85rem;color:var(--muted)">Stromsparmodus (Min, 0=nie)
      <input type="number" id="uiPowerSaveMin" min="0" max="60" style="display:block;width:100%;margin-top:.3rem" oninput="updatePreview()">
    </label>
    <label style="font-size:.85rem;color:var(--muted)">Datumsformat
      <select id="uiDateFmt" style="display:block;width:100%;margin-top:.3rem" onchange="updatePreview()">
        <option value="0">DD.MM.YYYY (Deutsch)</option>
        <option value="1">MM/DD/YYYY (US)</option>
        <option value="2">YYYY-MM-DD (ISO)</option>
      </select>
    </label>
    <label style="font-size:.85rem;color:var(--muted)">Uhr anzeigen
      <select id="uiShowClock" style="display:block;width:100%;margin-top:.3rem" onchange="updatePreview()">
        <option value="1">Ja</option>
        <option value="0">Nein</option>
      </select>
    </label>
    <div style="font-size:.85rem;color:var(--muted)">Signalton (Buzzer)
      <div style="display:flex;gap:.75rem;margin-top:.5rem;align-items:center">
        <label style="display:flex;gap:.35rem;align-items:center"><input type="checkbox" id="uiSoundOk"> &#x2705; Erfolg</label>
        <label style="display:flex;gap:.35rem;align-items:center"><input type="checkbox" id="uiSoundErr"> &#x274C; Fehler</label>
      </div>
    </div>
  </div>
</div>

<div id="dsec-portex" hidden>
  <p style="font-size:.8rem;color:var(--muted);margin-bottom:.75rem">Aktuelles Theme als JSON-Datei sichern oder ein gespeichertes laden</p>
  <div style="display:flex;gap:.75rem;flex-wrap:wrap;margin-bottom:1rem">
    <button class="btn btn-ok" onclick="exportThemeJSON()">&#x1F4E5; Als JSON exportieren</button>
    <label class="btn-ghost" style="cursor:pointer;display:inline-block">&#x1F4E4; JSON importieren
      <input type="file" id="importFile" accept=".json" style="display:none" onchange="importThemeJSON(this)">
    </label>
  </div>
  <textarea id="themeJsonPreview" rows="10" readonly
    style="width:100%;font-size:.72rem;font-family:monospace;background:var(--surface);color:var(--muted);border:1px solid var(--border);border-radius:6px;padding:.5rem;resize:vertical"></textarea>
</div>

<div style="margin-top:1.5rem;display:flex;gap:.75rem;flex-wrap:wrap;padding-top:1rem;border-top:1px solid var(--border)">
  <button class="btn btn-ok" onclick="saveDesign()">&#x1F4BE; Speichern &amp; Anwenden</button>
  <button class="btn-ghost" onclick="loadDesign()">&#x21BA; Zur&uuml;cksetzen</button>
</div>
<p id="designMsg" style="font-size:.85rem;margin-top:.6rem"></p>

</div>

<div style="flex:0 0 auto">
  <p style="font-size:.8rem;color:var(--muted);text-transform:uppercase;letter-spacing:.05em;margin-bottom:.5rem">Vorschau</p>
  <div style="display:flex;gap:.35rem;margin-bottom:.5rem;flex-wrap:wrap">
    <button id="pvbtn-cats" class="btn-ghost dtab-active" onclick="setPreviewMode('cats',this)" style="font-size:.75rem;padding:.2rem .6rem">Kategorien</button>
    <button id="pvbtn-list" class="btn-ghost" onclick="setPreviewMode('list',this)" style="font-size:.75rem;padding:.2rem .6rem">Produkte</button>
    <button id="pvbtn-date" class="btn-ghost" onclick="setPreviewMode('date',this)" style="font-size:.75rem;padding:.2rem .6rem">Datum</button>
  </div>
  <svg id="devicePreview" width="170" height="275"
       viewBox="0 0 280 456"
       style="border-radius:14px;border:2px solid var(--border);display:block">
  </svg>
</div>

</div>
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
  if(id==='system'){loadOtaPw();loadDevConfig();loadScannerCfg();loadFontSizes();}
  if(id==='scanlogs')loadScanlogs();
  if(id==='design')loadDesign();
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
    hdr.innerHTML='<td colspan="7">'+catBadge(cat)+'&nbsp;'+esc(cat)+' ('+bycat[cat].length+')</td>';
    tbody.appendChild(hdr);
    bycat[cat].forEach(p=>{const tr=document.createElement('tr');
      const days=p.defaultDays>0?'<span class="pill pill-ok">'+p.defaultDays+' Tage</span>':'<span style="color:var(--muted)">manuell</span>';
      const qty=p.askQty?'<span class="pill pill-ok">&#x2714;</span>':'<span style="color:var(--muted)">–</span>';
      const desc=p.description?'<div style="font-size:.8rem;color:var(--muted);margin-top:.15rem">'+esc(p.description)+'</div>':'';
      tr.innerHTML='<td>'+catBadge(p.category)+'</td>'+
        '<td style="font-weight:500">'+esc(p.name)+desc+'</td>'+
        '<td style="color:var(--muted)">'+esc(p.brand||'–')+'</td>'+
        '<td>'+days+'</td>'+
        '<td>'+qty+'</td>'+
        '<td class="mono">'+esc(p.barcode||'–')+'</td>'+
        '<td><button class="btn btn-danger btn-sm" onclick="deleteCP('+p._idx+')">L&ouml;schen</button></td>';
      tbody.appendChild(tr);});});
}
async function addCP(){
  const name=document.getElementById('cpName').value.trim();
  const brand=document.getElementById('cpBrand').value.trim();
  const barcode=document.getElementById('cpBarcode').value.trim();
  const category=document.getElementById('cpCategory').value;
  const description=document.getElementById('cpDescription').value.trim();
  const defaultDays=parseInt(document.getElementById('cpDefaultDays').value)||0;
  const askQty=document.getElementById('cpAskQty').checked;
  const msg=document.getElementById('cpMsg');
  if(!name){msg.style.color='var(--danger)';msg.textContent='Name ist Pflichtfeld.';return;}
  const r=await fetch('/api/custom-products',{method:'POST',
    headers:{'Content-Type':'application/json'},
    body:JSON.stringify({name,brand,barcode,category,description,defaultDays,askQty})});
  const data=await r.json();
  if(r.ok){msg.style.color='var(--ok)';
    msg.textContent='✓ "'+name+'" ('+category+(defaultDays>0?', MHD: '+defaultDays+' Tage':'')+(askQty?', Anzahl':'')+') hinzugefügt.';
    document.getElementById('cpName').value='';document.getElementById('cpBrand').value='';
    document.getElementById('cpBarcode').value='';document.getElementById('cpDefaultDays').value='0';
    document.getElementById('cpDescription').value='';document.getElementById('cpAskQty').checked=false;
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
let _devHouseholds=[];
async function loadHouseholds(){
  const sel=document.getElementById('devHousehold');
  const cur=sel.value;
  sel.innerHTML='<option value="0">-- kein Haushalt --</option>';
  _devHouseholds=[];
  try{
    const arr=await fetch('/api/household-options').then(r=>r.json());
    _devHouseholds=arr;
    arr.forEach(h=>{
      const o=document.createElement('option');
      o.value=h.id;o.textContent=h.name;sel.appendChild(o);});
    if(cur)sel.value=cur;
  }catch(e){}
}
function devServerChanged(){
  document.getElementById('devHousehold').innerHTML='<option value="0">-- kein Haushalt --</option>';
}
async function loadDevConfig(){
  try{const d=await fetch('/api/device-config').then(r=>r.json());
    document.getElementById('devName').value=d.name||'';
    document.getElementById('devRoom').value=d.room||'';
    document.getElementById('devServer').value=d.serverUrl||'';
    document.getElementById('dateInputMode').value=d.dateInput||'drum';
    await loadHouseholds();
    document.getElementById('devHousehold').value=d.householdId||0;
  }catch(e){}
}
async function saveDevConfig(){
  const name=document.getElementById('devName').value.trim();
  const room=document.getElementById('devRoom').value.trim();
  const serverUrl=document.getElementById('devServer').value.trim();
  const dateInput=document.getElementById('dateInputMode').value;
  const householdId=parseInt(document.getElementById('devHousehold').value)||0;
  const msg=document.getElementById('devMsg');
  const r=await fetch('/api/device-config',{method:'POST',
    headers:{'Content-Type':'application/json'},
    body:JSON.stringify({name,room,serverUrl,dateInput,householdId})});
  if(r.ok){msg.style.color='var(--ok)';msg.textContent='✓ Gespeichert.';}
  else{msg.style.color='var(--danger)';msg.textContent='Fehler.';}
}

// ── Scanner-Konfiguration ─────────────────────────────────────
async function loadScannerCfg(){
  try{const d=await fetch('/api/scanner-config').then(r=>r.json());
    document.getElementById('scannerMode').value=d.bt?'bt':'uart';
    document.getElementById('btDevName').value=d.btName||'';
    document.getElementById('btNameRow').style.display=d.bt?'flex':'none';
  }catch(e){}
}
async function saveScannerCfg(){
  const msg=document.getElementById('scannerMsg');
  const bt=(document.getElementById('scannerMode').value==='bt');
  const btName=document.getElementById('btDevName').value.trim();
  const r=await fetch('/api/scanner-config',{method:'POST',
    headers:{'Content-Type':'application/json'},
    body:JSON.stringify({bt,btName})});
  if(r.ok){
    msg.style.color='var(--ok)';msg.textContent='✓ Gespeichert. Gerät startet neu…';
    setTimeout(()=>location.reload(),4000);
  }else{msg.style.color='var(--danger)';msg.textContent='Fehler.';}
}

// ── Schriftgrößen ─────────────────────────────────────────────
async function loadFontSizes(){
  try{const d=await fetch('/api/font-config').then(r=>r.json());
    document.getElementById('fszTitle').value=d.title||16;
    document.getElementById('fszBody').value=d.body||16;
    document.getElementById('fszSmall').value=d.small||8;
    document.getElementById('fszValue').value=d.value||24;
    document.getElementById('fszKey').value=d.key||24;
    document.getElementById('fszBtn').value=d.btn||16;}catch(e){}
}
async function saveFontSizes(){
  const msg=document.getElementById('fszMsg');
  const clamp=v=>{const n=Math.round(parseInt(v));return n<8?8:n>32?32:n;};
  const body={
    title:clamp(document.getElementById('fszTitle').value),
    body:clamp(document.getElementById('fszBody').value),
    small:clamp(document.getElementById('fszSmall').value),
    value:clamp(document.getElementById('fszValue').value),
    key:clamp(document.getElementById('fszKey').value),
    btn:clamp(document.getElementById('fszBtn').value)};
  const r=await fetch('/api/font-config',{method:'POST',
    headers:{'Content-Type':'application/json'},body:JSON.stringify(body)});
  if(r.ok){msg.style.color='var(--ok)';msg.textContent='✓ Gespeichert.';}
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

// ── GitHub OTA ────────────────────────────────────────────────
let _ghOtaUrl='';
async function loadGithubReleases(){
  const sel=document.getElementById('ghVersionSelect');
  const msg=document.getElementById('ghOtaMsg');
  const btn=document.getElementById('ghOtaFlash');
  msg.style.color='var(--muted)';msg.textContent='Lade Releases…';
  btn.style.display='none';sel.disabled=true;
  try{
    const list=await fetch('/api/github-release').then(r=>r.json());
    if(!Array.isArray(list)||list.length===0){
      msg.style.color='var(--danger)';
      msg.textContent=list&&list.error?'Fehler: '+list.error:'Keine Releases mit Firmware gefunden.';
      return;
    }
    sel.innerHTML='';
    list.forEach(r=>{
      const o=document.createElement('option');
      o.value=r.url;
      o.textContent=r.tag+(r.size?' ('+r.size+')':'');
      sel.appendChild(o);
    });
    sel.disabled=false;
    _ghOtaUrl=sel.value;
    sel.onchange=()=>{_ghOtaUrl=sel.value;};
    msg.style.color='var(--ok)';
    msg.textContent=list.length+' Release(s) gefunden.';
    btn.style.display='';
  }catch(e){msg.style.color='var(--danger)';msg.textContent='Netzwerkfehler: '+e.message;}
}
async function flashGithubRelease(){
  if(!_ghOtaUrl)return;
  const prog=document.getElementById('ghOtaProgress');
  const bar=document.getElementById('ghOtaBar');
  const stat=document.getElementById('ghOtaStatus');
  const msg=document.getElementById('ghOtaMsg');
  prog.style.display='block';bar.style.width='10%';
  stat.textContent='Lade Firmware herunter und flashe…';
  msg.textContent='';
  try{
    const r=await fetch('/api/github-ota',{method:'POST',
      headers:{'Content-Type':'application/json'},
      body:JSON.stringify({url:_ghOtaUrl})});
    const d=await r.json();
    if(r.ok&&d.ok){
      bar.style.width='100%';
      stat.textContent='✓ Fertig! Gerät startet neu…';
      msg.style.color='var(--ok)';msg.textContent='Update erfolgreich.';
    }else{
      bar.style.width='0%';
      stat.textContent='Fehler: '+(d.error||r.status);
      msg.style.color='var(--danger)';
    }
  }catch(e){bar.style.width='0%';stat.textContent='Netzwerkfehler: '+e.message;
    msg.style.color='var(--danger)';}
}

// ── Diagnose ──────────────────────────────────────────────────
async function testBuzzer(){
  const msg=document.getElementById('buzzerMsg');
  msg.textContent='…';
  const r=await fetch('/api/buzzer-test',{method:'POST'});
  if(r.ok){msg.style.color='var(--ok)';msg.textContent='✓ Ton gespielt';}
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
async function loadScanlogs(){
  try{const r=await fetch('/api/scanlogs');const scans=await r.json();
    const tbody=document.getElementById('scanlogsTbody');
    const empty=document.getElementById('scanlogsEmpty');
    if(!scans||scans.length===0){
      tbody.innerHTML='';empty.style.display='block';
      return;
    }
    empty.style.display='none';
    tbody.innerHTML=scans.map(s=>{
      const d=new Date(s.ts*1000);
      const time=d.toLocaleString('de-DE',{hour:'2-digit',minute:'2-digit',second:'2-digit'});
      return `<tr><td>${time}</td><td style="font-family:monospace;font-size:.85rem">${s.barcode}</td></tr>`;
    }).join('');
  }catch(e){console.error(e);}
}

// ── Design-Editor ─────────────────────────────────────────────
const _colorFields=[
  {key:'bg',       label:'Hintergrund'},
  {key:'text',     label:'Text'},
  {key:'subtext',  label:'Hinweistext'},
  {key:'ok',       label:'Grün / OK'},
  {key:'warn',     label:'Warnung'},
  {key:'danger',   label:'Gefahr / Rot'},
  {key:'accent',   label:'Akzent / Cyan'},
  {key:'header',   label:'Kopfzeile'},
  {key:'surface',  label:'Oberfläche'},
  {key:'btn_ok',   label:'Button OK'},
  {key:'btn_back', label:'Button Zurück'},
];
const _themes={
  amoled:{bg:'#000000',text:'#FFFFFF',subtext:'#808080',ok:'#00FC00',warn:'#FF6800',
    danger:'#FF0000',accent:'#00FFFF',header:'#0C1213',surface:'#0C1861',
    btn_ok:'#003000',btn_back:'#142250',
    brightness:220,warning_days:7,danger_days:3,
    tbtn_h:48,btn_radius:10,tbtn_margin:8,list_radius:6,
    cat_cols:2,cat_gap:6,list_item_h:64,cat_hdr_h:44,sub_hdr_h:54,
    power_save_min:10,date_format:0,show_clock:1,sound_ok:1,sound_err:1},
  ocean:{bg:'#0A1628',text:'#D0E8FF',subtext:'#5080A0',ok:'#00CC88',warn:'#FF9900',
    danger:'#FF4444',accent:'#4488FF',header:'#0D2040',surface:'#122030',
    btn_ok:'#0A3020',btn_back:'#0A2040',
    brightness:200,warning_days:7,danger_days:3,
    tbtn_h:48,btn_radius:12,tbtn_margin:8,list_radius:8,
    cat_cols:2,cat_gap:6,list_item_h:64,cat_hdr_h:44,sub_hdr_h:54,
    power_save_min:10,date_format:0,show_clock:1,sound_ok:1,sound_err:1},
  forest:{bg:'#0A1408',text:'#D0FFD0',subtext:'#608060',ok:'#00FF44',warn:'#FFAA00',
    danger:'#FF2222',accent:'#44FF88',header:'#0C1C08',surface:'#122010',
    btn_ok:'#0A3010',btn_back:'#1A3010',
    brightness:200,warning_days:7,danger_days:3,
    tbtn_h:48,btn_radius:10,tbtn_margin:8,list_radius:6,
    cat_cols:2,cat_gap:8,list_item_h:64,cat_hdr_h:44,sub_hdr_h:54,
    power_save_min:10,date_format:0,show_clock:1,sound_ok:1,sound_err:1},
  sunset:{bg:'#120808',text:'#FFE8E0',subtext:'#806060',ok:'#00CC66',warn:'#FF8800',
    danger:'#FF2222',accent:'#FF6644',header:'#1C1008',surface:'#1A1010',
    btn_ok:'#0A2810',btn_back:'#2A1408',
    brightness:210,warning_days:7,danger_days:3,
    tbtn_h:48,btn_radius:10,tbtn_margin:8,list_radius:6,
    cat_cols:2,cat_gap:6,list_item_h:64,cat_hdr_h:44,sub_hdr_h:54,
    power_save_min:10,date_format:0,show_clock:1,sound_ok:1,sound_err:1},
  light:{bg:'#F0F0F0',text:'#111111',subtext:'#666666',ok:'#008800',warn:'#AA6600',
    danger:'#CC0000',accent:'#0066CC',header:'#DDDDDD',surface:'#E8E8E8',
    btn_ok:'#006600',btn_back:'#444444',
    brightness:255,warning_days:7,danger_days:3,
    tbtn_h:48,btn_radius:8,tbtn_margin:8,list_radius:6,
    cat_cols:2,cat_gap:6,list_item_h:64,cat_hdr_h:44,sub_hdr_h:54,
    power_save_min:10,date_format:0,show_clock:1,sound_ok:1,sound_err:1},
  neon:{bg:'#050510',text:'#E0E0FF',subtext:'#6060A0',ok:'#00FF88',warn:'#FFCC00',
    danger:'#FF2266',accent:'#BB44FF',header:'#0A0A20',surface:'#10103A',
    btn_ok:'#004433',btn_back:'#1A0A30',
    brightness:220,warning_days:7,danger_days:3,
    tbtn_h:52,btn_radius:14,tbtn_margin:8,list_radius:10,
    cat_cols:2,cat_gap:6,list_item_h:64,cat_hdr_h:44,sub_hdr_h:54,
    power_save_min:10,date_format:0,show_clock:1,sound_ok:1,sound_err:1},
  candy:{bg:'#1A0520',text:'#FFE0FF',subtext:'#A060A0',ok:'#44FF88',warn:'#FFAA22',
    danger:'#FF3366',accent:'#FF66CC',header:'#280A35',surface:'#2A0A38',
    btn_ok:'#0A4422',btn_back:'#380A44',
    brightness:210,warning_days:7,danger_days:3,
    tbtn_h:52,btn_radius:24,tbtn_margin:10,list_radius:12,
    cat_cols:2,cat_gap:8,list_item_h:68,cat_hdr_h:48,sub_hdr_h:56,
    power_save_min:10,date_format:0,show_clock:1,sound_ok:1,sound_err:1},
  mocha:{bg:'#1C1410',text:'#F0E8D8',subtext:'#907060',ok:'#88CC44',warn:'#E8A030',
    danger:'#CC3322',accent:'#D08844',header:'#241A12',surface:'#2A2018',
    btn_ok:'#224411',btn_back:'#3A2818',
    brightness:200,warning_days:7,danger_days:3,
    tbtn_h:48,btn_radius:10,tbtn_margin:8,list_radius:6,
    cat_cols:2,cat_gap:6,list_item_h:64,cat_hdr_h:44,sub_hdr_h:54,
    power_save_min:10,date_format:0,show_clock:1,sound_ok:1,sound_err:1},
};
let _uiVals={};
let _previewMode='cats';
function switchDTab(id,btn){
  ['themes','colors','layout','behavior','portex'].forEach(s=>{
    const el=document.getElementById('dsec-'+s);
    if(el)el.hidden=(s!==id);
  });
  document.querySelectorAll('.dtab').forEach(b=>b.classList.remove('dtab-active'));
  if(btn)btn.classList.add('dtab-active');
  if(id==='portex')_refreshExportPreview();
}
function setPreviewMode(mode,btn){
  _previewMode=mode;
  document.querySelectorAll('[id^="pvbtn-"]').forEach(b=>b.classList.remove('dtab-active'));
  if(btn)btn.classList.add('dtab-active');
  updatePreview();
}
function _buildColorPickers(){
  const grid=document.getElementById('colorPickers');
  if(!grid||grid.children.length)return;
  grid.innerHTML=_colorFields.map(f=>
    `<label style="font-size:.8rem;color:var(--muted)">${f.label}
      <div style="display:flex;gap:.5rem;align-items:center;margin-top:.25rem">
        <input type="color" id="uc_${f.key}" style="width:40px;height:32px;border:none;cursor:pointer;background:none"
               oninput="_uiVals['${f.key}']=this.value;updatePreview()">
        <span id="ucv_${f.key}" style="font-size:.75rem;font-family:monospace"></span>
      </div></label>`
  ).join('');
}
function _setColor(key,hex){
  _uiVals[key]=hex;
  const el=document.getElementById('uc_'+key);
  const lbl=document.getElementById('ucv_'+key);
  if(el)el.value=hex;
  if(lbl)lbl.textContent=hex.toUpperCase();
}
function _gi(id){return document.getElementById(id);}
function _giv(id,def){const el=_gi(id);return el?el.value:def;}
function _siv(id,v){const el=_gi(id);if(el)el.value=v;}
function applyTheme(name){
  const t=_themes[name];if(!t)return;
  _buildColorPickers();
  Object.entries(t).forEach(([k,v])=>{if(typeof v==='string')_setColor(k,v);else _uiVals[k]=v;});
  _siv('uiBrightness',t.brightness||220);
  _gi('brightVal').textContent=t.brightness||220;
  _siv('uiWarnDays',t.warning_days||7);
  _siv('uiDangerDays',t.danger_days||3);
  _siv('uiTbtnH',t.tbtn_h||48);
  _siv('uiBtnRadius',t.btn_radius||10);
  _siv('uiTbtnMargin',t.tbtn_margin||8);
  _siv('uiListRadius',t.list_radius||6);
  _siv('uiCatCols',t.cat_cols||2);
  _siv('uiCatGap',t.cat_gap||6);
  _siv('uiListItemH',t.list_item_h||64);
  _siv('uiCatHdrH',t.cat_hdr_h||44);
  _siv('uiSubHdrH',t.sub_hdr_h||54);
  _siv('uiPowerSaveMin',t.power_save_min||10);
  _siv('uiDateFmt',t.date_format||0);
  _siv('uiShowClock',t.show_clock!==undefined?t.show_clock:1);
  const sok=_gi('uiSoundOk');const ser=_gi('uiSoundErr');
  if(sok)sok.checked=!!(t.sound_ok!==undefined?t.sound_ok:1);
  if(ser)ser.checked=!!(t.sound_err!==undefined?t.sound_err:1);
  updatePreview();
}
function _buildPreviewCats(v){
  const cols=parseInt(_giv('uiCatCols',2));
  const gap=parseInt(_giv('uiCatGap',6));
  const hdr=parseInt(_giv('uiCatHdrH',44));
  const btnH=parseInt(_giv('uiTbtnH',48));
  const btnM=parseInt(_giv('uiTbtnMargin',8));
  const btnR=parseInt(_giv('uiBtnRadius',10));
  const showClk=_giv('uiShowClock',1)!=='0';
  const rows=Math.ceil(8/cols);
  const tw=Math.floor((280-(cols+1)*gap)/cols);
  const th=Math.floor((456-hdr-(rows+1)*gap)/rows);
  const tiles=[
    {n:'Fleisch',c:'#CC4422'},{n:'Gefluegel',c:'#CC8800'},
    {n:'Fisch',c:'#2255CC'},{n:'Gemüse',c:'#228822'},
    {n:'Obst',c:'#CC6600'},{n:'Fertig',c:'#7733BB'},
    {n:'Backwaren',c:'#998800'},{n:'Sonstiges',c:'#556677'},
  ];
  let s='';
  tiles.forEach((t,i)=>{
    const col=i%cols,row=Math.floor(i/cols);
    const tx=gap+col*(tw+gap),ty=hdr+gap+row*(th+gap);
    s+=`<rect x="${tx}" y="${ty}" width="${tw}" height="${th}" rx="${btnR}" fill="${t.c}"/>
    <text x="${tx+tw/2}" y="${ty+th/2+7}" text-anchor="middle" font-size="${t.n.length>8?15:20}" fill="white" font-family="sans-serif" font-weight="bold">${t.n}</text>`;
  });
  const clkSvg=showClk?`<text x="6" y="14" font-size="11" fill="${v.subtext||'#808080'}" font-family="monospace">12:34</text>`:'';
  return `<rect width="280" height="456" fill="${v.bg||'#000'}"/>
    <rect width="280" height="${hdr}" fill="${v.header||'#0C1213'}"/>
    <text x="140" y="${hdr/2+7}" text-anchor="middle" fill="${v.text||'#FFF'}" font-size="16" font-family="sans-serif" font-weight="600">Kategorien</text>
    <circle cx="272" cy="8" r="4" fill="${v.ok||'#00FF00'}"/>
    ${clkSvg}${s}`;
}
function _buildPreviewList(v){
  const sub=parseInt(_giv('uiSubHdrH',54));
  const itemH=parseInt(_giv('uiListItemH',64));
  const lr=parseInt(_giv('uiListRadius',6));
  const btnH=parseInt(_giv('uiTbtnH',48));
  const btnM=parseInt(_giv('uiTbtnMargin',8));
  const btnR=parseInt(_giv('uiBtnRadius',10));
  const showClk=_giv('uiShowClock',1)!=='0';
  const items=['Milch 1L','Butter 250g','Joghurt','Käse','Sahne'];
  const days=[14,3,1,-1,7];
  const dayCol=(d)=>d<0?(v.danger||'#FF0000'):d<=(parseInt(_giv('uiDangerDays',3)))?(v.danger||'#FF0000'):d<=(parseInt(_giv('uiWarnDays',7)))?(v.warn||'#FFAA00'):(v.ok||'#00FF00');
  const vis=Math.min(items.length,Math.floor((456-sub-btnH-8)/itemH));
  let s='';
  for(let i=0;i<vis;i++){
    const iy=sub+i*itemH;
    s+=`<rect x="4" y="${iy+2}" width="272" height="${itemH-4}" rx="${lr}" fill="${v.surface||'#181818'}"/>
    <text x="14" y="${iy+itemH/2+5}" font-size="14" fill="${v.text||'#FFF'}" font-family="sans-serif">${items[i]}</text>
    <text x="258" y="${iy+itemH/2+5}" text-anchor="end" font-size="12" fill="${dayCol(days[i])}" font-family="sans-serif">${days[i]<0?'abgel.':days[i]+'d'}</text>`;
  }
  const clkSvg=showClk?`<text x="6" y="14" font-size="11" fill="${v.subtext||'#808080'}" font-family="monospace">12:34</text>`:'';
  return `<rect width="280" height="456" fill="${v.bg||'#000'}"/>
    <rect width="280" height="${sub}" fill="${v.bg||'#000'}"/>
    <line x1="0" y1="${sub}" x2="280" y2="${sub}" stroke="${v.surface||'#333'}" stroke-width="1"/>
    <text x="50" y="${sub/2+7}" font-size="13" fill="${v.text||'#FFF'}" font-family="sans-serif">&lt; Zurück</text>
    <text x="140" y="${sub/2+7}" text-anchor="middle" font-size="16" fill="${v.accent||'#00FFFF'}" font-family="sans-serif" font-weight="600">Milchprodukte</text>
    <circle cx="272" cy="8" r="4" fill="${v.ok||'#00FF00'}"/>
    ${clkSvg}${s}
    <rect x="${btnM}" y="${456-btnH-8}" width="${280-2*btnM}" height="${btnH}" rx="${btnR}" fill="${v.btn_back||'#1A2250'}"/>
    <text x="140" y="${456-btnH/2-8+7}" text-anchor="middle" font-size="15" fill="${v.text||'#FFF'}" font-family="sans-serif">← Zurück</text>`;
}
function _buildPreviewDate(v){
  const sub=parseInt(_giv('uiSubHdrH',54));
  const btnH=parseInt(_giv('uiTbtnH',48));
  const btnM=parseInt(_giv('uiTbtnMargin',8));
  const btnR=parseInt(_giv('uiBtnRadius',10));
  const dfmt=parseInt(_giv('uiDateFmt',0));
  const dateStr=dfmt===1?'12/31/2025':dfmt===2?'2025-12-31':'31.12.2025';
  const dateH=72;const npTop=sub+dateH;const npColW=Math.floor(280/3);
  const npRowH=Math.floor((456-npTop)/4);
  const keys=['1','2','3','4','5','6','7','8','9','⌫','0','✓'];
  const kbg=(k)=>k==='✓'?(v.btn_ok||'#003000'):k==='⌫'?(v.btn_back||'#1A2250'):(v.surface||'#181818');
  let s='';
  keys.forEach((k,i)=>{
    const col=i%3,row=Math.floor(i/3);
    const kx=col*npColW+2,ky=npTop+row*npRowH+2,kw=npColW-4,kh=npRowH-4;
    s+=`<rect x="${kx}" y="${ky}" width="${kw}" height="${kh}" rx="${btnR}" fill="${kbg(k)}"/>
    <text x="${kx+kw/2}" y="${ky+kh/2+7}" text-anchor="middle" font-size="20" fill="${v.text||'#FFF'}" font-family="sans-serif" font-weight="600">${k}</text>`;
  });
  return `<rect width="280" height="456" fill="${v.bg||'#000'}"/>
    <rect width="280" height="${sub}" fill="${v.bg||'#000'}"/>
    <text x="140" y="${sub/2+7}" text-anchor="middle" font-size="16" fill="${v.text||'#FFF'}" font-family="sans-serif" font-weight="600">Datum eingeben</text>
    <rect width="280" height="${dateH}" y="${sub}" fill="${v.surface||'#181818'}"/>
    <text x="140" y="${sub+dateH/2-8}" text-anchor="middle" font-size="13" fill="${v.subtext||'#808080'}" font-family="sans-serif">Joghurt Natur</text>
    <text x="140" y="${sub+dateH/2+14}" text-anchor="middle" font-size="22" fill="${v.accent||'#00FFFF'}" font-family="monospace" font-weight="bold">${dateStr}</text>
    ${s}`;
}
function updatePreview(){
  const v=_uiVals;
  const svg=document.getElementById('devicePreview');
  if(!svg)return;
  let inner='';
  if(_previewMode==='list')inner=_buildPreviewList(v);
  else if(_previewMode==='date')inner=_buildPreviewDate(v);
  else inner=_buildPreviewCats(v);
  svg.innerHTML=inner;
}
function _refreshExportPreview(){
  const d=_collectDesignData();
  const ta=_gi('themeJsonPreview');
  if(ta)ta.value=JSON.stringify(d,null,2);
}
function _collectDesignData(){
  const body=Object.fromEntries(_colorFields.map(f=>[f.key,_uiVals[f.key]||'#000000']));
  body.brightness     =parseInt(_giv('uiBrightness',220));
  body.tbtn_h         =parseInt(_giv('uiTbtnH',48));
  body.btn_radius     =parseInt(_giv('uiBtnRadius',10));
  body.tbtn_margin    =parseInt(_giv('uiTbtnMargin',8));
  body.list_radius    =parseInt(_giv('uiListRadius',6));
  body.cat_cols       =parseInt(_giv('uiCatCols',2));
  body.cat_gap        =parseInt(_giv('uiCatGap',6));
  body.list_item_h    =parseInt(_giv('uiListItemH',64));
  body.cat_hdr_h      =parseInt(_giv('uiCatHdrH',44));
  body.sub_hdr_h      =parseInt(_giv('uiSubHdrH',54));
  body.warning_days   =parseInt(_giv('uiWarnDays',7));
  body.danger_days    =parseInt(_giv('uiDangerDays',3));
  body.power_save_min =parseInt(_giv('uiPowerSaveMin',10));
  body.date_format    =parseInt(_giv('uiDateFmt',0));
  body.show_clock     =parseInt(_giv('uiShowClock',1));
  const sok=_gi('uiSoundOk'),ser=_gi('uiSoundErr');
  body.sound_ok  =sok&&sok.checked?1:0;
  body.sound_err =ser&&ser.checked?1:0;
  return body;
}
function exportThemeJSON(){
  const d=_collectDesignData();
  const blob=new Blob([JSON.stringify(d,null,2)],{type:'application/json'});
  const a=document.createElement('a');
  a.href=URL.createObjectURL(blob);
  a.download='scanner-theme.json';
  a.click();
  _refreshExportPreview();
}
function importThemeJSON(inp){
  const file=inp.files[0];if(!file)return;
  const reader=new FileReader();
  reader.onload=e=>{
    try{
      const d=JSON.parse(e.target.result);
      _buildColorPickers();
      _colorFields.forEach(f=>{if(d[f.key])_setColor(f.key,d[f.key]);});
      _siv('uiBrightness',d.brightness||220);
      if(_gi('brightVal'))_gi('brightVal').textContent=d.brightness||220;
      _siv('uiTbtnH',d.tbtn_h||48);
      _siv('uiBtnRadius',d.btn_radius||10);
      _siv('uiTbtnMargin',d.tbtn_margin||8);
      _siv('uiListRadius',d.list_radius||6);
      _siv('uiCatCols',d.cat_cols||2);
      _siv('uiCatGap',d.cat_gap||6);
      _siv('uiListItemH',d.list_item_h||64);
      _siv('uiCatHdrH',d.cat_hdr_h||44);
      _siv('uiSubHdrH',d.sub_hdr_h||54);
      _siv('uiWarnDays',d.warning_days||7);
      _siv('uiDangerDays',d.danger_days||3);
      _siv('uiPowerSaveMin',d.power_save_min||10);
      _siv('uiDateFmt',d.date_format||0);
      _siv('uiShowClock',d.show_clock!==undefined?d.show_clock:1);
      const sok=_gi('uiSoundOk'),ser=_gi('uiSoundErr');
      if(sok)sok.checked=!!(d.sound_ok!==undefined?d.sound_ok:1);
      if(ser)ser.checked=!!(d.sound_err!==undefined?d.sound_err:1);
      updatePreview();_refreshExportPreview();
      document.getElementById('designMsg').textContent='Theme importiert – jetzt Speichern klicken.';
    }catch(ex){document.getElementById('designMsg').textContent='Import-Fehler: '+ex.message;}
  };
  reader.readAsText(file);
  inp.value='';
}
async function loadDesign(){
  _buildColorPickers();
  try{
    const d=await fetch('/api/ui-config').then(r=>r.json());
    if(d.error){document.getElementById('designMsg').textContent='Fehler: '+d.error;return;}
    _colorFields.forEach(f=>_setColor(f.key,d[f.key]||'#000000'));
    _siv('uiTbtnH',d.tbtn_h||48);
    _siv('uiBtnRadius',d.btn_radius||10);
    _siv('uiTbtnMargin',d.tbtn_margin||8);
    _siv('uiListRadius',d.list_radius||6);
    _siv('uiCatCols',d.cat_cols||2);
    _siv('uiCatGap',d.cat_gap||6);
    _siv('uiListItemH',d.list_item_h||64);
    _siv('uiCatHdrH',d.cat_hdr_h||44);
    _siv('uiSubHdrH',d.sub_hdr_h||54);
    _siv('uiBrightness',d.brightness||220);
    if(_gi('brightVal'))_gi('brightVal').textContent=d.brightness||220;
    _siv('uiWarnDays',d.warning_days||7);
    _siv('uiDangerDays',d.danger_days||3);
    _siv('uiPowerSaveMin',d.power_save_min||10);
    _siv('uiDateFmt',d.date_format||0);
    _siv('uiShowClock',d.show_clock!==undefined?d.show_clock:1);
    const sok=_gi('uiSoundOk'),ser=_gi('uiSoundErr');
    if(sok)sok.checked=!!(d.sound_ok!==undefined?d.sound_ok:1);
    if(ser)ser.checked=!!(d.sound_err!==undefined?d.sound_err:1);
    updatePreview();
  }catch(e){document.getElementById('designMsg').textContent='Ladefehler: '+e.message;}
}
async function saveDesign(){
  const msg=document.getElementById('designMsg');
  msg.style.color='var(--muted)';msg.textContent='Speichern…';
  const body=_collectDesignData();
  try{
    const r=await fetch('/api/ui-config',{method:'POST',
      headers:{'Content-Type':'application/json'},body:JSON.stringify(body)});
    const d=await r.json();
    if(d.ok){msg.style.color='var(--ok)';msg.textContent='✓ Gespeichert! Gerät aktualisiert.';}
    else{msg.style.color='var(--danger)';msg.textContent='Fehler: '+(d.error||'?');}
  }catch(e){msg.style.color='var(--danger)';msg.textContent='Netzwerkfehler: '+e.message;}
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
        int    hid  = p.getInt("household", 0);
        p.end();
        Preferences p2; p2.begin("dev", true);
        bool numpad = p2.getBool("numpad", false);
        p2.end();
        JsonDocument doc;
        doc["name"]        = name;
        doc["room"]        = room;
        doc["serverUrl"]   = url;
        doc["householdId"] = hid;
        doc["mac"]         = WiFi.macAddress();
        doc["ip"]          = WiFi.localIP().toString();
        doc["dateInput"]   = numpad ? "numpad" : "drum";
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
            p.putString("name",      doc["name"]      | "Lebensmittel Scanner");
            p.putString("room",      doc["room"]       | "");
            p.putString("url",       doc["serverUrl"]  | "");
            p.putInt("household",    doc["householdId"] | 0);
            p.end();
            bool numpad = (String(doc["dateInput"] | "drum") == "numpad");
            Preferences p2; p2.begin("dev", false);
            p2.putBool("numpad", numpad);
            p2.end();
            g_useNumpad = numpad;
            req->send(200, "application/json", "{\"ok\":true}");
        }
    );

    // ── Scanner-Konfiguration ────────────────────────────────
    _server.on("/api/scanner-config", HTTP_GET, [](AsyncWebServerRequest *req) {
        Preferences p; p.begin("scanner", true);
        bool   bt     = p.getBool("bt",       false);
        String btName = p.getString("btname", "");
        p.end();
        JsonDocument doc;
        doc["bt"]     = bt;
        doc["btName"] = btName;
        String out; serializeJson(doc, out);
        req->send(200, "application/json", out);
    });
    _server.on("/api/scanner-config", HTTP_POST,
        [](AsyncWebServerRequest *req) {},
        nullptr,
        [](AsyncWebServerRequest *req, uint8_t *data, size_t len, size_t index, size_t total) {
            JsonDocument doc;
            if (deserializeJson(doc, (const char*)data, len)) {
                req->send(400, "application/json", "{\"error\":\"JSON\"}"); return;
            }
            bool   bt     = doc["bt"]     | false;
            String btName = doc["btName"] | String("");
            BarcodeScanner::saveScannerConfig(bt, btName);
            req->send(200, "application/json", "{\"ok\":true}");
            // Neustart nach kurzer Verzögerung
            xTaskCreate([](void*){ vTaskDelay(pdMS_TO_TICKS(500)); ESP.restart(); vTaskDelete(nullptr); },
                        "rst", 2048, nullptr, 1, nullptr);
        }
    );

    // ── Haushalte vom Server laden ───────────────────────────
    _server.on("/api/household-options", HTTP_GET, [](AsyncWebServerRequest *req) {
        Preferences p; p.begin("sync", true);
        String url = p.getString("url", "");
        p.end();
        if (url.isEmpty()) {
            req->send(200, "application/json", "[]"); return;
        }
        HTTPClient http;
        http.setTimeout(4000);
        if (!http.begin(url + "/api/households")) {
            req->send(503, "application/json", "[]"); return;
        }
        int code = http.GET();
        if (code != 200) { http.end(); req->send(200, "application/json", "[]"); return; }
        String body = http.getString();
        http.end();
        req->send(200, "application/json", body);
    });

    // ── Schriftgrößen ────────────────────────────────────────
    _server.on("/api/font-config", HTTP_GET, [](AsyncWebServerRequest *req) {
        JsonDocument doc;
        doc["title"] = g_fontCfg.title;
        doc["body"]  = g_fontCfg.body;
        doc["small"] = g_fontCfg.small;
        doc["value"] = g_fontCfg.value;
        doc["key"]   = g_fontCfg.key;
        doc["btn"]   = g_fontCfg.btn;
        String out; serializeJson(doc, out);
        req->send(200, "application/json", out);
    });
    _server.on("/api/font-config", HTTP_POST,
        [](AsyncWebServerRequest *req) {},
        nullptr,
        [](AsyncWebServerRequest *req, uint8_t *data, size_t len, size_t index, size_t total) {
            JsonDocument doc;
            if (deserializeJson(doc, (const char*)data, len)) {
                req->send(400, "application/json", "{\"error\":\"JSON\"}"); return;
            }
            auto clamp = [](int v) -> uint8_t { return v < 8 ? 8 : v > 32 ? 32 : (uint8_t)v; };
            g_fontCfg.title = clamp(doc["title"] | 16);
            g_fontCfg.body  = clamp(doc["body"]  | 16);
            g_fontCfg.small = clamp(doc["small"] |  8);
            g_fontCfg.value = clamp(doc["value"] | 24);
            g_fontCfg.key   = clamp(doc["key"]   | 24);
            g_fontCfg.btn   = clamp(doc["btn"]   | 16);
            saveFontConfig(g_fontCfg);
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
            obj["description"] = p.description;
            obj["defaultDays"] = p.defaultDays;
            obj["askQty"]      = p.askQty;
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
            p.description = doc["description"].as<String>();
            p.defaultDays = doc["defaultDays"] | 0;
            p.askQty      = doc["askQty"] | false;
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

    // ── GitHub OTA ───────────────────────────────────────────
    _server.on("/api/github-release", HTTP_GET, [](AsyncWebServerRequest *req) {
        WiFiClientSecure client; client.setInsecure();
        HTTPClient https;
        String apiUrl = "https://api.github.com/repos/zendonir/esp-lebensmittel-scan/releases?per_page=10";
        if (!https.begin(client, apiUrl)) {
            req->send(503, "application/json", "{\"error\":\"begin failed\"}"); return;
        }
        https.addHeader("User-Agent", "ESP32-Lebensmittel");
        https.addHeader("Accept",     "application/vnd.github+json");
        int code = https.GET();
        if (code != 200) {
            String err = "{\"error\":\"HTTP " + String(code) + "\"}";
            https.end(); req->send(200, "application/json", err); return;
        }

        // Filter: nur tag_name und assets-Felder behalten → spart RAM
        JsonDocument filter;
        JsonArray fArr = filter.to<JsonArray>();
        JsonObject fObj = fArr.add<JsonObject>();
        fObj["tag_name"] = true;
        JsonArray fAssets = fObj["assets"].to<JsonArray>();
        JsonObject fAsset = fAssets.add<JsonObject>();
        fAsset["name"]                 = true;
        fAsset["browser_download_url"] = true;
        fAsset["size"]                 = true;

        JsonDocument doc;
        DeserializationError err2 = deserializeJson(doc, *https.getStreamPtr(),
                                                    DeserializationOption::Filter(filter));
        https.end();
        if (err2) {
            String e = "{\"error\":\"JSON: "; e += err2.c_str(); e += "\"}";
            req->send(200, "application/json", e); return;
        }

        JsonDocument out;
        JsonArray arr = out.to<JsonArray>();
        for (JsonObject rel : doc.as<JsonArray>()) {
            String tag = rel["tag_name"] | String("");
            if (tag.isEmpty()) continue;
            String url, sizeStr;
            for (JsonObject a : rel["assets"].as<JsonArray>()) {
                String name = a["name"] | String("");
                if (name.endsWith(".bin")) {
                    url     = a["browser_download_url"] | String("");
                    int sz  = a["size"] | 0;
                    sizeStr = String(sz / 1024) + " KB";
                    break;
                }
            }
            if (url.isEmpty()) continue;
            JsonObject entry = arr.add<JsonObject>();
            entry["tag"]  = tag;
            entry["url"]  = url;
            entry["size"] = sizeStr;
        }
        String s; serializeJson(out, s);
        req->send(200, "application/json", s);
    });

    _server.on("/api/github-ota", HTTP_POST,
        [](AsyncWebServerRequest *req) {},
        nullptr,
        [](AsyncWebServerRequest *req, uint8_t *data, size_t len, size_t index, size_t total) {
            JsonDocument doc;
            if (deserializeJson(doc, (const char*)data, len)) {
                req->send(400, "application/json", "{\"error\":\"JSON\"}"); return;
            }
            String url = doc["url"] | String("");
            if (url.isEmpty()) {
                req->send(400, "application/json", "{\"error\":\"no url\"}"); return;
            }
            // Run OTA in a background task to allow response to be sent first
            req->send(200, "application/json", "{\"ok\":true}");

            // Delay slightly then start OTA
            static String _otaUrl;
            _otaUrl = url;
            xTaskCreate([](void *) {
                vTaskDelay(pdMS_TO_TICKS(200));
                WiFiClientSecure client; client.setInsecure();
                HTTPClient https;
                // Follow redirect: GitHub releases redirect to S3
                https.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
                if (!https.begin(client, _otaUrl)) { vTaskDelete(nullptr); return; }
                https.addHeader("User-Agent", "ESP32-Lebensmittel");
                int code = https.GET();
                if (code != 200) { https.end(); vTaskDelete(nullptr); return; }
                int sz = https.getSize();
                if (!Update.begin(sz > 0 ? sz : UPDATE_SIZE_UNKNOWN)) {
                    https.end(); vTaskDelete(nullptr); return;
                }
                WiFiClient *stream = https.getStreamPtr();
                Update.writeStream(*stream);
                https.end();
                if (Update.end(true)) ESP.restart();
                vTaskDelete(nullptr);
            }, "ghota", 8192, nullptr, 1, nullptr);
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

    // Scan logs endpoint
    // ── UI-Design-Config ─────────────────────────────────────────
    _server.on("/api/ui-config", HTTP_GET, [](AsyncWebServerRequest *req) {
        JsonDocument doc;
        doc["bg"]          = rgb565ToHex(g_uiCfg.bg);
        doc["text"]        = rgb565ToHex(g_uiCfg.text);
        doc["subtext"]     = rgb565ToHex(g_uiCfg.subtext);
        doc["ok"]          = rgb565ToHex(g_uiCfg.ok);
        doc["warn"]        = rgb565ToHex(g_uiCfg.warn);
        doc["danger"]      = rgb565ToHex(g_uiCfg.danger);
        doc["accent"]      = rgb565ToHex(g_uiCfg.accent);
        doc["header"]      = rgb565ToHex(g_uiCfg.header);
        doc["surface"]     = rgb565ToHex(g_uiCfg.surface);
        doc["btn_ok"]      = rgb565ToHex(g_uiCfg.btn_ok);
        doc["btn_back"]    = rgb565ToHex(g_uiCfg.btn_back);
        doc["brightness"]  = g_uiCfg.brightness;
        doc["warning_days"]= g_uiCfg.warning_days;
        doc["danger_days"] = g_uiCfg.danger_days;
        doc["tbtn_h"]        = g_uiCfg.tbtn_h;
        doc["btn_radius"]    = g_uiCfg.btn_radius;
        doc["tbtn_margin"]   = g_uiCfg.tbtn_margin;
        doc["list_radius"]   = g_uiCfg.list_radius;
        doc["cat_cols"]      = g_uiCfg.cat_cols;
        doc["cat_gap"]       = g_uiCfg.cat_gap;
        doc["list_item_h"]   = g_uiCfg.list_item_h;
        doc["cat_hdr_h"]     = g_uiCfg.cat_hdr_h;
        doc["sub_hdr_h"]     = g_uiCfg.sub_hdr_h;
        doc["power_save_min"]= g_uiCfg.power_save_min;
        doc["date_format"]   = g_uiCfg.date_format;
        doc["show_clock"]    = g_uiCfg.show_clock;
        doc["sound_ok"]      = g_uiCfg.sound_ok;
        doc["sound_err"]     = g_uiCfg.sound_err;
        String s; serializeJson(doc, s);
        req->send(200, "application/json", s);
    });

    _server.on("/api/ui-config", HTTP_POST,
        [](AsyncWebServerRequest *req) {},
        nullptr,
        [](AsyncWebServerRequest *req, uint8_t *data, size_t len, size_t, size_t) {
            JsonDocument doc;
            if (deserializeJson(doc, (const char*)data, len)) {
                req->send(400, "application/json", "{\"error\":\"JSON\"}"); return;
            }
            if (doc["bg"].is<String>())      g_uiCfg.bg      = hexToRGB565(doc["bg"].as<String>());
            if (doc["text"].is<String>())    g_uiCfg.text    = hexToRGB565(doc["text"].as<String>());
            if (doc["subtext"].is<String>()) g_uiCfg.subtext = hexToRGB565(doc["subtext"].as<String>());
            if (doc["ok"].is<String>())      g_uiCfg.ok      = hexToRGB565(doc["ok"].as<String>());
            if (doc["warn"].is<String>())    g_uiCfg.warn    = hexToRGB565(doc["warn"].as<String>());
            if (doc["danger"].is<String>())  g_uiCfg.danger  = hexToRGB565(doc["danger"].as<String>());
            if (doc["accent"].is<String>())  g_uiCfg.accent  = hexToRGB565(doc["accent"].as<String>());
            if (doc["header"].is<String>())  g_uiCfg.header  = hexToRGB565(doc["header"].as<String>());
            if (doc["surface"].is<String>()) g_uiCfg.surface = hexToRGB565(doc["surface"].as<String>());
            if (doc["btn_ok"].is<String>())  g_uiCfg.btn_ok  = hexToRGB565(doc["btn_ok"].as<String>());
            if (doc["btn_back"].is<String>())g_uiCfg.btn_back= hexToRGB565(doc["btn_back"].as<String>());
            if (!doc["brightness"].isNull()) g_uiCfg.brightness  = doc["brightness"].as<uint8_t>();
            if (!doc["warning_days"].isNull())g_uiCfg.warning_days=doc["warning_days"].as<uint8_t>();
            if (!doc["danger_days"].isNull()) g_uiCfg.danger_days =doc["danger_days"].as<uint8_t>();
            if (!doc["tbtn_h"].isNull())       g_uiCfg.tbtn_h       = doc["tbtn_h"].as<uint8_t>();
            if (!doc["btn_radius"].isNull())   g_uiCfg.btn_radius   = doc["btn_radius"].as<uint8_t>();
            if (!doc["tbtn_margin"].isNull())  g_uiCfg.tbtn_margin  = doc["tbtn_margin"].as<uint8_t>();
            if (!doc["list_radius"].isNull())  g_uiCfg.list_radius  = doc["list_radius"].as<uint8_t>();
            if (!doc["cat_cols"].isNull())     g_uiCfg.cat_cols     = doc["cat_cols"].as<uint8_t>();
            if (!doc["cat_gap"].isNull())      g_uiCfg.cat_gap      = doc["cat_gap"].as<uint8_t>();
            if (!doc["list_item_h"].isNull())  g_uiCfg.list_item_h  = doc["list_item_h"].as<uint8_t>();
            if (!doc["cat_hdr_h"].isNull())    g_uiCfg.cat_hdr_h    = doc["cat_hdr_h"].as<uint8_t>();
            if (!doc["sub_hdr_h"].isNull())    g_uiCfg.sub_hdr_h    = doc["sub_hdr_h"].as<uint8_t>();
            if (!doc["power_save_min"].isNull())g_uiCfg.power_save_min=doc["power_save_min"].as<uint8_t>();
            if (!doc["date_format"].isNull())  g_uiCfg.date_format  = doc["date_format"].as<uint8_t>();
            if (!doc["show_clock"].isNull())   g_uiCfg.show_clock   = doc["show_clock"].as<uint8_t>();
            if (!doc["sound_ok"].isNull())     g_uiCfg.sound_ok     = doc["sound_ok"].as<uint8_t>();
            if (!doc["sound_err"].isNull())    g_uiCfg.sound_err    = doc["sound_err"].as<uint8_t>();
            saveUIConfig();
            extern void applyUIBrightness();
            applyUIBrightness();
            req->send(200, "application/json", "{\"ok\":true}");
        }
    );

    _server.on("/api/scanlogs", HTTP_GET, [](AsyncWebServerRequest *req) {
        extern String getScanLogsJSON();
        req->send(200, "application/json", getScanLogsJSON());
    });

    _server.on("/api/buzzer-test", HTTP_POST, [](AsyncWebServerRequest *req) {
#if defined(BUZZER_PIN) && BUZZER_PIN >= 0
        tone(BUZZER_PIN, 2200, 80);
        delay(120);
        tone(BUZZER_PIN, 1800, 80);
#endif
        req->send(200, "application/json", "{\"ok\":true}");
    });

    _server.onNotFound([](AsyncWebServerRequest *req) { req->send(404,"text/plain","Not found"); });

    // OTA-Update unter /ota (Basic-Auth: admin / NVS-Passwort)
    {
        Preferences p; p.begin("ota", true);
        String pw = p.getString("password", "lebensmittel");
        p.end();

        _server.on("/ota", HTTP_GET, [pw](AsyncWebServerRequest *req) {
            if (!req->authenticate("admin", pw.c_str()))
                return req->requestAuthentication();
            req->send(200, "text/html", OTA_PAGE_HTML);
        });

        _server.on("/ota", HTTP_POST,
            [pw](AsyncWebServerRequest *req) {
                if (!req->authenticate("admin", pw.c_str()))
                    return req->requestAuthentication();
                bool ok = !Update.hasError();
                String body = ok
                    ? String(F("<html><body style='font-family:system-ui;background:#111827;color:#10b981;padding:3rem'>"
                               "<h2>&#x2713; Update erfolgreich! Neustart in 5&nbsp;s&hellip;</h2>"
                               "<script>setTimeout(()=>location.href='/',6000)</script></body></html>"))
                    : (String(F("<html><body style='font-family:system-ui;background:#111827;color:#ef4444;padding:3rem'>"
                                "<h2>&#x26A0; Fehler: ")) + Update.errorString() + F("</h2></body></html>"));
                auto *resp = req->beginResponse(200, "text/html", body);
                resp->addHeader("Connection", "close");
                req->send(resp);
                if (ok) { delay(300); ESP.restart(); }
            },
            [](AsyncWebServerRequest*, String filename, size_t index,
               uint8_t *data, size_t len, bool final) {
                if (!index) {
                    Serial.printf("[OTA] Start: %s\n", filename.c_str());
                    Update.begin(UPDATE_SIZE_UNKNOWN);
                }
                Update.write(data, len);
                if (final) {
                    if (Update.end(true))
                        Serial.printf("[OTA] OK (%u Bytes)\n", index + len);
                    else
                        Update.printError(Serial);
                }
            }
        );
    }

    _server.begin();
    Serial.println("[Web] Server gestartet auf Port 80");
}

void WebInterface::loop() {
}
