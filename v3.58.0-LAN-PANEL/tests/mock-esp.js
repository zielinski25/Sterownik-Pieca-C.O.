'use strict';
// ─────────────────────────────────────────────────────────────────────────────
// Atrapa centrali ESP32 (mock) — implementuje wyłącznie kontrakt HTTP, z
// którego korzysta panel WWW i stara strona:
//   GET  /                     → nowy panel (WIFI_LOCAL_PANEL_HTML)
//   GET  /stara                → stara strona (wifiZbudujStrone())
//   GET  /api/lan/status       → ten sam JSON co /piec/status + lan/lan_api/internet
//   POST /api/lan/cmd          → slot komendy (token w nagłówku X-Piec-Token)
//   GET  /api/lan/ack?id=      → wynik komendy (odpowiednik /piec/ack)
//   GET  /api/status           → status starej strony
//   GET  /api/history, /api/prognoza, /api/solar-archive → minimalne odpowiedzi
// Semantyka /api/lan/cmd i /api/lan/ack odtworzona 1:1 z lanKomendaPrzyjmij()
// i lanAckOdpowiedz() (main_centrala.cpp): jeden slot, retry tego samego id,
// 409 przy zajętym slocie, 403 przy złym tokenie, 202 po przyjęciu.
//
// Tryb "remote" (serwer bez endpointów LAN) służy testom pracy zdalnej przez
// Firebase — wtedy atrapa Firebasa przechwytuje wywołania w polyfillu fetch.
// ─────────────────────────────────────────────────────────────────────────────
const http = require('http');
const { buildStatus, buildOldStatus } = require('./fixtures');

class MockEsp {
  constructor(opts = {}) {
    this.opts = Object.assign({
      port: 0,
      cmdToken: 'TEST-TOKEN-123',
      lanTokenOk: true,         // false = symulacja starego firmware (brak /api/lan/token)
      cmdDelayMs: 300,
      internet: false,        // pole "internet" w /api/lan/status
      lan: true,              // false = serwer BEZ endpointów LAN (tryb zdalny)
      dropLan: false,         // true = /api/lan/status przestaje odpowiadać
      cmdResult: null         // {ok, error} wymuszone dla komendy
    }, opts);
    this.counters = {
      root: 0, stara: 0, lanStatus: 0, lanToken: 0, lanCmd: 0, lanAck: 0, apiStatus: 0,
      history: 0, prognoza: 0, solar: 0, other: 0
    };
    this.lanCmdCalls = [];      // {id, cmd, token}
    this.slot = null;           // {id, cmd, at}
    this.ack = null;            // {cmdId, done, ok, error}
    this.server = null;
    this.port = null;
  }

  start() {
    return new Promise(resolve => {
      this.server = http.createServer((req, res) => this._handle(req, res));
      this.server.listen(this.opts.port, '127.0.0.1', () => {
        this.port = this.server.address().port;
        resolve(this.port);
      });
    });
  }

  stop() {
    return new Promise(resolve => {
      if (!this.server) return resolve();
      // wykonaj pendingowe timery slotów natychmiast
      if (this._slotTimer) { clearTimeout(this._slotTimer); this._finishSlot(); }
      // Panel testowy żyje do końca procesu i utrzymuje połączenia keep-alive
      // (odpytywanie co 4 s) — samo close() czekałoby na nie w nieskończoność.
      if (typeof this.server.closeAllConnections === 'function') this.server.closeAllConnections();
      if (typeof this.server.closeIdleConnections === 'function') this.server.closeIdleConnections();
      this.server.close(() => resolve());
      // Bezpiecznik: resolve najpóźniej po 2 s niezależnie od wszystkiego
      setTimeout(resolve, 2000);
    });
  }

  _json(res, code, obj, extraHeaders) {
    const body = JSON.stringify(obj);
    res.writeHead(code, Object.assign({
      'Content-Type': 'application/json',
      'Cache-Control': 'no-store, max-age=0'
    }, extraHeaders || {}));
    res.end(body);
  }

