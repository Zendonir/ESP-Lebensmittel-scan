"""
Lebensmittel-Scanner Zentral-Server
Empfängt Ereignisse von mehreren ESP32-Geräten und stellt ein
zentrales Dashboard bereit.

Start: docker compose up --build
       oder: uvicorn main:app --host 0.0.0.0 --port 8000 --reload
"""

import os
import sqlite3
import time
from contextlib import asynccontextmanager
from datetime import date, datetime, timedelta
from typing import Any

from fastapi import FastAPI, HTTPException, Request
from fastapi.middleware.cors import CORSMiddleware
from fastapi.responses import HTMLResponse, JSONResponse
from pydantic import BaseModel

# ── Konfiguration ─────────────────────────────────────────────

DB_PATH = os.environ.get("DB_PATH", "lager.db")


# ── Datenbank ─────────────────────────────────────────────────

def get_db() -> sqlite3.Connection:
    conn = sqlite3.connect(DB_PATH)
    conn.row_factory = sqlite3.Row
    conn.execute("PRAGMA journal_mode=WAL")
    return conn


def init_db():
    with get_db() as db:
        db.executescript("""
        CREATE TABLE IF NOT EXISTS devices (
            device_id   TEXT PRIMARY KEY,
            name        TEXT,
            room        TEXT,
            ip          TEXT,
            last_seen   INTEGER,
            registered  INTEGER
        );
        CREATE TABLE IF NOT EXISTS inventory (
            id          INTEGER PRIMARY KEY AUTOINCREMENT,
            device_id   TEXT,
            name        TEXT,
            brand       TEXT,
            category    TEXT,
            expiry      TEXT,
            added       TEXT,
            qty         INTEGER DEFAULT 1,
            label       TEXT UNIQUE,
            removed     INTEGER DEFAULT 0,
            removed_at  TEXT
        );
        CREATE TABLE IF NOT EXISTS storage_stats (
            id          INTEGER PRIMARY KEY AUTOINCREMENT,
            device_id   TEXT,
            name        TEXT,
            category    TEXT,
            added_date  TEXT,
            removed_date TEXT,
            storage_days INTEGER,
            recorded_at  INTEGER
        );
        CREATE TABLE IF NOT EXISTS categories (
            name        TEXT PRIMARY KEY,
            bg_color    INTEGER,
            fg_color    INTEGER,
            sort_order  INTEGER DEFAULT 0
        );
        CREATE TABLE IF NOT EXISTS products (
            id          INTEGER PRIMARY KEY AUTOINCREMENT,
            name        TEXT,
            brand       TEXT,
            barcode     TEXT,
            category    TEXT,
            default_days INTEGER DEFAULT 0
        );
        """)


# ── Lifespan ──────────────────────────────────────────────────

@asynccontextmanager
async def lifespan(app: FastAPI):
    init_db()
    yield


# ── App ───────────────────────────────────────────────────────

app = FastAPI(title="Lebensmittel-Server", lifespan=lifespan)
app.add_middleware(CORSMiddleware, allow_origins=["*"],
                   allow_methods=["*"], allow_headers=["*"])


# ── Pydantic Models ───────────────────────────────────────────

class DeviceReg(BaseModel):
    device_id:   str
    device_name: str = ""
    device_room: str = ""
    ip:          str = ""


class EventPayload(BaseModel):
    device_id:   str
    device_name: str = ""
    device_room: str = ""
    type:        str   # item_added | item_removed | heartbeat
    data:        dict[str, Any] = {}


class CategoryIn(BaseModel):
    name:      str
    bg_color:  int = 0
    fg_color:  int = 65535


class ProductIn(BaseModel):
    name:         str
    brand:        str = ""
    barcode:      str = ""
    category:     str = ""
    default_days: int = 0


# ── Helpers ───────────────────────────────────────────────────

def days_until(date_str: str) -> int:
    try:
        exp = datetime.strptime(date_str, "%Y-%m-%d").date()
        return (exp - date.today()).days
    except Exception:
        return 9999


