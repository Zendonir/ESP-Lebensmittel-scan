"""
Lebensmittel-Scanner Zentral-Server
Multi-Haushalt-fähig: Geräte werden Haushalten zugeordnet.

Start: docker compose up --build
       oder: uvicorn main:app --host 0.0.0.0 --port 8000 --reload
"""

import os
import sqlite3
import time
from contextlib import asynccontextmanager
from datetime import date, datetime
from typing import Any

from fastapi import FastAPI, HTTPException, Request
from fastapi.middleware.cors import CORSMiddleware
from fastapi.responses import HTMLResponse, JSONResponse
from pydantic import BaseModel

DB_PATH = os.environ.get("DB_PATH", "lager.db")


# ── Datenbank ─────────────────────────────────────────────────

def get_db() -> sqlite3.Connection:
    conn = sqlite3.connect(DB_PATH)
    conn.row_factory = sqlite3.Row
    conn.execute("PRAGMA journal_mode=WAL")
    conn.execute("PRAGMA foreign_keys=ON")
    return conn


def init_db():
    with get_db() as db:
        db.executescript("""
        CREATE TABLE IF NOT EXISTS households (
            id         INTEGER PRIMARY KEY AUTOINCREMENT,
            name       TEXT UNIQUE NOT NULL,
            created_at INTEGER
        );
        CREATE TABLE IF NOT EXISTS devices (
            device_id    TEXT PRIMARY KEY,
            name         TEXT,
            room         TEXT,
            ip           TEXT,
            last_seen    INTEGER,
            registered   INTEGER,
            household_id INTEGER DEFAULT 0
        );
        CREATE TABLE IF NOT EXISTS inventory (
            id           INTEGER PRIMARY KEY AUTOINCREMENT,
            device_id    TEXT,
            household_id INTEGER DEFAULT 0,
            name         TEXT,
            brand        TEXT,
            category     TEXT,
            expiry       TEXT,
            added        TEXT,
            qty          INTEGER DEFAULT 1,
            label        TEXT UNIQUE,
            removed      INTEGER DEFAULT 0,
            removed_at   TEXT
        );
        CREATE TABLE IF NOT EXISTS storage_stats (
            id           INTEGER PRIMARY KEY AUTOINCREMENT,
            device_id    TEXT,
            household_id INTEGER DEFAULT 0,
            name         TEXT,
            category     TEXT,
            added_date   TEXT,
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
        # Migrations: Spalten in bestehende Tabellen einfügen (idempotent)
        for sql in [
            "ALTER TABLE devices ADD COLUMN household_id INTEGER DEFAULT 0",
            "ALTER TABLE inventory ADD COLUMN household_id INTEGER DEFAULT 0",
            "ALTER TABLE storage_stats ADD COLUMN household_id INTEGER DEFAULT 0",
        ]:
            try:
                db.execute(sql)
            except Exception:
                pass  # Spalte existiert bereits


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

class HouseholdIn(BaseModel):
    name: str


class DeviceReg(BaseModel):
    device_id:    str
    device_name:  str = ""
    device_room:  str = ""
    ip:           str = ""
    household_id: int = 0


class EventPayload(BaseModel):
    device_id:   str
    device_name: str = ""
    device_room: str = ""
    type:        str
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


def touch_device(db: sqlite3.Connection, device_id: str, name: str = "",
                 room: str = "", ip: str = "", household_id: int | None = None):
    hh_clause = ", household_id=excluded.household_id" if household_id is not None else ""
    db.execute(
        f"""INSERT INTO devices(device_id,name,room,ip,last_seen,registered,household_id)
           VALUES(?,?,?,?,?,?,?)
           ON CONFLICT(device_id) DO UPDATE SET
             last_seen=excluded.last_seen,
             name=CASE WHEN excluded.name!='' THEN excluded.name ELSE name END,
             room=CASE WHEN excluded.room!='' THEN excluded.room ELSE room END,
             ip=CASE WHEN excluded.ip!='' THEN excluded.ip ELSE ip END
             {hh_clause}""",
        (device_id, name, room, ip, int(time.time()), int(time.time()),
         household_id if household_id is not None else 0)
    )


def get_device_household(db: sqlite3.Connection, device_id: str) -> int:
    row = db.execute(
        "SELECT household_id FROM devices WHERE device_id=?", (device_id,)
    ).fetchone()
    return row["household_id"] if row else 0


# ── Haushalt-Endpunkte ────────────────────────────────────────

@app.get("/api/households")
def list_households():
    with get_db() as db:
        rows = db.execute("SELECT * FROM households ORDER BY name").fetchall()
        counts = {
            r["household_id"]: r["cnt"]
            for r in db.execute(
                "SELECT household_id, COUNT(*) as cnt FROM devices GROUP BY household_id"
            ).fetchall()
        }
    return [{"id": r["id"], "name": r["name"],
             "device_count": counts.get(r["id"], 0)} for r in rows]


@app.post("/api/households", status_code=201)
def create_household(h: HouseholdIn):
    name = h.name.strip()
    if not name:
        raise HTTPException(400, "Name darf nicht leer sein")
    with get_db() as db:
        try:
            cur = db.execute(
                "INSERT INTO households(name, created_at) VALUES(?,?)",
                (name, int(time.time()))
            )
            return {"ok": True, "id": cur.lastrowid, "name": name}
        except sqlite3.IntegrityError:
            raise HTTPException(409, f'Haushalt "{name}" existiert bereits')


@app.delete("/api/households/{hid}")
def delete_household(hid: int):
    with get_db() as db:
        devs = db.execute(
            "SELECT COUNT(*) FROM devices WHERE household_id=?", (hid,)
        ).fetchone()[0]
        if devs > 0:
            raise HTTPException(
                400, f"Haushalt hat noch {devs} Gerät(e). Bitte zuerst umzuweisen."
            )
        db.execute("DELETE FROM households WHERE id=?", (hid,))
    return {"ok": True}


@app.put("/api/devices/{device_id}/household")
def assign_device_household(device_id: str, body: dict):
    hid = int(body.get("household_id", 0))
    with get_db() as db:
        db.execute("UPDATE devices SET household_id=? WHERE device_id=?", (hid, device_id))
    return {"ok": True}


# ── Device-Endpunkte ──────────────────────────────────────────

@app.get("/api/status")
def status(household_id: int = 0):
    with get_db() as db:
        hh = f" AND household_id={household_id}" if household_id else ""
        total   = db.execute(f"SELECT COUNT(*) FROM inventory WHERE removed=0{hh}").fetchone()[0]
        devices = db.execute("SELECT COUNT(*) FROM devices").fetchone()[0]
        rows    = db.execute(f"SELECT expiry FROM inventory WHERE removed=0{hh}").fetchall()
        warn7   = sum(1 for r in rows if 0 <= days_until(r["expiry"]) <= 7)
        expired = sum(1 for r in rows if days_until(r["expiry"]) < 0)
    return {"total": total, "devices": devices, "expiring_7d": warn7, "expired": expired}


@app.post("/api/devices")
async def register_device(dev: DeviceReg):
    with get_db() as db:
        touch_device(db, dev.device_id, dev.device_name, dev.device_room,
                     dev.ip, dev.household_id if dev.household_id else None)
    return {"ok": True}


@app.get("/api/devices")
def list_devices():
    with get_db() as db:
        rows = db.execute("""
            SELECT d.*, h.name as hh_name
            FROM devices d
            LEFT JOIN households h ON d.household_id = h.id
            ORDER BY d.last_seen DESC
        """).fetchall()
    now = int(time.time())
    return [{"device_id": r["device_id"], "name": r["name"],
             "room": r["room"], "ip": r["ip"],
             "household_id": r["household_id"],
             "household_name": r["hh_name"] or "",
             "online": now - r["last_seen"] < 300,
             "last_seen": r["last_seen"]} for r in rows]


# ── Event-Endpunkt ────────────────────────────────────────────

@app.post("/api/events")
async def receive_event(ev: EventPayload):
    with get_db() as db:
        touch_device(db, ev.device_id, ev.device_name, ev.device_room)
        household_id = get_device_household(db, ev.device_id)

        if ev.type == "item_added":
            d = ev.data
            db.execute(
                """INSERT INTO inventory
                   (device_id,household_id,name,brand,category,expiry,added,qty,label)
                   VALUES(?,?,?,?,?,?,?,?,?)
                   ON CONFLICT(label) DO UPDATE SET
                     qty=qty+excluded.qty, removed=0, removed_at=NULL""",
                (ev.device_id, household_id, d.get("name", ""), d.get("brand", ""),
                 d.get("category", ""), d.get("expiry", ""), d.get("added", ""),
                 d.get("qty", 1), d.get("label", ""))
            )

        elif ev.type == "item_removed":
            d = ev.data
            today = date.today().isoformat()
            db.execute(
                "UPDATE inventory SET removed=1, removed_at=? WHERE label=?",
                (today, d.get("label", ""))
            )
            if d.get("storageDays") is not None:
                db.execute(
                    """INSERT INTO storage_stats
                       (device_id,household_id,name,category,added_date,
                        removed_date,storage_days,recorded_at)
                       VALUES(?,?,?,?,?,?,?,?)""",
                    (ev.device_id, household_id, d.get("name", ""), d.get("category", ""),
                     d.get("added", ""), today, d.get("storageDays", 0), int(time.time()))
                )

    return {"ok": True}


# ── Inventar-Endpunkte ────────────────────────────────────────

@app.get("/api/inventory")
def get_inventory(device_id: str = "", category: str = "",
                  household_id: int = 0, status: str = "active", limit: int = 0):
    sql = """SELECT i.*, d.name as dev_name, d.room as dev_room,
                    h.name as hh_name
             FROM inventory i
             LEFT JOIN devices d ON i.device_id=d.device_id
             LEFT JOIN households h ON i.household_id=h.id
             WHERE 1=1"""
    params: list = []
    if status == "active":
        sql += " AND i.removed=0"
    elif status == "removed":
        sql += " AND i.removed=1"
    if device_id:
        sql += " AND i.device_id=?"
        params.append(device_id)
    if household_id:
        sql += " AND i.household_id=?"
        params.append(household_id)
    if category:
        sql += " AND i.category=?"
        params.append(category)
    sql += " ORDER BY i.expiry ASC"
    if limit:
        sql += f" LIMIT {int(limit)}"
    with get_db() as db:
        rows = db.execute(sql, params).fetchall()
    return [{"id": r["id"], "device_id": r["device_id"],
             "device_name": r["dev_name"] or r["device_id"],
             "device_room": r["dev_room"] or "",
             "household_id": r["household_id"],
             "household_name": r["hh_name"] or "",
             "name": r["name"], "brand": r["brand"],
             "category": r["category"], "expiry": r["expiry"],
             "added": r["added"], "qty": r["qty"], "label": r["label"],
             "days_left": days_until(r["expiry"] or "")} for r in rows]


@app.delete("/api/inventory/{label}")
def remove_item(label: str):
    with get_db() as db:
        db.execute("UPDATE inventory SET removed=1, removed_at=? WHERE label=?",
                   (date.today().isoformat(), label))
    return {"ok": True}


# ── Statistik ─────────────────────────────────────────────────

@app.get("/api/storage-stats")
def get_storage_stats(device_id: str = "", household_id: int = 0):
    sql = """SELECT s.*, d.name as dev_name, h.name as hh_name
             FROM storage_stats s
             LEFT JOIN devices d ON s.device_id=d.device_id
             LEFT JOIN households h ON s.household_id=h.id
             WHERE 1=1"""
    params: list = []
    if device_id:
        sql += " AND s.device_id=?"
        params.append(device_id)
    if household_id:
        sql += " AND s.household_id=?"
        params.append(household_id)
    sql += " ORDER BY s.recorded_at DESC LIMIT 500"
    with get_db() as db:
        rows = db.execute(sql, params).fetchall()
    items = [{"device_id": r["device_id"],
              "device_name": r["dev_name"] or r["device_id"],
              "household_name": r["hh_name"] or "",
              "name": r["name"], "category": r["category"],
              "added_date": r["added_date"], "removed_date": r["removed_date"],
              "storage_days": r["storage_days"]} for r in rows]
    avg = round(sum(i["storage_days"] for i in items) / len(items)) if items else 0
    return {"stats": items, "count": len(items), "avg_days": avg}


# ── Kategorien & Produkte ─────────────────────────────────────

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
        db.execute(
            "INSERT INTO products(name,brand,barcode,category,default_days) VALUES(?,?,?,?,?)",
            (p.name, p.brand, p.barcode, p.category, p.default_days)
        )
    return {"ok": True}


@app.delete("/api/products/{pid}")
def delete_product(pid: int):
    with get_db() as db:
        db.execute("DELETE FROM products WHERE id=?", (pid,))
    return {"ok": True}


# ── Dashboard HTML ────────────────────────────────────────────

DASHBOARD_HTML = r"""<!DOCTYPE html>
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
      display:flex;align-items:center;padding:0 1rem;flex-wrap:wrap;}
  nav h1{font-size:1rem;padding:.9rem 1rem;color:var(--accent);white-space:nowrap;}
  nav button{background:none;border:none;color:var(--muted);padding:.9rem 1.2rem;
             cursor:pointer;font-size:.9rem;border-bottom:2px solid transparent;white-space:nowrap;}
  nav button.active{color:var(--text);border-bottom-color:var(--accent);}
  .panel{display:none;} .panel.active{display:block;}
  .stats{display:flex;gap:1rem;padding:1.25rem 1.5rem;flex-wrap:wrap;}
  .stat-card{background:var(--surface);border:1px solid var(--border);
             border-radius:.75rem;padding:1rem 1.5rem;min-width:140px;}
  .stat-card .label{font-size:.75rem;color:var(--muted);margin-bottom:.25rem;}
  .stat-card .value{font-size:2rem;font-weight:700;}
  .toolbar{padding:.75rem 1.5rem;display:flex;gap:.75rem;flex-wrap:wrap;align-items:center;}
  select,input[type=text],input[type=search],input[type=number]{
    background:var(--surface);border:1px solid var(--border);color:var(--text);
    padding:.5rem .75rem;border-radius:.5rem;font-size:.9rem;outline:none;}
  input[type=search]{flex:1;min-width:180px;}
  .btn{background:var(--accent);color:#fff;border:none;padding:.5rem 1rem;
       border-radius:.5rem;cursor:pointer;font-size:.9rem;}
  .btn-sm{padding:.25rem .6rem;font-size:.8rem;}
  .btn-danger{background:var(--danger);}
  .btn-ok{background:#065f46;color:var(--ok);}
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
  .pill-accent{background:#1e3a5f;color:var(--accent);}
  .online{display:inline-block;width:8px;height:8px;border-radius:50%;
          background:var(--ok);margin-right:.4rem;}
  .offline{background:var(--muted);}
  .empty{text-align:center;color:var(--muted);padding:3rem;}
  .add-card{background:var(--surface);border:1px solid var(--border);border-radius:.75rem;
            padding:1.25rem;margin:0 1.5rem 1.5rem;max-width:600px;}
  .form-row{display:flex;gap:.75rem;flex-wrap:wrap;align-items:flex-end;}
  .form-row input,.form-row select{flex:1;min-width:180px;}
</style>
</head>
<body>
<nav>
  <h1>&#x1F4E6; Lager-Server</h1>
  <button class="active" onclick="tab('overview',this)">&#x1F3E0; &Uuml;bersicht</button>
  <button onclick="tab('households',this)">&#x1F3DA; Haushalte</button>
  <button onclick="tab('devices',this)">&#x1F4F1; Ger&auml;te</button>
  <button onclick="tab('inventory',this)">&#x1F4E6; Inventar</button>
  <button onclick="tab('statistics',this)">&#x1F4CA; Statistik</button>
  <button onclick="tab('config',this)">&#x2699; Konfiguration</button>
</nav>

<!-- ── Übersicht ────────────────────────────────────────────── -->
<div id="overview" class="panel active">
  <div class="stats">
    <div class="stat-card"><div class="label">Gesamt Artikel</div><div class="value" id="oTotal">–</div></div>
    <div class="stat-card"><div class="label">Ger&auml;te</div><div class="value" id="oDevices">–</div></div>
    <div class="stat-card"><div class="label">Bald ablaufend</div>
      <div class="value" style="color:var(--warn)" id="oWarn">–</div></div>
    <div class="stat-card"><div class="label">Abgelaufen</div>
      <div class="value" style="color:var(--danger)" id="oExpired">–</div></div>
  </div>
  <div class="toolbar">
    <label style="color:var(--muted);font-size:.85rem">Haushalt:
      <select id="ovHousehold" onchange="loadOverview()" style="margin-left:.4rem">
        <option value="">Alle</option></select>
    </label>
  </div>
  <div class="table-wrap">
    <h3 style="padding:0 0 .75rem;font-size:.95rem;color:var(--muted)">
      Bald ablaufende Artikel</h3>
    <table><thead><tr>
      <th>Haushalt</th><th>Ger&auml;t</th><th>Produkt</th><th>Kategorie</th>
      <th>MHD</th><th>Verbleibend</th>
    </tr></thead>
    <tbody id="overviewWarnBody"></tbody></table>
    <div id="overviewWarnEmpty" class="empty" style="display:none">
      Keine ablaufenden Artikel &#x1F389;</div>
  </div>
</div>

<!-- ── Haushalte ────────────────────────────────────────────── -->
<div id="households" class="panel">
  <div class="toolbar">
    <button class="btn btn-sm" onclick="loadHouseholds()">&#x21BB; Aktualisieren</button>
  </div>
  <div class="table-wrap">
    <table><thead><tr>
      <th>Name</th><th>Ger&auml;te</th><th></th>
    </tr></thead>
    <tbody id="hhBody"></tbody></table>
    <div id="hhEmpty" class="empty" style="display:none">Noch keine Haushalte.</div>
  </div>
  <div class="add-card">
    <h3 style="margin-bottom:.75rem">Neuer Haushalt</h3>
    <div class="form-row">
      <input type="text" id="hhName" placeholder="z.B. Hauptwohnung" maxlength="40">
      <button class="btn" onclick="addHousehold()">Hinzuf&uuml;gen</button>
    </div>
    <div id="hhMsg" style="margin-top:.6rem;font-size:.85rem"></div>
  </div>
</div>

<!-- ── Geräte ────────────────────────────────────────────────── -->
<div id="devices" class="panel">
  <div class="toolbar">
    <button class="btn btn-sm" onclick="loadDevices()">&#x21BB;</button>
  </div>
  <div class="table-wrap">
    <table><thead><tr>
      <th>Status</th><th>Haushalt</th><th>Name</th><th>Raum</th>
      <th>IP</th><th>Ger&auml;te-ID</th><th>Zuletzt</th>
    </tr></thead>
    <tbody id="devBody"></tbody></table>
    <div id="devEmpty" class="empty" style="display:none">Noch keine Ger&auml;te.</div>
  </div>
</div>

<!-- ── Inventar ──────────────────────────────────────────────── -->
<div id="inventory" class="panel">
  <div class="toolbar">
    <input type="search" id="invSearch" placeholder="Suchen&hellip;" oninput="renderInv()">
    <select id="invHousehold" onchange="loadInv()"><option value="">Alle Haushalte</option></select>
    <select id="invDevice" onchange="loadInv()"><option value="">Alle Ger&auml;te</option></select>
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
      <th>Haushalt</th><th>Ger&auml;t</th><th>Produkt</th><th>Kategorie</th>
      <th>MHD</th><th>Verbleibend</th><th>Menge</th><th>Label</th>
    </tr></thead>
    <tbody id="invBody"></tbody></table>
    <div id="invEmpty" class="empty" style="display:none">Keine Eintr&auml;ge.</div>
  </div>
</div>

<!-- ── Statistik ─────────────────────────────────────────────── -->
<div id="statistics" class="panel">
  <div class="stats">
    <div class="stat-card"><div class="label">Auslagerungen</div>
      <div class="value" id="sTotal">–</div></div>
    <div class="stat-card"><div class="label">&Oslash; Lagerdauer</div>
      <div class="value" id="sAvg">–</div></div>
  </div>
  <div class="toolbar">
    <select id="statsHousehold" onchange="loadStats()"><option value="">Alle Haushalte</option></select>
    <select id="statsDevice" onchange="loadStats()"><option value="">Alle Ger&auml;te</option></select>
    <button class="btn btn-sm" onclick="loadStats()">&#x21BB;</button>
    <button class="btn btn-sm" onclick="exportStatsCSV()">CSV</button>
  </div>
  <div class="table-wrap">
    <table><thead><tr>
      <th>Haushalt</th><th>Ger&auml;t</th><th>Produkt</th><th>Kategorie</th>
      <th>Eingelagert</th><th>Ausgelagert</th><th>Lagerdauer</th>
    </tr></thead>
    <tbody id="statsBody"></tbody></table>
    <div id="statsEmpty" class="empty" style="display:none">Noch keine Auslagerungen.</div>
  </div>
</div>

<!-- ── Konfiguration ─────────────────────────────────────────── -->
<div id="config" class="panel">
  <div style="padding:1.5rem;max-width:640px;display:flex;flex-direction:column;gap:1.5rem">
    <div class="add-card">
      <h3 style="margin-bottom:1rem">Kategorien</h3>
      <div id="catList"></div>
    </div>
    <div class="add-card">
      <h3 style="margin-bottom:.75rem">Produkt-Vorlagen</h3>
      <div id="prodList" style="margin-bottom:1rem;max-height:300px;overflow-y:auto"></div>
      <div class="form-row">
        <input type="text" id="pName" placeholder="Name *">
        <input type="text" id="pBrand" placeholder="Marke" style="max-width:160px">
        <input type="text" id="pCat" placeholder="Kategorie" style="max-width:160px">
        <input type="number" id="pDays" placeholder="MHD-Tage" style="max-width:100px" min="0">
        <button class="btn btn-sm" onclick="addProd()">+ Hinzuf&uuml;gen</button>
      </div>
    </div>
    <div id="cfgMsg" style="font-size:.85rem"></div>
  </div>
</div>

<script>
const API='/api';
let allHouseholds=[];
function esc(s){return String(s||'').replace(/&/g,'&amp;').replace(/</g,'&lt;').replace(/>/g,'&gt;');}
function tab(id,btn){
  document.querySelectorAll('.panel').forEach(p=>p.classList.remove('active'));
  document.querySelectorAll('nav button').forEach(b=>b.classList.remove('active'));
  document.getElementById(id).classList.add('active');btn.classList.add('active');
  if(id==='households')loadHouseholds();
  if(id==='devices')loadDevices();
  if(id==='inventory')loadInv();
  if(id==='statistics')loadStats();
  if(id==='config'){loadCats();loadProds();}
}
function statusOf(d){return d<0?'danger':d<=3?'danger':d<=7?'warn':'ok';}
function daysLabel(d){return d<0?'Abgelaufen':d===0?'Heute':d+' Tage';}
function fmtDate(s){if(!s||s.length<10)return'–';const[y,m,d]=s.split('-');return d+'.'+m+'.'+y;}
function fmtTime(ts){if(!ts)return'–';return new Date(ts*1000).toLocaleString('de-DE');}

function fillHouseholdDropdowns(selIds){
  selIds.forEach(id=>{
    const sel=document.getElementById(id);if(!sel)return;
    const prev=sel.value;
    sel.innerHTML='<option value="">Alle Haushalte</option>';
    allHouseholds.forEach(h=>{
      const o=document.createElement('option');o.value=h.id;o.textContent=h.name;sel.appendChild(o);
    });
    if(prev)sel.value=prev;
  });
}

// ── Haushalte ─────────────────────────────────────────────────
async function loadHouseholds(){
  allHouseholds=await fetch(API+'/households').then(r=>r.json());
  fillHouseholdDropdowns(['ovHousehold','invHousehold','statsHousehold']);
  const tbody=document.getElementById('hhBody');tbody.innerHTML='';
  if(allHouseholds.length===0){
    document.getElementById('hhEmpty').style.display='block';return;}
  document.getElementById('hhEmpty').style.display='none';
  allHouseholds.forEach(h=>{const tr=document.createElement('tr');
    tr.innerHTML='<td style="font-weight:500">'+esc(h.name)+'</td>'+
      '<td style="color:var(--muted)">'+h.device_count+' Ger&auml;t(e)</td>'+
      '<td><button class="btn btn-danger btn-sm" onclick="deleteHousehold('+h.id+',\''+esc(h.name)+'\')">L&ouml;schen</button></td>';
    tbody.appendChild(tr);});
}
async function addHousehold(){
  const name=document.getElementById('hhName').value.trim();
  const msg=document.getElementById('hhMsg');
  if(!name){msg.style.color='var(--danger)';msg.textContent='Name erforderlich.';return;}
  const r=await fetch(API+'/households',{method:'POST',
    headers:{'Content-Type':'application/json'},body:JSON.stringify({name})});
  const d=await r.json();
  if(r.ok){msg.style.color='var(--ok)';msg.textContent='✓ "'+name+'" erstellt.';
    document.getElementById('hhName').value='';loadHouseholds();}
  else{msg.style.color='var(--danger)';msg.textContent='Fehler: '+(d.detail||r.status);}
}
async function deleteHousehold(id,name){
  if(!confirm('Haushalt "'+name+'" löschen?\nAlle Geräte müssen vorher einem anderen Haushalt zugewiesen werden.'))return;
  const r=await fetch(API+'/households/'+id,{method:'DELETE'});
  const d=await r.json();
  if(r.ok)loadHouseholds();
  else alert('Fehler: '+(d.detail||r.status));
}

// ── Geräte ────────────────────────────────────────────────────
async function loadDevices(){
  const rows=await fetch(API+'/devices').then(r=>r.json());
  const tbody=document.getElementById('devBody');tbody.innerHTML='';
  if(rows.length===0){document.getElementById('devEmpty').style.display='block';return;}
  document.getElementById('devEmpty').style.display='none';
  // Device-Dropdowns befüllen
  ['invDevice','statsDevice'].forEach(selId=>{
    const sel=document.getElementById(selId);if(!sel)return;
    const prev=sel.value;sel.innerHTML='<option value="">Alle Geräte</option>';
    rows.forEach(d=>{const o=document.createElement('option');
      o.value=d.device_id;o.textContent=d.name||d.device_id;sel.appendChild(o);});
    if(prev)sel.value=prev;
  });
  rows.forEach(d=>{const tr=document.createElement('tr');
    const hhBadge=d.household_name?'<span class="pill pill-accent">'+esc(d.household_name)+'</span>':'<span style="color:var(--muted)">–</span>';
    tr.innerHTML='<td><span class="online'+(d.online?'':' offline')+'"></span>'+(d.online?'Online':'Offline')+'</td>'+
      '<td>'+hhBadge+'</td>'+
      '<td style="font-weight:500">'+esc(d.name||d.device_id)+'</td>'+
      '<td>'+esc(d.room||'–')+'</td><td class="mono">'+esc(d.ip||'–')+'</td>'+
      '<td class="mono" style="font-size:.75rem;color:var(--muted)">'+esc(d.device_id)+'</td>'+
      '<td style="color:var(--muted);font-size:.8rem">'+fmtTime(d.last_seen)+'</td>';
    tbody.appendChild(tr);});
}

// ── Übersicht ─────────────────────────────────────────────────
async function loadOverview(){
  const hh=document.getElementById('ovHousehold').value;
  let stUrl=API+'/status',invUrl=API+'/inventory';
  if(hh){stUrl+='?household_id='+hh;invUrl+='?household_id='+hh;}
  const[st,inv]=await Promise.all([
    fetch(stUrl).then(r=>r.json()),fetch(invUrl).then(r=>r.json())]);
  document.getElementById('oTotal').textContent=st.total;
  document.getElementById('oDevices').textContent=st.devices;
  document.getElementById('oWarn').textContent=st.expiring_7d;
  document.getElementById('oExpired').textContent=st.expired;
  const warn=inv.filter(i=>i.days_left<=7);
  const tbody=document.getElementById('overviewWarnBody');tbody.innerHTML='';
  document.getElementById('overviewWarnEmpty').style.display=warn.length===0?'block':'none';
  warn.forEach(i=>{const sc=statusOf(i.days_left);const tr=document.createElement('tr');
    tr.innerHTML='<td><span class="pill pill-accent">'+esc(i.household_name||'–')+'</span></td>'+
      '<td>'+esc(i.device_name)+'</td>'+
      '<td style="font-weight:500">'+esc(i.name)+'</td><td>'+esc(i.category||'–')+'</td>'+
      '<td style="color:var(--'+sc+')">'+fmtDate(i.expiry)+'</td>'+
      '<td><span class="pill pill-'+sc+'">'+daysLabel(i.days_left)+'</span></td>';
    tbody.appendChild(tr);});
}

// ── Inventar ──────────────────────────────────────────────────
let allInv=[];
async function loadInv(){
  const status=document.getElementById('invStatus').value;
  const dev=document.getElementById('invDevice').value;
  const hh=document.getElementById('invHousehold').value;
  let url=API+'/inventory?status='+(status||'active');
  if(dev)url+='&device_id='+encodeURIComponent(dev);
  if(hh)url+='&household_id='+hh;
  allInv=await fetch(url).then(r=>r.json());
  renderInv();
}
function renderInv(){
  const q=document.getElementById('invSearch').value.toLowerCase();
  const items=allInv.filter(i=>!q||i.name.toLowerCase().includes(q)||
    (i.brand||'').toLowerCase().includes(q)||(i.category||'').toLowerCase().includes(q));
  const tbody=document.getElementById('invBody');tbody.innerHTML='';
  document.getElementById('invEmpty').style.display=items.length===0?'block':'none';
  items.forEach(i=>{const sc=statusOf(i.days_left);const tr=document.createElement('tr');
    tr.innerHTML='<td><span class="pill pill-accent">'+esc(i.household_name||'–')+'</span></td>'+
      '<td>'+esc(i.device_name)+'</td>'+
      '<td style="font-weight:500">'+esc(i.name)+'</td><td>'+esc(i.category||'–')+'</td>'+
      '<td style="color:var(--'+sc+')">'+fmtDate(i.expiry)+'</td>'+
      '<td><span class="pill pill-'+sc+'">'+daysLabel(i.days_left)+'</span></td>'+
      '<td>'+i.qty+'</td><td class="mono" style="font-size:.75rem;color:var(--muted)">'+esc(i.label||'–')+'</td>';
    tbody.appendChild(tr);});
}
function exportInvCSV(){
  const rows=[['Haushalt','Gerät','Raum','Produkt','Kategorie','MHD','Tage','Menge','Label']];
  allInv.forEach(i=>rows.push([i.household_name||'',i.device_name,i.device_room||'',
    i.name,i.category||'',i.expiry,i.days_left,i.qty,i.label||'']));
  dlCSV(rows,'inventar');
}

// ── Statistik ─────────────────────────────────────────────────
let allStats=[];
async function loadStats(){
  const dev=document.getElementById('statsDevice').value;
  const hh=document.getElementById('statsHousehold').value;
  let url=API+'/storage-stats';const params=[];
  if(dev)params.push('device_id='+encodeURIComponent(dev));
  if(hh)params.push('household_id='+hh);
  if(params.length)url+='?'+params.join('&');
  const d=await fetch(url).then(r=>r.json());
  allStats=d.stats||[];
  document.getElementById('sTotal').textContent=d.count||0;
  document.getElementById('sAvg').textContent=(d.avg_days||0)+' Tage';
  const tbody=document.getElementById('statsBody');tbody.innerHTML='';
  document.getElementById('statsEmpty').style.display=allStats.length===0?'block':'none';
  allStats.forEach(s=>{const tr=document.createElement('tr');
    tr.innerHTML='<td><span class="pill pill-accent">'+esc(s.household_name||'–')+'</span></td>'+
      '<td>'+esc(s.device_name)+'</td>'+
      '<td style="font-weight:500">'+esc(s.name)+'</td><td>'+esc(s.category||'–')+'</td>'+
      '<td class="mono">'+fmtDate(s.added_date)+'</td>'+
      '<td class="mono">'+fmtDate(s.removed_date)+'</td>'+
      '<td><span class="pill pill-ok">'+s.storage_days+' Tage</span></td>';
    tbody.appendChild(tr);});
}
function exportStatsCSV(){
  const rows=[['Haushalt','Gerät','Produkt','Kategorie','Eingelagert','Ausgelagert','Tage']];
  allStats.forEach(s=>rows.push([s.household_name||'',s.device_name,s.name,
    s.category||'',s.added_date||'',s.removed_date||'',s.storage_days]));
  dlCSV(rows,'statistik');
}
function dlCSV(rows,name){
  const csv=rows.map(r=>r.map(c=>'"'+String(c||'').replace(/"/g,'""')+'"').join(',')).join('\n');
  const a=document.createElement('a');
  a.href='data:text/csv;charset=utf-8,'+encodeURIComponent(csv);
  a.download=name+'_'+new Date().toISOString().slice(0,10)+'.csv';a.click();
}

// ── Konfiguration ─────────────────────────────────────────────
async function loadCats(){
  const cats=await fetch(API+'/categories').then(r=>r.json());
  const div=document.getElementById('catList');
  if(!cats.length){div.innerHTML='<p style="color:var(--muted);font-size:.9rem">Noch keine Kategorien.</p>';return;}
  div.innerHTML=cats.map(c=>{
    const r=(c.bg>>11&31)<<3,g=(c.bg>>5&63)<<2,b=(c.bg&31)<<3;
    const hex='#'+[r,g,b].map(v=>v.toString(16).padStart(2,'0')).join('');
    return`<span style="display:inline-block;background:${hex};color:#fff;padding:.2rem .6rem;border-radius:.4rem;margin:.2rem;font-size:.85rem">${esc(c.name)}</span>`;
  }).join('');
}
async function loadProds(){
  const prods=await fetch(API+'/products').then(r=>r.json());
  const div=document.getElementById('prodList');
  if(!prods.length){div.innerHTML='<p style="color:var(--muted);font-size:.9rem">Noch keine Vorlagen.</p>';return;}
  div.innerHTML=prods.map(p=>`<div style="display:flex;justify-content:space-between;align-items:center;padding:.4rem 0;border-bottom:1px solid var(--border)">
    <span>${esc(p.name)}<small style="color:var(--muted);margin-left:.5rem">${esc(p.category||'')}${p.default_days?' • '+p.default_days+' Tage':''}</small></span>
    <button class="btn btn-danger btn-sm" onclick="delProd(${p.id})">&#x2715;</button></div>`).join('');
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
(async()=>{await loadHouseholds();loadOverview();loadDevices();})();
setInterval(loadOverview,60000);
</script>
</body>
</html>"""


@app.get("/", response_class=HTMLResponse)
def dashboard():
    return DASHBOARD_HTML