  _handle(req, res) {
    const u = new URL(req.url, 'http://127.0.0.1');
    const path = u.pathname;

    if (req.method === 'GET' && path === '/') {
      this.counters.root++;
      res.writeHead(200, { 'Content-Type': 'text/html; charset=utf-8', 'Cache-Control': 'no-store, max-age=0' });
      res.end(this.opts.panelHtml);
      return;
    }
    if (req.method === 'GET' && path === '/stara') {
      this.counters.stara++;
      res.writeHead(200, { 'Content-Type': 'text/html; charset=utf-8', 'Cache-Control': 'no-store, max-age=0' });
      res.end(this.opts.staraHtml || '<!DOCTYPE html><html><body>stara</body></html>');
      return;
    }
    if (this.opts.lan && req.method === 'GET' && path === '/api/lan/status') {
      this.counters.lanStatus++;
      if (this.opts.dropLan) { res.socket.destroy(); return; }
      // Realny ESP dopisuje tu pola: "lan":true,"lan_api":1,"internet":<bool>
      const s = buildStatus({});
      s.lan = true;
      s.lan_api = 1;
      s.internet = !!this.opts.internet;
      this._json(res, 200, s, {
        'Access-Control-Allow-Origin': '*',
        'Access-Control-Allow-Methods': 'GET, OPTIONS',
        'Access-Control-Allow-Headers': 'Content-Type'
      });
      return;
    }
    // [v3.58.3 LAN-TOKEN-AUTO] Token komend dla panelu z centrali (bez
    // weryfikacji — model "kto w sieci, ten steruje", jak endpointy starej
    // strony). lanTokenOk:false symuluje stary firmware bez tego endpointu.
    if (this.opts.lan && req.method === 'GET' && path === '/api/lan/token') {
      this.counters.lanToken++;
      if (this.opts.lanTokenOk === false) {
        this._json(res, 500, { ok: false, error: 'token_niedostepny' });
        return;
      }
      this._json(res, 200, { token: this.opts.cmdToken });
      return;
    }
    if (this.opts.lan && req.method === 'POST' && path === '/api/lan/cmd') {
      this.counters.lanCmd++;
      const token = req.headers['x-piec-token'];
      let body = '';
      req.on('data', d => { body += d; });
      req.on('end', () => {
        const params = new URLSearchParams(body);
        const id = (params.get('id') || '').trim();
        const cmd = (params.get('cmd') || '').trim();
        this.lanCmdCalls.push({ id, cmd, token });
        if (token !== this.opts.cmdToken) {
          this._json(res, 403, { accepted: false, error: 'nieprawidlowy_token' });
          return;
        }
        if (!id || !cmd) {
          this._json(res, 400, { accepted: false, error: 'brak_id_lub_cmd' });
          return;
        }
        if (this.ack && this.ack.cmdId === id && this.ack.done) {
          this._json(res, 200, { accepted: true, cmdId: id, done: true, ok: this.ack.ok, error: this.ack.error || '' });
          return;
        }
        if (this.slot) {
          if (this.slot.id !== id) {
            this._json(res, 409, { accepted: false, error: 'centrala_wykonuje_poprzednia_komende' });
            return;
          }
          this._json(res, 202, { accepted: true, cmdId: id, done: false });
          return;
        }
        this.slot = { id, cmd, at: Date.now() };
        this._slotTimer = setTimeout(() => this._finishSlot(), this.opts.cmdDelayMs);
        this._json(res, 202, { accepted: true, cmdId: id, done: false });
      });
      return;
    }
    if (this.opts.lan && req.method === 'GET' && path === '/api/lan/ack') {
      this.counters.lanAck++;
      const id = (u.searchParams.get('id') || '').trim();
      if (!id) { this._json(res, 400, { done: false, error: 'brak_id' }); return; }
      if (this.ack && this.ack.cmdId === id) {
        this._json(res, 200, { cmdId: id, done: true, ok: this.ack.ok, error: this.ack.error || '' });
        return;
      }
      if (this.slot && this.slot.id === id) {
        this._json(res, 200, { cmdId: id, done: false });
        return;
      }
      this._json(res, 404, { cmdId: id, done: false, error: 'nieznany_cmdId' });
      return;
    }
    if (req.method === 'GET' && path === '/api/status') {
      this.counters.apiStatus++;
      this._json(res, 200, buildOldStatus({}));
      return;
    }
    if (req.method === 'GET' && path === '/api/history') {
      this.counters.history++;
      res.writeHead(200, { 'Content-Type': 'text/plain; charset=utf-8' });
      res.end('ts,ogrz,bojler,zewn\n1762751000,58.1,48.2,11.7\n1762751100,58.4,48.6,11.8\n');
      return;
    }
    if (req.method === 'GET' && path === '/api/prognoza') {
      this.counters.prognoza++;
      this._json(res, 200, { ok: true });
      return;
    }
    if (req.method === 'GET' && path === '/api/solar-archive') {
      this.counters.solar++;
      this._json(res, 200, { days: [], syncStatus: '', fetchedAtUtc: '' });
      return;
    }
    this.counters.other++;
    res.writeHead(404, { 'Content-Type': 'text/plain' });
    res.end('not found: ' + path);
  }