def touch_device(db: sqlite3.Connection, device_id: str, name: str = "", room: str = "", ip: str = ""):
    db.execute(
        """INSERT INTO devices(device_id,name,room,ip,last_seen,registered)
           VALUES(?,?,?,?,?,?)
           ON CONFLICT(device_id) DO UPDATE SET
             last_seen=excluded.last_seen,
             name=CASE WHEN excluded.name!='' THEN excluded.name ELSE name END,
             room=CASE WHEN excluded.room!='' THEN excluded.room ELSE room END,
             ip=CASE WHEN excluded.ip!='' THEN excluded.ip ELSE ip END""",
        (device_id, name, room, ip, int(time.time()), int(time.time()))
    )


# ── API-Endpunkte ─────────────────────────────────────────────

@app.get("/api/status")
def status():
    with get_db() as db:
        total   = db.execute("SELECT COUNT(*) FROM inventory WHERE removed=0").fetchone()[0]
        devices = db.execute("SELECT COUNT(*) FROM devices").fetchone()[0]
        warn7   = sum(1 for r in db.execute(
            "SELECT expiry FROM inventory WHERE removed=0").fetchall()
            if 0 <= days_until(r["expiry"]) <= 7)
        expired = sum(1 for r in db.execute(
            "SELECT expiry FROM inventory WHERE removed=0").fetchall()
            if days_until(r["expiry"]) < 0)
    return {"total": total, "devices": devices, "expiring_7d": warn7, "expired": expired}


@app.post("/api/devices")
async def register_device(dev: DeviceReg):
    with get_db() as db:
        touch_device(db, dev.device_id, dev.device_name, dev.device_room, dev.ip)
    return {"ok": True}


@app.get("/api/devices")
def list_devices():
    with get_db() as db:
        rows = db.execute("SELECT * FROM devices ORDER BY last_seen DESC").fetchall()
    now = int(time.time())
    return [{"device_id": r["device_id"], "name": r["name"],
             "room": r["room"], "ip": r["ip"],
             "online": now - r["last_seen"] < 300,
             "last_seen": r["last_seen"]} for r in rows]


@app.post("/api/events")
async def receive_event(ev: EventPayload):
    with get_db() as db:
        touch_device(db, ev.device_id, ev.device_name, ev.device_room)

        if ev.type == "item_added":
            d = ev.data
            db.execute(
                """INSERT INTO inventory(device_id,name,brand,category,expiry,added,qty,label)
                   VALUES(?,?,?,?,?,?,?,?)
                   ON CONFLICT(label) DO UPDATE SET
                     qty=qty+excluded.qty, removed=0, removed_at=NULL""",
                (ev.device_id, d.get("name",""), d.get("brand",""),
                 d.get("category",""), d.get("expiry",""), d.get("added",""),
                 d.get("qty",1), d.get("label",""))
            )

        elif ev.type == "item_removed":
            d = ev.data
            today = date.today().isoformat()
            db.execute(
                "UPDATE inventory SET removed=1, removed_at=? WHERE label=? AND device_id=?",
                (today, d.get("label",""), ev.device_id)
            )
            if d.get("storageDays") is not None:
                db.execute(
                    """INSERT INTO storage_stats
                       (device_id,name,category,added_date,removed_date,storage_days,recorded_at)
                       VALUES(?,?,?,?,?,?,?)""",
                    (ev.device_id, d.get("name",""), d.get("category",""),
                     d.get("added",""), today, d.get("storageDays",0), int(time.time()))
                )

        elif ev.type == "heartbeat":
            pass  # Nur last_seen aktualisieren

    return {"ok": True}


@app.get("/api/inventory")
def get_inventory(device_id: str = "", category: str = "",
                  status: str = "active"):
    sql = "SELECT i.*, d.name as dev_name, d.room as dev_room FROM inventory i LEFT JOIN devices d ON i.device_id=d.device_id WHERE 1=1"
    params = []
    if status == "active":
        sql += " AND i.removed=0"
    elif status == "removed":
        sql += " AND i.removed=1"
    if device_id:
        sql += " AND i.device_id=?"
        params.append(device_id)
    if category:
        sql += " AND i.category=?"
        params.append(category)
    sql += " ORDER BY i.expiry ASC"
    with get_db() as db:
        rows = db.execute(sql, params).fetchall()
    return [{"id": r["id"], "device_id": r["device_id"],
             "device_name": r["dev_name"] or r["device_id"],
             "device_room": r["dev_room"] or "",
             "name": r["name"], "brand": r["brand"],
             "category": r["category"], "expiry": r["expiry"],
             "added": r["added"], "qty": r["qty"], "label": r["label"],
             "days_left": days_until(r["expiry"] or "")} for r in rows]


@app.delete("/api/inventory/{label}")
def remove_item(label: str, device_id: str = ""):
    with get_db() as db:
        db.execute("UPDATE inventory SET removed=1, removed_at=? WHERE label=?",
                   (date.today().isoformat(), label))
    return {"ok": True}


@app.get("/api/storage-stats")
def get_storage_stats(device_id: str = ""):
    sql = """SELECT s.*, d.name as dev_name FROM storage_stats s
             LEFT JOIN devices d ON s.device_id=d.device_id"""
    params = []
    if device_id:
        sql += " WHERE s.device_id=?"
        params.append(device_id)
    sql += " ORDER BY s.recorded_at DESC LIMIT 500"
    with get_db() as db:
        rows = db.execute(sql, params).fetchall()
    items = [{"device_id": r["device_id"],
              "device_name": r["dev_name"] or r["device_id"],
              "name": r["name"], "category": r["category"],
              "added_date": r["added_date"], "removed_date": r["removed_date"],
              "storage_days": r["storage_days"]} for r in rows]
    avg = round(sum(i["storage_days"] for i in items) / len(items)) if items else 0
    return {"stats": items, "count": len(items), "avg_days": avg}


@app.get("/api/categories")
def get_categories():
    with get_db() as db:
        rows = db.execute("SELECT * FROM categories ORDER BY sort_order").fetchall()
    return [{"name": r["name"], "bg": r["bg_color"], "fg": r["fg_color"]} for r in rows]


@app.post("/api/categories")
def set_categories(cats: list[CategoryIn]):
    with get_db() as db:
        db.execute("DELETE FROM categories")
        for i, c in enumerate(cats):
            db.execute("INSERT INTO categories VALUES(?,?,?,?)",
                       (c.name, c.bg_color, c.fg_color, i))
    return {"ok": True, "count": len(cats)}


@app.get("/api/products")
def get_products():
    with get_db() as db:
        rows = db.execute("SELECT * FROM products ORDER BY category, name").fetchall()
    return [{"id": r["id"], "name": r["name"], "brand": r["brand"],
             "barcode": r["barcode"], "category": r["category"],
             "default_days": r["default_days"]} for r in rows]


@app.post("/api/products")
def add_product(p: ProductIn):
    with get_db() as db:
        db.execute("INSERT INTO products(name,brand,barcode,category,default_days) VALUES(?,?,?,?,?)",
                   (p.name, p.brand, p.barcode, p.category, p.default_days))
    return {"ok": True}


@app.delete("/api/products/{pid}")
def delete_product(pid: int):
    with get_db() as db:
        db.execute("DELETE FROM products WHERE id=?", (pid,))
    return {"ok": True}


# ── Dashboard HTML ────────────────────────────────────────────