  _finishSlot() {
    if (!this.slot) return;
    const forced = this.opts.cmdResult;
    const ok = forced ? !!forced.ok : true;
    const error = forced && forced.error ? forced.error : '';
    this.ack = { cmdId: this.slot.id, done: true, ok, error };
    this.slot = null;
    this._slotTimer = null;
  }
}

// ─────────────────────────────────────────────────────────────────────────────
// Atrapa Firebase (praca zdalna): REST RTDB + Google Identity.
// Przechwytywana w polyfillu fetch po nazwie hosta — nigdy prawdziwa sieć.
// ─────────────────────────────────────────────────────────────────────────────
class MockFirebase {
  constructor(opts = {}) {
    this.opts = Object.assign({
      cmdToken: 'TEST-TOKEN-123',
      cmdDelayMs: 300,
      email: 'test@akwarium.local',
      password: 'test-haslo-123',
      apiKey: 'AIzaSyTESTTESTTESTTESTTESTTESTTESTTE'
    }, opts);
    this.counters = { signin: 0, refresh: 0, statusGet: 0, cmdPatch: 0, ackGet: 0 };
    this.cmdPatches = [];
    this.ack = null;
    this._timer = null;
  }

  // Obsługa żądania "firebase'owego". Zwraca {status, body} lub null, gdy URL
  // nie należy do atrapy.
  handle(url, method, body) {
    const s = String(url);
    if (s.includes('identitytoolkit.googleapis.com') && s.includes('signInWithPassword')) {
      this.counters.signin++;
      const j = JSON.parse(body || '{}');
      if (j.email === this.opts.email && j.password === this.opts.password) {
        return { status: 200, body: { idToken: 'idtok-1', refreshToken: 'reftok-1', email: j.email } };
      }
      return { status: 400, body: { error: { message: 'INVALID_PASSWORD' } } };
    }
    if (s.includes('securetoken.googleapis.com')) {
      this.counters.refresh++;
      return { status: 200, body: { id_token: 'idtok-2', refresh_token: 'reftok-2' } };
    }
    if (s.includes('firebasedatabase.app')) {
      const clean = s.split('?')[0];
      if (method === 'GET' && clean.endsWith('/piec/status.json')) {
        this.counters.statusGet++;
        return { status: 200, body: buildStatus({}) };
      }
      if (method === 'PATCH' && clean.endsWith('/piec/cmd.json')) {
        this.counters.cmdPatch++;
        const j = JSON.parse(body || '{}');
        this.cmdPatches.push(j);
        const forced = this.opts.cmdResult;
        this._timer = setTimeout(() => {
          this.ack = { cmdId: j.id, ok: forced ? !!forced.ok : true, error: forced && forced.error ? forced.error : '' };
        }, this.opts.cmdDelayMs);
        return { status: 200, body: j };
      }
      if (method === 'GET' && clean.endsWith('/piec/ack.json')) {
        this.counters.ackGet++;
        if (this.ack) return { status: 200, body: this.ack };
        return { status: 200, body: null };
      }
    }
    return null;
  }

  dispose() { if (this._timer) clearTimeout(this._timer); }
}

module.exports = { MockEsp, MockFirebase };