DASHBOARD_HTML = """<!DOCTYPE html>
<html lang="de">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1.0">
<title>Lebensmittel-Server</title>
<style>
  :root{--bg:#111827;--surface:#1f2937;--border:#374151;--text:#f9fafb;
        --muted:#9ca3af;--ok:#10b981;--warn:#f59e0b;--danger:#ef4444;--accent:#3b82f6;}
  *{box-sizing:border-box;margin:0;padding:0;}
  body{background:var(--bg);color:var(--text);font-family:system-ui,sans-serif;}
  nav{background:var(--surface);border-bottom:1px solid var(--border);
      display:flex;align-items:center;padding:0 1rem;}
  nav h1{font-size:1rem;padding:.9rem 1rem;color:var(--accent);}
  nav button{background:none;border:none;color:var(--muted);padding:.9rem 1.2rem;
             cursor:pointer;font-size:.9rem;border-bottom:2px solid transparent;}
  nav button.active{color:var(--text);border-bottom-color:var(--accent);}
  .panel{display:none;} .panel.active{display:block;}
  .stats{display:flex;gap:1rem;padding:1.25rem 1.5rem;flex-wrap:wrap;}
  .stat-card{background:var(--surface);border:1px solid var(--border);
             border-radius:.75rem;padding:1rem 1.5rem;min-width:140px;}
  .stat-card .label{font-size:.75rem;color:var(--muted);margin-bottom:.25rem;}
  .stat-card .value{font-size:2rem;font-weight:700;}
  .toolbar{padding:.75rem 1.5rem;display:flex;gap:.75rem;flex-wrap:wrap;align-items:center;}
  select,input[type=text],input[type=search]{background:var(--surface);border:1px solid var(--border);
    color:var(--text);padding:.5rem .75rem;border-radius:.5rem;font-size:.9rem;outline:none;}
  input[type=search]{flex:1;min-width:180px;}
  .btn{background:var(--accent);color:#fff;border:none;padding:.5rem 1rem;
       border-radius:.5rem;cursor:pointer;font-size:.9rem;}
  .btn-sm{padding:.25rem .6rem;font-size:.8rem;}
  .btn-danger{background:var(--danger);}
  .table-wrap{overflow-x:auto;padding:0 1.5rem 2rem;}
  table{width:100%;border-collapse:collapse;}
  thead{background:var(--surface);position:sticky;top:0;z-index:1;}
  th{padding:.7rem 1rem;text-align:left;font-size:.8rem;color:var(--muted);
     text-transform:uppercase;border-bottom:1px solid var(--border);}
  td{padding:.7rem 1rem;border-bottom:1px solid var(--border);font-size:.9rem;}
  tr:hover td{background:rgba(255,255,255,.02);}
  .pill{display:inline-block;padding:.15rem .5rem;border-radius:999px;font-size:.75rem;font-weight:600;}
  .pill-ok{background:#064e3b;color:var(--ok);}
  .pill-warn{background:#78350f;color:var(--warn);}
  .pill-danger{background:#7f1d1d;color:var(--danger);}
  .online{display:inline-block;width:8px;height:8px;border-radius:50%;
          background:var(--ok);margin-right:.4rem;}
  .offline{background:var(--muted);}
  .empty{text-align:center;color:var(--muted);padding:3rem;}
</style>
</head>
<body>
<nav>
  <h1>&#x1F4E6; Lager-Server</h1>
  <button class="active" onclick="tab('overview',this)">&#x1F3E0; &Uuml;bersicht</button>
  <button onclick="tab('devices',this)">&#x1F4F1; Ger&auml;te</button>
  <button onclick="tab('inventory',this)">&#x1F4E6; Inventar</button>
  <button onclick="tab('statistics',this)">&#x1F4CA; Statistik</button>
  <button onclick="tab('config',this)">&#x2699; Konfiguration</button>
</nav>

<div id="overview" class="panel active">
  <div class="stats" id="overviewCards">
    <div class="stat-card"><div class="label">Gesamt Artikel</div><div class="value" id="oTotal">-</div></div>
    <div class="stat-card"><div class="label">Ger&auml;te online</div><div class="value" id="oDevices">-</div></div>
    <div class="stat-card"><div class="label">Bald ablaufend</div>
      <div class="value" style="color:var(--warn)" id="oWarn">-</div></div>
    <div class="stat-card"><div class="label">Abgelaufen</div>
      <div class="value" style="color:var(--danger)" id="oExpired">-</div></div>
  </div>
  <div class="table-wrap">
    <h3 style="padding:0 0 .75rem;font-size:.95rem;color:var(--muted)">
      Bald ablaufende Artikel (alle Ger&auml;te)
    </h3>
    <table><thead><tr>
      <th>Ger&auml;t</th><th>Produkt</th><th>Kategorie</th><th>MHD</th><th>Verbleibend</th>
    </tr></thead>
    <tbody id="overviewWarnBody"></tbody></table>
    <div id="overviewWarnEmpty" class="empty" style="display:none">Keine ablaufenden Artikel. &#x1F389;</div>
  </div>
</div>

<div id="devices" class="panel">
  <div class="toolbar">
    <span style="color:var(--muted);font-size:.9rem">Ger&auml;te die sich beim Server registriert haben.</span>
    <button class="btn btn-sm" onclick="loadDevices()">&#x21BB;</button>
  </div>
  <div class="table-wrap">
    <table><thead><tr>
      <th>Status</th><th>Name</th><th>Raum</th><th>IP</th><th>Ger&auml;te-ID</th><th>Zuletzt gesehen</th>
    </tr></thead>
    <tbody id="devBody"></tbody></table>
    <div id="devEmpty" class="empty" style="display:none">Noch keine Ger&auml;te registriert.</div>
  </div>
</div>

<div id="inventory" class="panel">
  <div class="toolbar">
    <input type="search" id="invSearch" placeholder="Suchen&hellip;" oninput="renderInv()">
    <select id="invDevice" onchange="renderInv()"><option value="">Alle Ger&auml;te</option></select>
    <select id="invStatus" onchange="loadInv()">
      <option value="active">Eingelagert</option>
      <option value="removed">Ausgelagert</option>
      <option value="">Alle</option>
    </select>
    <button class="btn btn-sm" onclick="loadInv()">&#x21BB;</button>
    <button class="btn btn-sm" onclick="exportInvCSV()">CSV</button>
  </div>
  <div class="table-wrap">
    <table><thead><tr>
      <th>Ger&auml;t</th><th>Produkt</th><th>Kategorie</th>
      <th>MHD</th><th>Verbleibend</th><th>Menge</th><th>Label</th>
    </tr></thead>
    <tbody id="invBody"></tbody></table>
    <div id="invEmpty" class="empty" style="display:none">Keine Eintr&auml;ge.</div>
  </div>
</div>

<div id="statistics" class="panel">
  <div class="stats">
    <div class="stat-card"><div class="label">Auslagerungen</div>
      <div class="value" id="sTotal">-</div></div>
    <div class="stat-card"><div class="label">&Oslash; Lagerdauer</div>
      <div class="value" id="sAvg">-</div></div>
  </div>
  <div class="toolbar">
    <select id="statsDevice" onchange="loadStats()"><option value="">Alle Ger&auml;te</option></select>
    <button class="btn btn-sm" onclick="loadStats()">&#x21BB;</button>
    <button class="btn btn-sm" onclick="exportStatsCSV()">CSV</button>
  </div>
  <div class="table-wrap">
    <table><thead><tr>
      <th>Ger&auml;t</th><th>Produkt</th><th>Kategorie</th>
      <th>Eingelagert</th><th>Ausgelagert</th><th>Lagerdauer</th>
    </tr></thead>
    <tbody id="statsBody"></tbody></table>
    <div id="statsEmpty" class="empty" style="display:none">Noch keine Auslagerungen.</div>
  </div>
</div>

<div id="config" class="panel">
  <div style="padding:1.5rem;max-width:640px;display:flex;flex-direction:column;gap:1.5rem">
    <div style="background:var(--surface);border:1px solid var(--border);border-radius:.75rem;padding:1.25rem">
      <h3 style="margin-bottom:1rem">Kategorien</h3>
      <div id="catList" style="margin-bottom:1rem"></div>
    </div>
    <div style="background:var(--surface);border:1px solid var(--border);border-radius:.75rem;padding:1.25rem">
      <h3 style="margin-bottom:1rem">Produkt-Vorlagen</h3>
      <div id="prodList" style="margin-bottom:1rem;max-height:300px;overflow-y:auto"></div>
      <div style="display:flex;gap:.5rem;flex-wrap:wrap">
        <input type="text" id="pName" placeholder="Name *" style="flex:1;min-width:120px">
        <input type="text" id="pBrand" placeholder="Marke" style="width:120px">
        <input type="text" id="pCat" placeholder="Kategorie" style="width:130px">
        <input type="number" id="pDays" placeholder="MHD-Tage" style="width:100px" min="0">
        <button class="btn btn-sm" onclick="addProd()">+ Hinzuf&uuml;gen</button>
      </div>
    </div>
    <div id="cfgMsg" style="font-size:.85rem"></div>
  </div>
</div>

<script>
const API='/api';
function esc(s){return String(s||'').replace(/&/g,'&amp;').replace(/</g,'&lt;').replace(/>/g,'&gt;');}
function tab(id,btn){
  document.querySelectorAll('.panel').forEach(p=>p.classList.remove('active'));
  document.querySelectorAll('nav button').forEach(b=>b.classList.remove('active'));
  document.getElementById(id).classList.add('active');btn.classList.add('active');
  if(id==='devices')loadDevices();
  if(id==='inventory')loadInv();
  if(id==='statistics')loadStats();
  if(id==='config'){loadCats();loadProds();}
}
function statusOf(d){return d<0?'danger':d<=3?'danger':d<=7?'warn':'ok';}
function daysLabel(d){return d<0?'Abgelaufen':d===0?'Heute':d+' Tage';}
function fmtDate(s){if(!s||s.length<10)return'–';const[y,m,d]=s.split('-');return d+'.'+m+'.'+y;}
function fmtTime(ts){if(!ts)return'–';const d=new Date(ts*1000);return d.toLocaleString('de-DE');}

// Übersicht
async function loadOverview(){
  try{
    const[st,inv]=await Promise.all([
      fetch(API+'/status').then(r=>r.json()),
      fetch(API+'/inventory').then(r=>r.json())
    ]);
    document.getElementById('oTotal').textContent=st.total;
    document.getElementById('oDevices').textContent=st.devices;
    document.getElementById('oWarn').textContent=st.expiring_7d;
    document.getElementById('oExpired').textContent=st.expired;
    const warn=inv.filter(i=>i.days_left<=7);
    const tbody=document.getElementById('overviewWarnBody');tbody.innerHTML='';
    if(warn.length===0){document.getElementById('overviewWarnEmpty').style.display='block';return;}
    warn.forEach(i=>{
      const sc=statusOf(i.days_left);const tr=document.createElement('tr');
      tr.innerHTML='<td>'+esc(i.device_name)+(i.device_room?' <small style="color:var(--muted)">'+esc(i.device_room)+'</small>':'')+'</td>'+
        '<td style="font-weight:500">'+esc(i.name)+'</td><td>'+esc(i.category||'–')+'</td>'+
        '<td style="color:var(--'+sc+')">'+fmtDate(i.expiry)+'</td>'+
        '<td><span class="pill pill-'+sc+'">'+daysLabel(i.days_left)+'</span></td>';
      tbody.appendChild(tr);
    });
  }catch(e){console.error(e);}
}

// Geräte
async function loadDevices(){
  const rows=await fetch(API+'/devices').then(r=>r.json());
  const tbody=document.getElementById('devBody');tbody.innerHTML='';
  if(rows.length===0){document.getElementById('devEmpty').style.display='block';return;}
  document.getElementById('devEmpty').style.display='none';
  rows.forEach(d=>{const tr=document.createElement('tr');
    tr.innerHTML='<td><span class="online'+(d.online?'':' offline')+'"></span>'+(d.online?'Online':'Offline')+'</td>'+
      '<td style="font-weight:500">'+esc(d.name||d.device_id)+'</td>'+
      '<td>'+esc(d.room||'–')+'</td><td class="mono">'+esc(d.ip||'–')+'</td>'+
      '<td class="mono" style="font-size:.75rem;color:var(--muted)">'+esc(d.device_id)+'</td>'+
      '<td style="color:var(--muted);font-size:.8rem">'+fmtTime(d.last_seen)+'</td>';
    tbody.appendChild(tr);});
  // Geräte-Dropdown befüllen
  const sel1=document.getElementById('invDevice');
  const sel2=document.getElementById('statsDevice');
  [sel1,sel2].forEach(sel=>{
    const prev=sel.value;sel.innerHTML='<option value="">Alle Geräte</option>';
    rows.forEach(d=>{const o=document.createElement('option');
      o.value=d.device_id;o.textContent=d.name||d.device_id;sel.appendChild(o);});
    if(prev)sel.value=prev;
  });
}

// Inventar
let allInv=[];
async function loadInv(){
  const status=document.getElementById('invStatus').value;
  const dev=document.getElementById('invDevice').value;
  let url=API+'/inventory?status='+(status||'active');
  if(dev)url+='&device_id='+encodeURIComponent(dev);
  allInv=await fetch(url).then(r=>r.json());
  renderInv();
}
function renderInv(){
  const q=document.getElementById('invSearch').value.toLowerCase();
  const items=allInv.filter(i=>!q||i.name.toLowerCase().includes(q)||
    (i.brand||'').toLowerCase().includes(q)||(i.category||'').toLowerCase().includes(q));
  const tbody=document.getElementById('invBody');tbody.innerHTML='';
  if(items.length===0){document.getElementById('invEmpty').style.display='block';return;}
  document.getElementById('invEmpty').style.display='none';
  items.forEach(i=>{const sc=statusOf(i.days_left);const tr=document.createElement('tr');
    tr.innerHTML='<td>'+esc(i.device_name)+(i.device_room?' <small style="color:var(--muted)">'+esc(i.device_room)+'</small>':'')+'</td>'+
      '<td style="font-weight:500">'+esc(i.name)+'</td><td>'+esc(i.category||'–')+'</td>'+
      '<td style="color:var(--'+sc+')">'+fmtDate(i.expiry)+'</td>'+
      '<td><span class="pill pill-'+sc+'">'+daysLabel(i.days_left)+'</span></td>'+
      '<td>'+i.qty+'</td><td class="mono" style="font-size:.75rem;color:var(--muted)">'+esc(i.label||'–')+'</td>';
    tbody.appendChild(tr);});
}
function exportInvCSV(){
  const rows=[['Gerät','Raum','Produkt','Kategorie','MHD','Verbleibend','Menge','Label']];
  allInv.forEach(i=>rows.push([i.device_name,i.device_room||'',i.name,i.category||'',
    i.expiry,i.days_left,i.qty,i.label||'']));
  const csv=rows.map(r=>r.map(c=>'"'+String(c||'').replace(/"/g,'""')+'"').join(',')).join('\\n');
  const a=document.createElement('a');
  a.href='data:text/csv;charset=utf-8,'+encodeURIComponent(csv);
  a.download='inventar_zentral_'+new Date().toISOString().slice(0,10)+'.csv';a.click();
}

// Statistik
let allStats=[];
async function loadStats(){
  const dev=document.getElementById('statsDevice').value;
  let url=API+'/storage-stats';
  if(dev)url+='?device_id='+encodeURIComponent(dev);
  const d=await fetch(url).then(r=>r.json());
  allStats=d.stats||[];
  document.getElementById('sTotal').textContent=d.count||0;
  document.getElementById('sAvg').textContent=(d.avg_days||0)+' Tage';
  const tbody=document.getElementById('statsBody');tbody.innerHTML='';
  if(allStats.length===0){document.getElementById('statsEmpty').style.display='block';return;}
  document.getElementById('statsEmpty').style.display='none';
  allStats.forEach(s=>{const tr=document.createElement('tr');
    tr.innerHTML='<td>'+esc(s.device_name)+'</td><td style="font-weight:500">'+esc(s.name)+'</td>'+
      '<td>'+esc(s.category||'–')+'</td><td class="mono">'+fmtDate(s.added_date)+'</td>'+
      '<td class="mono">'+fmtDate(s.removed_date)+'</td>'+
      '<td><span class="pill pill-ok">'+s.storage_days+' Tage</span></td>';
    tbody.appendChild(tr);});
}
function exportStatsCSV(){
  const rows=[['Gerät','Produkt','Kategorie','Eingelagert','Ausgelagert','Tage']];
  allStats.forEach(s=>rows.push([s.device_name,s.name,s.category||'',
    s.added_date||'',s.removed_date||'',s.storage_days]));
  const csv=rows.map(r=>r.map(c=>'"'+String(c||'').replace(/"/g,'""')+'"').join(',')).join('\\n');
  const a=document.createElement('a');
  a.href='data:text/csv;charset=utf-8,'+encodeURIComponent(csv);
  a.download='statistik_'+new Date().toISOString().slice(0,10)+'.csv';a.click();
}

// Konfiguration: Kategorien & Produkte
async function loadCats(){
  const cats=await fetch(API+'/categories').then(r=>r.json());
  const div=document.getElementById('catList');
  if(cats.length===0){div.innerHTML='<p style="color:var(--muted);font-size:.9rem">Noch keine Kategorien (werden von Geräten gesendet).</p>';return;}
  div.innerHTML=cats.map(c=>`<span style="display:inline-block;background:#${((c.bg>>11&31)<<3|0).toString(16).padStart(2,'0')}${((c.bg>>5&63)<<2|0).toString(16).padStart(2,'0')}${((c.bg&31)<<3|0).toString(16).padStart(2,'0')};color:#fff;padding:.2rem .6rem;border-radius:.4rem;margin:.2rem;font-size:.85rem">${esc(c.name)}</span>`).join('');
}
async function loadProds(){
  const prods=await fetch(API+'/products').then(r=>r.json());
  const div=document.getElementById('prodList');
  if(prods.length===0){div.innerHTML='<p style="color:var(--muted);font-size:.9rem">Noch keine Vorlagen.</p>';return;}
  div.innerHTML=prods.map(p=>`<div style="display:flex;justify-content:space-between;align-items:center;padding:.4rem 0;border-bottom:1px solid var(--border)">
    <span>${esc(p.name)} <small style="color:var(--muted)">${esc(p.category||'')}${p.default_days?' • '+p.default_days+' Tage':''}</small></span>
    <button class="btn btn-danger btn-sm" onclick="delProd(${p.id})">✕</button></div>`).join('');
}
async function addProd(){
  const name=document.getElementById('pName').value.trim();
  if(!name){document.getElementById('cfgMsg').textContent='Name fehlt.';return;}
  const r=await fetch(API+'/products',{method:'POST',headers:{'Content-Type':'application/json'},
    body:JSON.stringify({name,brand:document.getElementById('pBrand').value.trim(),
    category:document.getElementById('pCat').value.trim(),
    default_days:parseInt(document.getElementById('pDays').value)||0})});
  if(r.ok){document.getElementById('pName').value='';loadProds();}
}
async function delProd(id){
  if(!confirm('Löschen?'))return;
  await fetch(API+'/products/'+id,{method:'DELETE'});loadProds();
}

// Init
loadOverview();
loadDevices();
// Auto-refresh Übersicht alle 60s
setInterval(loadOverview,60000);
</script>
</body>
</html>"""


@app.get("/", response_class=HTMLResponse)
def dashboard():
    return DASHBOARD_HTML
