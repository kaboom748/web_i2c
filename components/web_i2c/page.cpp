#include "web_i2c.h"

namespace esphome {
namespace web_i2c {

// I2C debug terminal. Address/register/bytes fields, read/write/scan buttons,
// and SSD1306 shortcuts. The ESP does the real I2C; this page only sends text
// commands and displays the JSON responses.
extern const char WEB_I2C_PAGE[] PROGMEM = R"HTMLDOC(<!DOCTYPE html>
<html lang="fr">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>Web I2C</title>
<style>
  :root{--bg:#0f1115;--fg:#d6d9df;--dim:#8a90a0;--accent:#4fd1c5;--ok:#5dcaa5;--err:#e06c75;--warn:#f6c177;--line:#1c202a;--line2:#232733;--panel:#1b1f2a;--bord:#2c3140;}
  *{box-sizing:border-box;}
  body{margin:0;background:var(--bg);color:var(--fg);font:14px/1.5 system-ui,sans-serif;}
  .mono{font-family:ui-monospace,Menlo,monospace;}
  header{display:flex;align-items:center;gap:12px;padding:10px 14px;border-bottom:1px solid var(--line2);flex-wrap:wrap;}
  h1{font-size:15px;font-weight:500;margin:0;color:var(--accent);}
  .stat{font-size:12px;color:var(--dim);}
  .stat b{color:var(--err);font-weight:500;}
  .stat b.up{color:var(--ok);}
  #topbar{transition:background .3s,box-shadow .3s;border-radius:0 0 8px 8px;}
  #topbar.alert{background:#1c1416;box-shadow:inset 0 0 0 1px var(--err);}
  .brand{display:flex;align-items:baseline;gap:7px;}
  .bname{font-size:15px;font-weight:500;color:var(--accent);}
  .bsub{font-size:11px;color:var(--dim);}
  .pills{display:flex;align-items:center;gap:11px;flex-wrap:wrap;margin-left:auto;}
  .pill{display:inline-flex;align-items:center;gap:5px;font-size:12px;color:var(--fg);}
  .pill .pdot{width:8px;height:8px;border-radius:50%;background:#3a424e;flex:none;}
  .pill b{font-weight:500;}
  .wrap{display:grid;grid-template-columns:1fr 300px;gap:0;}
  @media(max-width:720px){.wrap{grid-template-columns:1fr;}.side{border-left:none;border-top:1px solid var(--line);}}
  .main{padding:14px;min-height:340px;display:flex;flex-direction:column;}
  .side{border-left:1px solid var(--line);padding:14px;}
  .lbl{font-size:11px;color:var(--dim);text-transform:uppercase;letter-spacing:.5px;margin:0 0 8px;}
  .row{display:flex;gap:8px;align-items:center;margin-bottom:8px;flex-wrap:wrap;}
  .cdiv{border-top:1px solid var(--line);}
  label{font-size:12px;color:var(--dim);min-width:52px;}
  input{background:var(--panel);border:1px solid var(--bord);border-radius:6px;color:var(--fg);padding:7px 10px;font-family:ui-monospace,monospace;font-size:13px;}
  input.addr{width:66px;text-transform:uppercase;}
  input.reg{width:66px;text-transform:uppercase;}
  input.len{width:56px;}
  input.data{flex:1;min-width:140px;text-transform:uppercase;}
  button{background:var(--panel);border:1px solid var(--bord);border-radius:6px;color:var(--fg);padding:7px 13px;cursor:pointer;font-size:13px;}
  button:hover:not(:disabled){border-color:var(--accent);}
  button:disabled{opacity:.4;cursor:default;}
  button.go{border-color:var(--accent);color:var(--accent);}
  button.sm{padding:5px 9px;font-size:12px;}
  .seg-row{display:flex;align-items:center;gap:8px;margin-top:6px;}
  .seg-lbl{font-size:12px;color:var(--dim);min-width:54px;}
  .rgrp{font-size:10px;letter-spacing:.05em;color:#5f6570;margin:11px 0 3px;}
  .seg{display:inline-flex;border:1px solid var(--bord);border-radius:6px;overflow:hidden;}
  .tabpanel .seg button.seg-b{width:auto;margin:0;text-align:center;border:none;border-radius:0;background:transparent;color:var(--dim);padding:5px 13px;font-size:12px;}
  .seg button.seg-b.on{background:var(--accent);color:var(--bg);}
  .seg button.seg-b.own{background:var(--warn);color:var(--bg);}
  #fb-canvas{display:none;width:100%;max-width:260px;image-rendering:pixelated;image-rendering:crisp-edges;border:1px solid var(--bord);border-radius:6px;background:#000;margin-top:8px;}
  #out{flex:1;overflow-y:auto;padding:10px;font-family:ui-monospace,monospace;font-size:12.5px;line-height:1.55;}
  .sertools{display:flex;align-items:center;gap:8px;flex-wrap:wrap;}
  .slbl{font-size:12px;color:var(--dim);}
  .tabs{display:flex;gap:2px;border-bottom:1px solid var(--line2);margin-bottom:12px;flex-wrap:wrap;}
  .tab{padding:6px 9px;font-size:12px;color:var(--dim);border-bottom:2px solid transparent;cursor:pointer;white-space:nowrap;}
  .tab.on{color:var(--accent);border-bottom-color:var(--accent);}
  .tabpanel button{width:100%;text-align:left;margin-bottom:6px;font-family:ui-monospace,monospace;font-size:12px;}
  .tabpanel .pair{display:flex;gap:6px;margin-bottom:6px;}
  .tabpanel .pair button{width:auto;flex:1;text-align:center;margin-bottom:0;}
  .sl{display:flex;align-items:center;gap:8px;margin:8px 0 6px;}
  .sval{font-family:ui-monospace,monospace;font-size:12px;color:var(--dim);min-width:26px;text-align:right;}
  .l-ok{color:var(--ok);}.l-err{color:var(--err);}.l-rx{color:var(--accent);}.l-tx{color:var(--warn);}.l-sys{color:var(--dim);font-style:italic;}
  .l-tap{color:#6fb0c9;}
  .l-tapd{color:#586b7a;}
  .card{background:var(--panel);border:1px solid var(--line);border-radius:8px;padding:10px;margin-bottom:12px;}
  .chip{display:inline-block;padding:3px 8px;margin:2px;border-radius:5px;background:#12261e;border:1px solid #1d5a44;color:var(--ok);font-family:ui-monospace,monospace;font-size:12px;cursor:pointer;}
  .chip:hover{border-color:var(--accent);}
  .banner{background:#2a1f14;border:1px solid #6b4f2a;color:var(--warn);padding:10px 14px;font-size:13px;display:none;}
  .banner code{background:var(--panel);padding:1px 5px;border-radius:3px;}
  .hint{font-size:11px;color:var(--dim);margin-top:4px;}
  input[type=range]{accent-color:var(--accent);width:auto;padding:0;}
  input[type=checkbox]{accent-color:var(--accent);width:auto;vertical-align:-2px;}
  .ck{min-width:auto;display:inline-flex;align-items:center;gap:5px;font-size:12px;color:var(--fg);}
  .jbar{display:flex;align-items:center;justify-content:space-between;gap:8px;flex-wrap:wrap;}
  .oled-device{background:linear-gradient(145deg,#3a3a38,#1c1c1a);border-radius:18px;padding:20px 20px 32px;box-shadow:0 10px 30px rgba(0,0,0,.6),inset 0 1px 1px rgba(255,255,255,.08),inset 0 -2px 4px rgba(0,0,0,.5);position:relative;margin:6px auto 0;width:max-content;max-width:100%;}
  .oled-pins{text-align:center;font-size:9px;letter-spacing:4px;color:#888;margin-bottom:6px;font-family:ui-monospace,monospace;}
  .oled-screenframe{background:#000;border-radius:4px;padding:6px;box-shadow:inset 0 0 8px rgba(0,0,0,.9),0 0 0 1px #444;}
  #oledCanvas{display:block;border-radius:2px;background:#000;image-rendering:pixelated;max-width:100%;cursor:crosshair;}
  .drawbar{display:flex;align-items:center;gap:6px;flex-wrap:wrap;margin:12px 0 4px;}
  button.tsel{border-color:var(--accent);color:#08201d;background:var(--accent);}
  .dgrid{display:grid;grid-template-columns:repeat(17,1fr);gap:1px;margin-top:8px;font-family:ui-monospace,monospace;font-size:9px;}
  .dcell{background:#0c0e12;text-align:center;padding:2px 0;border-radius:2px;color:var(--fg);}
  .dcell.dhdr{background:transparent;color:var(--dim);}
  .dcell .dnull{color:#3a3f4b;}
  .oled-bracket{position:absolute;left:-14px;right:-14px;bottom:9px;height:10px;background:linear-gradient(180deg,#4a4a48,#222);border-radius:4px;box-shadow:0 2px 4px rgba(0,0,0,.5);}
  .thumbs{display:flex;gap:12px;flex-wrap:wrap;align-items:center;margin-top:2px;}
  .thumb{position:relative;background:#000;border:1px solid var(--bord);border-radius:4px;padding:3px;cursor:pointer;line-height:0;}
  .thumb canvas{display:block;image-rendering:pixelated;border-radius:2px;}
  .thumb .del{position:absolute;top:-7px;right:-7px;width:17px;height:17px;border-radius:50%;background:var(--panel);border:1px solid var(--err);color:var(--err);font-size:12px;line-height:15px;text-align:center;cursor:pointer;}
  #log-row{display:flex;gap:10px;align-items:stretch;margin-top:10px;height:346px;}
  #log-wrap{flex:1;min-width:0;display:flex;flex-direction:column;background:#0c0e12;border:1px solid var(--line);border-radius:8px;overflow:hidden;}
  .ptools{display:flex;align-items:center;gap:5px;padding:6px 8px;border-bottom:1px solid var(--line);flex-wrap:wrap;}
  #decode-mid{display:flex;align-items:center;flex:none;}
  #dec-panel{display:flex;flex:0 0 44%;min-width:0;flex-direction:column;background:#0c0e12;border:1px solid var(--line);border-radius:8px;overflow:hidden;}
  .dec-head{display:flex;align-items:center;gap:5px;padding:6px 8px;border-bottom:1px solid var(--line);}
  #dec-out{flex:1;overflow-y:auto;padding:8px 10px;font-family:ui-monospace,monospace;font-size:11.5px;line-height:1.5;}
  .ib{width:29px;height:27px;display:inline-flex;align-items:center;justify-content:center;background:transparent;border:1px solid var(--bord);border-radius:6px;color:var(--dim);cursor:pointer;padding:0;flex:none;}
  .ib:hover{border-color:var(--dim);color:var(--fg);}
  .ib.go{border-color:var(--accent);color:var(--accent);}
  .ib.act{background:#3a2f1a;border-color:var(--warn);color:var(--warn);}
  .ib.armed{background:#153230;border-color:var(--accent);color:var(--accent);}
  .ib.fired{background:#3a2f1a;border-color:var(--warn);color:var(--warn);}
  .ib svg{width:15px;height:15px;}
  .lts{display:inline-block;min-width:60px;color:#5f6570;font-size:11px;}
  .trghit{background:rgba(246,193,119,.16);border-radius:3px;}
  .fbox{display:inline-flex;align-items:center;gap:4px;border:1px solid var(--bord);border-radius:6px;padding:0 7px;background:#14171c;}
  .fbox input{border:none;background:transparent;color:var(--fg);font-size:12px;width:54px;padding:5px 0;outline:none;font-family:inherit;}
  .fbox svg{width:12px;height:12px;color:var(--dim);flex:none;}
  #dec-btn{width:36px;height:40px;display:inline-flex;align-items:center;justify-content:center;background:#0e2626;border:1px solid var(--accent);border-radius:8px;color:var(--accent);cursor:pointer;padding:0;}
  #dec-btn:hover{background:#123030;}
  #dec-btn svg{width:19px;height:19px;}
  .dtxn{padding-bottom:7px;margin-bottom:7px;border-bottom:1px solid var(--line);}
  .dtxn:last-child{border-bottom:none;margin-bottom:0;}
  .rcon{border:1px solid var(--bord);border-radius:10px;background:var(--panel);padding:9px 10px;margin-top:2px;}
  .rbanner{background:#0c0e12;border:1px solid var(--bord);border-radius:7px;padding:6px 9px;margin:4px 0 9px;font-family:ui-monospace,monospace;font-size:12px;color:var(--accent);}
  #seg-bus{flex:1;}
  #seg-bus .seg-b{flex:1;padding:8px 0;font-size:13px;}
  #dec-panel.dcol{flex:0 0 30px;}
  #dec-panel.dcol #dec-out,#dec-panel.dcol .fbox,#dec-panel.dcol #dec-clear,#dec-panel.dcol .slbl{display:none;}
  #dec-panel.dcol .dec-head{flex-direction:column;padding:6px 3px;border-bottom:none;}
  #dec-panel.dcol #dec-collapse svg{transform:rotate(180deg);}
  .dempty{color:var(--dim);text-align:center;padding:34px 12px;font-size:12px;line-height:1.7;}
  .dtx{margin-bottom:1px;}
  .dsep{border-top:1px dashed var(--line);margin:6px 0;}
  .drow{display:flex;gap:8px;align-items:baseline;}
  .dhx{flex:0 0 60px;white-space:nowrap;}
  .dmn{flex:1;min-width:0;color:#cfd4db;white-space:nowrap;overflow:hidden;text-overflow:ellipsis;}
  .d-ack{flex:0 0 auto;color:#56d364;}
  .d-nack{flex:0 0 auto;color:#f85149;}
  .d-start,.d-restart{color:#56d364;}
  .d-stop{color:#8b929c;}
  .d-addr{color:#6cb6ff;}
  .d-ctrl{color:#c9922f;}
  .d-cmd{color:#e3b341;}
  .d-data{color:#4dd8de;}
  .dsum{color:#8b929c;margin-top:2px;}
  .d-ts{color:#5f6570;font-size:11px;margin-bottom:2px;}
  .dnote{background:#241d12;border:0.5px solid #3a3020;border-radius:6px;color:#c8a860;font-size:11px;padding:6px 8px;margin-bottom:8px;line-height:1.5;}
  .d-note{color:#6b7280;font-size:10.5px;margin-bottom:1px;}
  .irow{display:flex;justify-content:space-between;align-items:baseline;gap:10px;font-size:12.5px;padding:3px 0;}
  .ik{color:var(--dim);}
  .iv{color:var(--fg);text-align:right;font-family:ui-monospace,monospace;}
  .ilive{font-size:9.5px;color:#34d3a6;border:1px solid #234a40;border-radius:20px;padding:1px 6px;margin-left:6px;vertical-align:1px;}
</style>
</head>
<body>
<header id="topbar">
  <div class="brand"><span class="bname">GCB</span><span class="bsub">Web I2C</span></div>
  <div class="pills">
    <span class="pill" id="st-ws"><span class="pdot"></span>WebSocket <b>disconnected</b></span>
    <span class="pill" id="st-bus"><span class="pdot"></span>Bus <b>--</b></span>
    <span class="pill" id="st-tap"><span class="pdot"></span>Tap <b>--</b></span>
    <span class="pill" id="st-dev"><span class="pdot"></span><b>--</b></span>
    <span class="pill" id="st-flood" style="display:none"><span class="pdot" style="background:#f6c177"></span><b>flooded</b></span>
    <span class="stat" style="margin-left:14px">Serial: <b id="ser">inactive</b></span>
  </div>
</header>
<div class="banner" id="ser-banner">
  Serial mirror unavailable: Web Serial requires a secure context. Over HTTP, open
  <code>chrome://flags/#unsafely-treat-insecure-origin-as-secure</code>, add the exact
  origin shown in the address bar (protocol, IP and port), then restart the
  browser. Chrome and Edge only.
</div>
<div class="wrap">
  <div class="main">
    <div class="card">
      <p class="lbl">SSD1306 display -- compose and send</p>
      <div class="row">
        <label>Display</label>
        <select id="oled-size">
          <option value="128x64">128 x 64</option>
          <option value="128x32">128 x 32</option>
          <option value="96x16">96 x 16</option>
          <option value="64x48">64 x 48</option>
          <option value="64x32">64 x 32</option>
        </select>
        <label style="min-width:auto">Addr</label>
        <input class="addr" id="oled-addr" placeholder="3C" value="3C">
        <label style="min-width:auto">Ctrl</label>
        <select id="oled-ctrl">
          <option value="ssd1306">SSD1306</option>
          <option value="sh1106">SH1106</option>
        </select>
      </div>
      <div class="oled-device">
        <div class="oled-pins">GND &nbsp; VCC &nbsp; SCL &nbsp; SDA</div>
        <div class="oled-screenframe"><canvas id="oledCanvas"></canvas></div>
        <div class="oled-bracket"></div>
      </div>
      <div class="drawbar">
        <button class="sm tsel" id="oled-draw">Draw</button>
        <button class="sm" id="oled-erase">Erase</button>
        <button class="sm" id="scr-fill">Fill</button>
        <button class="sm" id="scr-damier">Checkerboard</button>
        <span style="margin-left:auto"><button class="sm" id="oled-clear">Clear all</button></span>
      </div>
      <p class="hint" style="margin-bottom:8px">click or drag on the screen to draw / erase</p>
      <div class="cdiv"></div>
      <div class="row" style="margin-top:10px">
        <input type="file" id="oled-file" accept="image/*" style="display:none">
        <button class="go" id="oled-load">Load image</button>
        <span class="hint">convert a photo to 1-bit, or draw by hand above</span>
      </div>
      <div id="import-opts" style="display:none">
        <div class="row" style="margin-top:8px">
          <label style="min-width:auto">Threshold</label>
          <input type="range" id="oled-thresh" min="0" max="255" value="128" style="flex:1;min-width:90px">
          <span class="hint" id="oled-thresh-val" style="min-width:28px">128</span>
        </div>
        <div class="row">
          <label class="ck"><input type="checkbox" id="oled-dither" checked> Dither</label>
          <label class="ck"><input type="checkbox" id="oled-invert"> Invert</label>
          <label class="ck"><input type="checkbox" id="oled-fit" checked> Contain</label>
        </div>
        <p class="hint" style="margin:2px 0 0">Threshold = brightness cutoff for the loaded image: lower lights more pixels. Dither trades hard edges for simulated gradients.</p>
      </div>
      <div class="cdiv"></div>
      <div class="row" style="margin-top:10px">
        <button class="go" id="oled-send" style="flex:1">Send to display</button>
      </div>
      <div class="jbar" style="margin-top:6px">
        <p class="lbl" style="margin:0">Saved images</p>
        <button class="sm" id="oled-save">+ Save</button>
      </div>
      <div class="thumbs" id="oled-gallery"></div>
      <p class="hint" id="oled-galhint" style="margin-top:6px">no saved image (session memory)</p>
    </div>
    <div class="jbar">
      <p class="lbl" style="margin:0">Log</p>
      <div class="sertools">
        <span class="slbl">Serial mirror</span>
        <select id="baud" title="Serial port baud">
          <option value="9600">9600</option>
          <option value="19200">19200</option>
          <option value="38400">38400</option>
          <option value="57600">57600</option>
          <option value="115200" selected>115200</option>
          <option value="230400">230400</option>
          <option value="460800">460800</option>
          <option value="921600">921600</option>
        </select>
        <button id="ser-btn" class="go sm">Connect serial port</button>
      </div>
    </div>
    <div id="log-row">
      <div id="log-wrap">
        <div class="ptools">
          <button id="log-pause" class="ib" title="Pause"><svg viewBox="0 0 24 24" fill="currentColor"><rect x="6" y="5" width="4" height="14" rx="1"/><rect x="14" y="5" width="4" height="14" rx="1"/></svg></button>
          <button id="log-err" class="ib" title="Show errors only"><svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><path d="M12 4 L21 19 H3 Z"/><line x1="12" y1="10" x2="12" y2="14"/><line x1="12" y1="16.7" x2="12" y2="16.75"/></svg></button>
          <span class="fbox"><svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><path d="M3 5 H21 L14 13 V19 L10 21 V13 Z"/></svg><input id="log-filter" placeholder="filter" title="Show only lines containing this text"></span>
          <span class="fbox"><svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><circle cx="12" cy="12" r="6.5"/><line x1="12" y1="1.5" x2="12" y2="5.5"/><line x1="12" y1="18.5" x2="12" y2="22.5"/><line x1="1.5" y1="12" x2="5.5" y2="12"/><line x1="18.5" y1="12" x2="22.5" y2="12"/></svg><input id="trg-input" placeholder="trigger" title="Trigger on text / address / error -- e.g. NACK or 0x50"></span>
          <button id="trg-arm" class="ib" title="Arm trigger"><svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><circle cx="12" cy="12" r="6.5"/><line x1="12" y1="1.5" x2="12" y2="5.5"/><line x1="12" y1="18.5" x2="12" y2="22.5"/><line x1="1.5" y1="12" x2="5.5" y2="12"/><line x1="18.5" y1="12" x2="22.5" y2="12"/></svg></button>
          <span style="flex:1"></span>
          <button id="log-csv" class="ib go" title="Export as CSV"><svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linejoin="round"><rect x="3" y="4" width="18" height="16" rx="1"/><line x1="3" y1="9.5" x2="21" y2="9.5"/><line x1="3" y1="14.5" x2="21" y2="14.5"/><line x1="9" y1="9.5" x2="9" y2="20"/></svg></button>
          <button id="log-json" class="ib go" title="Export as JSON"><svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><path d="M9 4 Q5.5 4 5.5 8 Q5.5 12 3 12 Q5.5 12 5.5 16 Q5.5 20 9 20"/><path d="M15 4 Q18.5 4 18.5 8 Q18.5 12 21 12 Q18.5 12 18.5 16 Q18.5 20 15 20"/></svg></button>
          <button id="clear" class="ib" title="Clear log"><svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><path d="M4 7 H20"/><path d="M6 7 V19 A2 2 0 0 0 8 21 H16 A2 2 0 0 0 18 19 V7"/><path d="M9.5 7 V4 H14.5 V7"/></svg></button>
        </div>
        <div id="out"></div>
      </div>
      <div id="decode-mid">
        <button id="dec-btn" title="Decode the log into the decoder"><svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><line x1="4" y1="12" x2="18" y2="12"/><path d="M13 6 L19 12 L13 18"/></svg></button>
      </div>
      <div id="dec-panel">
        <div class="dec-head">
          <span class="slbl">Decoder</span>
          <span class="fbox"><svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><path d="M3 5 H21 L14 13 V19 L10 21 V13 Z"/></svg><input id="dec-filter" placeholder="filter" title="Show only decoded blocks containing this text"></span>
          <span style="flex:1"></span>
          <button id="dec-collapse" class="ib" title="Collapse / expand"><svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><path d="M9 6 L15 12 L9 18"/></svg></button>
          <button id="dec-clear" class="ib" title="Clear decoder"><svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><path d="M4 7 H20"/><path d="M6 7 V19 A2 2 0 0 0 8 21 H16 A2 2 0 0 0 18 19 V7"/><path d="M9.5 7 V4 H14.5 V7"/></svg></button>
        </div>
        <div id="dec-out"><div class="dempty">No decode yet<br>click &#8594; to decode the log</div></div>
      </div>
    </div>
  </div>
  <div class="side">
    <div class="tabs">
      <span class="tab" data-tab="brut">Commands</span>
      <span class="tab" data-tab="ecran">Display</span>
      <span class="tab" data-tab="init">Init</span>
      <span class="tab" data-tab="scroll">Scroll</span>
      <span class="tab on" data-tab="info">Info</span>
      <span class="tab" data-tab="system">System</span>
    </div>

    <div class="tabpanel" data-panel="brut" style="display:none">
      <p class="lbl">Raw commands</p>
      <div class="row">
        <button id="scan" class="go">Scan bus</button>
        <span class="hint">detects 0x08-0x77</span>
      </div>
      <div class="row">
        <label>Write</label>
        <input class="addr" id="w-addr" placeholder="3C" value="3C">
        <input class="data" id="w-data" placeholder="hex bytes e.g. 00 AF">
        <button id="w-go">W</button>
      </div>
      <div class="row">
        <label>Read</label>
        <input class="addr" id="r-addr" placeholder="3C" value="3C">
        <input class="len" id="r-len" placeholder="len" value="1">
        <button id="r-go">R</button>
      </div>
      <div class="row">
        <label>Write reg</label>
        <input class="addr" id="wr-addr" placeholder="3C" value="3C">
        <input class="reg" id="wr-reg" placeholder="reg">
        <input class="data" id="wr-data" placeholder="bytes">
        <button id="wr-go">WR</button>
      </div>
      <div class="row">
        <label>Read reg</label>
        <input class="addr" id="rr-addr" placeholder="3C" value="3C">
        <input class="reg" id="rr-reg" placeholder="reg">
        <input class="len" id="rr-len" placeholder="len" value="1">
        <button id="rr-go">RR</button>
      </div>
      <div class="row">
        <label>Read as</label>
        <select id="read-fmt">
          <option value="hex">Hex</option>
          <option value="dec">Decimal</option>
          <option value="bin">Binary</option>
          <option value="ascii">ASCII</option>
        </select>
        <span class="hint">applies to R / RR results</span>
      </div>
      <div class="row">
        <label>Dump</label>
        <input class="addr" id="dump-addr" placeholder="3C" value="3C">
        <button id="dump-go" class="go">Registers 0x00-0xFF</button>
      </div>
      <div id="dump-grid" class="dgrid"></div>
    </div>

    <div class="tabpanel" data-panel="ecran" style="display:none">
      <p class="lbl">SSD1306 -- addr = Addr field</p>
      <div class="pair"><button data-cmd="00 AF">Display ON</button><button data-cmd="00 AE">Display OFF</button></div>
      <div class="pair"><button data-cmd="00 A5">All on</button><button data-cmd="00 A4">RAM (normal)</button></div>
      <div class="pair"><button data-cmd="00 A6">Normal</button><button data-cmd="00 A7">Inverted</button></div>
      <button class="go" id="oled-cp">Charge pump ON</button>
      <div class="sl"><span class="slbl">Contrast</span><input type="range" id="ct-slider" min="0" max="255" value="127"><span class="sval" id="ct-val">7F</span></div>
      <div class="pair"><button id="flip-h">Flip H</button><button id="flip-v">Flip V</button></div>
    </div>

    <div class="tabpanel" data-panel="init" style="display:none">
      <p class="lbl">SSD1306 -- addr = Addr field</p>
      <button class="go" data-cmd="00 AE D5 80 A8 3F D3 00 40 8D 14 20 00 A1 C8 DA 12 81 CF D9 F1 DB 40 A4 A6 2E AF">Init 128 x 64</button>
      <button class="go" data-cmd="00 AE D5 80 A8 1F D3 00 40 8D 14 20 00 A1 C8 DA 02 81 CF D9 F1 DB 40 A4 A6 2E AF">Init 128 x 32</button>
      <button class="go" data-cmd="00 AE D5 80 A8 3F D3 00 40 AD 8B A1 C8 DA 12 81 CF D9 22 DB 40 A4 A6 AF">Init SH1106 128 x 64</button>
      <button data-cmd="SCAN">Scan bus (SCAN)</button>
      <p class="lbl" style="margin-top:12px">Control bytes</p>
      <div style="font-size:12px;color:var(--dim);">
        <span class="chip" data-ins="00">00 = command</span>
        <span class="chip" data-ins="40">40 = data (pixels)</span>
      </div>
    </div>

    <div class="tabpanel" data-panel="scroll" style="display:none">
      <p class="lbl">SSD1306 -- addr = Addr field</p>
      <div class="pair"><button data-cmd="00 2E 26 00 00 00 07 00 FF 2F">Scroll right</button><button data-cmd="00 2E 27 00 00 00 07 00 FF 2F">Scroll left</button></div>
      <button class="go" data-cmd="00 2E">Stop scroll</button>
      <p class="hint">horizontal scroll. "Stop" before rewriting RAM. (Diagonal is unreliable across modules, removed.)</p>
    </div>

    <div class="tabpanel" data-panel="info">
      <svg id="gauges" viewBox="0 0 300 118" role="img" aria-label="Live gauges: observed per second, loop rate, free heap" style="width:100%;max-width:340px;display:block;margin:2px auto 6px"></svg>
      <div style="display:flex;justify-content:center;gap:12px;font-size:10px;color:var(--dim);margin:0 auto 8px;max-width:340px">
        <span><span style="display:inline-block;width:7px;height:7px;border-radius:50%;background:#5dcaa5;margin-right:3px"></span>ok</span>
        <span><span style="display:inline-block;width:7px;height:7px;border-radius:50%;background:#f6c177;margin-right:3px"></span>busy</span>
        <span><span style="display:inline-block;width:7px;height:7px;border-radius:50%;background:#e06c75;margin-right:3px"></span>saturated</span>
      </div>
      <svg id="wiring" viewBox="0 0 300 96" role="img" aria-label="Live signal path between the component, tap, bus, and mirror" style="width:100%;max-width:340px;display:block;margin:0 auto 10px"></svg>
      <div class="rcon">
        <p class="lbl" style="margin-top:0">Monitor &amp; control <span class="ilive">live</span></p>
        <div class="rbanner"><span id="i-tapmode">--</span></div>
        <div class="irow"><span class="ik">Compression</span><span class="iv" id="i-batch">--</span></div>
        <p class="rgrp">LOGGING</p>
      <div class="seg-row">
        <span class="seg-lbl">Filter</span>
        <input class="addr" id="tap-filter" value="3C" style="width:46px;padding:5px 7px">
        <span class="seg-lbl" style="min-width:0">blank=all</span>
      </div>
      <div class="seg-row">
        <span class="seg-lbl">Detail</span>
        <div class="seg">
          <button class="seg-b" id="tap-sum-btn">Summary</button>
          <button class="seg-b" id="tap-full-btn">Full</button>
        </div>
      </div>
      <div class="seg-row">
        <span class="seg-lbl">Delivery</span>
        <div class="seg">
          <button class="seg-b" id="tap-live-btn">Live</button>
          <button class="seg-b" id="tap-batch-btn">Batch</button>
        </div>
      </div>
        <p class="rgrp">BUS CONTROL</p>
      <div class="seg-row">
        <span class="seg-lbl">Bus</span>
        <div class="seg" id="seg-bus">
          <button class="seg-b" id="tap-relay-btn">Relay</button>
          <button class="seg-b" id="tap-takeover-btn">Take over</button>
        </div>
      </div>
        <p class="rgrp">SCREEN MIRROR</p>
      <div class="seg-row">
        <span class="seg-lbl">Screen</span>
        <div class="seg">
          <button class="seg-b" id="tap-mirroroff-btn">Off</button>
          <button class="seg-b" id="tap-mirror-btn">Mirror</button>
        </div>
      </div>
      <canvas id="fb-canvas" width="128" height="64" title="reconstructed from the bus"></canvas>
      </div>
      <p class="hint">full data streams the bytes of command writes (decodable); bulk/pixel writes are counted only (not decodable, and would flood). Summary = byte-count only. Batch coalesces + dedupes to ~5 msg/s. Addr blank = all.</p>
      <p class="hint">Take over stops relaying device traffic to the bus, so your terminal commands own it -- compose on the display without its component overwriting you. Relay restores normal operation.</p>
      <p class="hint">Mirror reconstructs the display's framebuffer from the pixel writes on the bus and draws it below -- what the screen actually receives, rebuilt from I2C. Assumes a full sequential refresh (as ESPHome's ssd1306 does).</p>
    </div>

    <div class="tabpanel" data-panel="system" style="display:none">
      <p class="lbl">I2C bus</p>
      <div class="irow"><span class="ik">Frequency</span><span class="iv" id="i-freq">--</span></div>
      <div class="irow"><span class="ik">SDA pin</span><span class="iv" id="i-sda">--</span></div>
      <div class="irow"><span class="ik">SCL pin</span><span class="iv" id="i-scl">--</span></div>
      <div class="irow"><span class="ik">Scan on boot</span><span class="iv" id="i-scan">--</span></div>
      <div class="irow"><span class="ik">Internal pull-ups</span><span class="iv" id="i-pullup">--</span></div>
      <p class="lbl" style="margin-top:12px">Bus monitor <span class="ilive">live</span></p>
      <div class="irow"><span class="ik">Tap status</span><span class="iv" id="i-tapstatus">--</span></div>
      <div class="irow"><span class="ik">Detected</span><span class="iv" id="i-attached">--</span></div>
      <div class="irow"><span class="ik">Observed</span><span class="iv" id="i-observed">--</span></div>
      <div class="irow"><span class="ik">Observed/s</span><span class="iv" id="i-obsrate">--</span></div>
      <div class="irow"><span class="ik">Log lines skipped</span><span class="iv" id="i-dropped">--</span></div>
      <p class="hint" style="margin:1px 0 0;color:#8fae94">Bus data is never lost -- only the log display is throttled when the ESP is busy.</p>
      <div class="irow"><span class="ik">Loop rate</span><span class="iv" id="i-loophz">--</span></div>
      <div class="irow"><span class="ik">Deferred at boot</span><span class="iv" id="i-bootheld">--</span></div>
      <p class="lbl" style="margin-top:12px">Bus lines <span class="ilive">live</span></p>
      <div class="irow"><span class="ik">SDA idle level</span><span class="iv" id="i-sdalvl">--</span></div>
      <div class="irow"><span class="ik">SCL idle level</span><span class="iv" id="i-scllvl">--</span></div>
      <div class="irow"><span class="ik">Bus status</span><span class="iv" id="i-busstatus">--</span></div>
      <p class="hint">LOW at idle = a line is held (stuck device or missing pull-up).</p>
      <button class="go sm" id="recover-btn" style="margin-top:2px">Recover bus</button>
      <p class="lbl" style="margin-top:12px">Memory <span class="ilive">live</span></p>
      <div class="irow"><span class="ik">Free heap</span><span class="iv" id="i-heap">--</span></div>
      <div class="irow"><span class="ik">Min free heap</span><span class="iv" id="i-minheap">--</span></div>
      <p class="lbl" style="margin-top:12px">Device</p>
      <div class="irow"><span class="ik">Chip</span><span class="iv" id="i-mcu">--</span></div>
      <div class="irow"><span class="ik">Framework</span><span class="iv" id="i-fw">--</span></div>
      <div class="irow"><span class="ik">Last reset</span><span class="iv" id="i-reset">--</span></div>
      <div class="irow"><span class="ik">Uptime <span class="ilive">live</span></span><span class="iv" id="i-uptime">--</span></div>
      <p class="lbl" style="margin-top:12px">Server</p>
      <div class="irow"><span class="ik">WebSocket port</span><span class="iv" id="i-port">--</span></div>
      <div class="irow"><span class="ik">Client <span class="ilive">live</span></span><span class="iv" id="i-client">--</span></div>
    </div>
  </div>
</div>
<script>
(function(){
  "use strict";
  var $=function(id){return document.getElementById(id);};
  var out=$('out'), ws=null;
  var LOGMAX=2000, logBuf=[], logPaused=false, logSkipped=0, logFilter='', logErr=false;
  var floodPrev=0, lastDropTime=0;
  var lastTxUs=null, trgOn=false, trgStr='', trgHit=null;
  function fmtDelta(us){ if(us<1000) return '+'+us+'\u00b5s'; if(us<1000000) return '+'+(us/1000).toFixed(1)+'ms'; return '+'+(us/1000000).toFixed(2)+'s'; }
  function tapKind(t){ return t.kind||((t.dir==='R'||t.dir==='RR')?'read':'cmd'); }
  function tapLine(t){ var k=tapKind(t),body;
    if(t.data!==undefined&&t.data!==''){ body=k+'  '+t.data+((t.count>1)?('  \u00d7'+t.count):''); }
    else { var sz=(t.count>1)?(t.count+' \u00d7 '+t.n+' B'):(t.n+' B'); body=k+'  '+sz; }
    return '\u00bb '+t.dir+' 0x'+t.addr+'  '+body+'  '+t.res; }
  function tapTxn(t){ var k=tapKind(t),ad=(''+t.addr).toUpperCase();
    var bts=(t.data!==undefined&&t.data!=='')?t.data.trim().split(/\s+/):[];
    if(t.dir==='R'||t.dir==='RR') return {kw:'R',toks:['R',ad,String(t.n)],res:t.res,data:(t.data!==undefined?t.data:null),scan:[],kind:k,n:t.n,count:t.count||1};
    return {kw:'W',toks:['W',ad].concat(bts),res:t.res,data:null,scan:[],kind:k,n:t.n,count:t.count||1}; }
  function setPill(id,color,txt){ var p=$(id); if(!p)return; p.style.display=''; var d=p.querySelector('.pdot'); if(d)d.style.background=color; var b=p.querySelector('b'); if(b)b.textContent=txt; }

  function logLineEl(e){ var d=document.createElement('div'); d.className=e.cls+(e===trgHit?' trghit':''); d.dataset.raw=e.txt; if(e.txn) d.dataset.txn=JSON.stringify(e.txn);
    var dt=(e.dus!==undefined)?fmtDelta(e.dus):''; d.dataset.dt=dt;
    var ts=document.createElement('span'); ts.className='lts'; ts.textContent=dt;
    d.appendChild(ts); d.appendChild(document.createTextNode(e.txt)); return d; }
  function logMatch(e){
    if(logErr && !/l-err/.test(e.cls) && !/NACK|TIMEOUT|BUS_ERROR|CRC|BAD_ARG|NOT_INIT|TOO_LARGE|UNKNOWN/.test(e.txt)) return false;
    if(logFilter && e.txt.toLowerCase().indexOf(logFilter)<0) return false;
    return true;
  }
  function log(cls,txt,us,txn){
    var e={t:Date.now(),cls:cls,txt:txt};
    if(typeof us==='number'){ e.us=us; if(lastTxUs!==null) e.dus=(us-lastTxUs)>>>0; lastTxUs=us; }
    if(txn) e.txn=txn;
    logBuf.push(e); if(logBuf.length>LOGMAX) logBuf.shift();
    var fired=false;
    if(trgOn && trgStr && txt.toLowerCase().indexOf(trgStr)>=0){ trgHit=e; logPaused=true; trgOn=false; fired=true; }
    if(fired){ renderLog(); updTrgBtn(); updPauseBtn(); }
    else if(logPaused){ logSkipped++; updPauseBtn(); }
    else if(logMatch(e)){ out.appendChild(logLineEl(e)); out.scrollTop=out.scrollHeight; while(out.childNodes.length>LOGMAX) out.removeChild(out.firstChild); }
    serMirror((e.dus!==undefined?fmtDelta(e.dus)+' ':'')+txt);
  }
  function renderLog(){
    out.textContent=''; var frag=document.createDocumentFragment();
    for(var i=0;i<logBuf.length;i++){ if(logMatch(logBuf[i])||logBuf[i]===trgHit) frag.appendChild(logLineEl(logBuf[i])); }
    out.appendChild(frag); out.scrollTop=out.scrollHeight;
  }
  function updTrgBtn(){ var b=$('trg-arm'); if(!b)return; b.classList.remove('armed','fired');
    if(trgHit) b.classList.add('fired'); else if(trgOn) b.classList.add('armed');
    b.title=trgHit?'Triggered -- click to re-arm':(trgOn?'Armed, watching (click to disarm)':'Arm trigger on the text/address/error above'); }
  function trgClick(){ if(trgHit){ trgHit=null; logPaused=false; logSkipped=0; trgOn=!!trgStr; renderLog(); updPauseBtn(); } else { trgOn=(!trgOn)&&!!trgStr; } updTrgBtn(); }
  var ICO_PAUSE='<svg viewBox="0 0 24 24" fill="currentColor"><rect x="6" y="5" width="4" height="14" rx="1"/><rect x="14" y="5" width="4" height="14" rx="1"/></svg>';
  var ICO_PLAY='<svg viewBox="0 0 24 24" fill="currentColor"><path d="M7 5 L19 12 L7 19 Z"/></svg>';
  function updPauseBtn(){ var b=$('log-pause'); if(!b)return;
    if(logPaused){ b.innerHTML=ICO_PLAY+(logSkipped>0?'<span style="font-size:11px;margin-left:3px;font-weight:500;font-family:ui-sans-serif,sans-serif">'+logSkipped+'</span>':''); b.style.width='auto'; b.style.padding='0 7px'; }
    else { b.innerHTML=ICO_PAUSE; b.style.width=''; b.style.padding=''; }
    b.classList.toggle('act',logPaused); b.title=logPaused?('Resume -- '+logSkipped+' new line(s) buffered while paused'):'Pause'; }
  function parseLog(t){
    var r={dir:'',addr:'',bytes:'',result:'',count:''};
    var cm=t.match(/\s+x(\d+)\s*$/); if(cm){ r.count=cm[1]; t=t.replace(/\s+x\d+\s*$/,''); }
    var m=t.match(/^\u00bb\s+(\w+)\s+0x([0-9A-Fa-f]{1,2})\s*:\s*([0-9A-Fa-f ]*?)\s+(\S+)\s*$/);
    if(m){ r.dir=m[1]; r.addr=m[2]; r.bytes=m[3].trim(); r.result=m[4]; return r; }
    m=t.match(/^\u00bb\s+(\w+)\s+0x([0-9A-Fa-f]{1,2})\s*\((\d+)\s*o\)\s+(\S+)/);
    if(m){ r.dir=m[1]; r.addr=m[2]; r.result=m[4]; return r; }
    m=t.match(/^(\w+)\s+0x([0-9A-Fa-f]{1,2}).*->\s*(\w+)(?:\s*:\s*([0-9A-Fa-f ]+))?/);
    if(m){ r.dir=m[1]; r.addr=m[2]; r.result=m[3]; if(m[4])r.bytes=m[4].trim(); return r; }
    m=t.match(/^>\s+(\w+)\s+([0-9A-Fa-f]{1,2})\s+([0-9A-Fa-f ]+)/);
    if(m){ r.dir=m[1]; r.addr=m[2].toUpperCase(); r.bytes=m[3].trim(); return r; }
    return r;
  }
  function csvq(s){ s=(s==null)?'':(''+s); return (/[",\n]/.test(s))?('"'+s.replace(/"/g,'""')+'"'):s; }
  function dlFile(name,mime,content){ var b=new Blob([content],{type:mime}); var u=URL.createObjectURL(b); var a=document.createElement('a'); a.href=u; a.download=name; document.body.appendChild(a); a.click(); a.remove(); setTimeout(function(){URL.revokeObjectURL(u);},1000); }
  function logExport(fmt){
    if(!logBuf.length){ log('l-sys','log is empty -- nothing to export'); return; }
    var stamp=new Date().toISOString().replace(/[:.]/g,'-').slice(0,19);
    function fld(e){ if(e.txn){ var t=e.txn; return {dir:t.kw,addr:t.toks[1]||'',bytes:(t.kw==='R'?(t.data||''):t.toks.slice(2).join(' ')),result:t.res,count:(t.count>1?String(t.count):'')}; } return parseLog(e.txt); }
    if(fmt==='csv'){
      var rows=['arrival,esp_us,delta_ms,dir,addr,bytes,result,count,raw'];
      for(var i=0;i<logBuf.length;i++){ var e=logBuf[i],p=fld(e); rows.push([new Date(e.t).toISOString(),(e.us!==undefined?e.us:''),(e.dus!==undefined?(e.dus/1000).toFixed(3):''),p.dir,p.addr,csvq(p.bytes),p.result,p.count,csvq(e.txt)].join(',')); }
      dlFile('web_i2c_'+stamp+'.csv','text/csv',String.fromCharCode(0xFEFF)+rows.join('\n'));
    } else {
      var arr=logBuf.map(function(e){ var p=fld(e); return {arrival:new Date(e.t).toISOString(),esp_us:(e.us!==undefined?e.us:null),delta_ms:(e.dus!==undefined?e.dus/1000:null),dir:p.dir,addr:p.addr,bytes:p.bytes,result:p.result,count:p.count,raw:e.txt}; });
      dlFile('web_i2c_'+stamp+'.json','application/json',JSON.stringify(arr,null,1));
    }
    log('l-sys','exported '+logBuf.length+' line(s) as '+fmt.toUpperCase());
  }

  function connectWS(){
    var url=(location.protocol==='https:'?'wss://':'ws://')+location.host+'/';
    ws=new WebSocket(url);
    ws.onopen=function(){ setPill('st-ws','#5dcaa5','connected'); };
    ws.onclose=function(){ setPill('st-ws','#e06c75','disconnected'); oledSending=false;oledQueue=[];setTimeout(connectWS,1500);};
    ws.onmessage=function(ev){
      var m; try{m=JSON.parse(ev.data);}catch(e){log('l-sys',ev.data);return;}
      if(m.t==='hello'){log('l-sys',m.msg);}
      else if(m.t==='err'){log('l-err','ERROR: '+m.msg);}
      else if(m.t==='scan'){
        if(m.count===0){log('l-err','SCAN: no device found');}
        else{
          log('l-ok','SCAN: '+m.count+' device(s) found');
          m.found.forEach(function(a){ var h=I2C_HINTS[a.toUpperCase()]; log('l-ok','  0x'+a+(h?('  ('+h+')'):'')); });
        }
      }
      else if(m.t==='write'){log(m.res==='OK'?'l-ok':'l-err','W 0x'+m.addr+' ('+m.n+' B) -> '+m.res);if(oledSending)oledSendNext();}
      else if(m.t==='read'){log(m.res==='OK'?'l-ok':'l-err','R 0x'+m.addr+' -> '+m.res+(m.data?(' : '+fmtBytes(m.data,readFmt)):''));}
      else if(m.t==='writereg'){log(m.res==='OK'?'l-ok':'l-err','WR 0x'+m.addr+' reg 0x'+m.reg+' -> '+m.res);}
      else if(m.t==='readreg'){
        if(dumping){ var dr=parseInt(m.reg,16); dumpData[dr]=(m.res==='OK'&&m.data)?(parseInt(m.data.trim().split(/\s+/)[0],16)&0xFF):null; dumpSet(dr,dumpData[dr]); dumpReg=dr+1; dumpNext(); }
        else log(m.res==='OK'?'l-ok':'l-err','RR 0x'+m.addr+' reg 0x'+m.reg+' -> '+m.res+(m.data?(' : '+fmtBytes(m.data,readFmt)):''));
      }
      else if(m.t==='info'){ updInfo(m); }
      else if(m.t==='tap'){ log(m.kind==='data'?'l-tapd':'l-tap', tapLine(m), m.us, tapTxn(m)); }
      else if(m.t==='tapbatch'){ (m.txns||[]).forEach(function(x){ log(x.kind==='data'?'l-tapd':'l-tap', tapLine(x), x.us, tapTxn(x)); }); }
      else if(m.t==='recover'){ log(m.sda===1?'l-ok':'l-err', m.sda===1?('Bus recovery: SDA released after '+m.pulses+' pulse(s)'):(m.sda===0?'Bus recovery: SDA still LOW -- hardware fault (pull-up or short)':'Bus recovery: no pins configured')); }
      else if(m.t==='fb'){ renderFb(m.data); }
    };
  }

  function send(cmd){ if(ws&&ws.readyState===1){ log('l-tx','> '+cmd); ws.send(cmd); } else { log('l-err','WebSocket not connected'); } }

  $('scan').onclick=function(){send('SCAN');};
  $('w-go').onclick=function(){send('W '+$('w-addr').value+' '+$('w-data').value);};
  $('r-go').onclick=function(){send('R '+$('r-addr').value+' '+$('r-len').value);};
  $('wr-go').onclick=function(){send('WR '+$('wr-addr').value+' '+$('wr-reg').value+' '+$('wr-data').value);};
  $('rr-go').onclick=function(){send('RR '+$('rr-addr').value+' '+$('rr-reg').value+' '+$('rr-len').value);};
  $('clear').onclick=function(){ out.textContent=''; logBuf=[]; logSkipped=0; lastTxUs=null; if(trgHit){ trgHit=null; trgOn=!!trgStr; updTrgBtn(); } if(logPaused){ logPaused=false; updPauseBtn(); } };
  $('log-pause').onclick=function(){ logPaused=!logPaused; if(!logPaused){ logSkipped=0; renderLog(); } updPauseBtn(); };
  $('log-filter').oninput=function(){ logFilter=this.value.trim().toLowerCase(); renderLog(); };
  $('log-err').onclick=function(){ logErr=!logErr; this.classList.toggle('act',logErr); renderLog(); };
  $('log-csv').onclick=function(){ logExport('csv'); };
  $('log-json').onclick=function(){ logExport('json'); };
  $('trg-arm').onclick=trgClick;
  $('trg-input').oninput=function(){ trgStr=this.value.trim().toLowerCase(); if(!trgStr) trgOn=false; updTrgBtn(); };
  $('dump-go').onclick=dumpStart;
  $('read-fmt').onchange=function(){ readFmt=$('read-fmt').value; };

  // --- tabbed SSD1306 panel ---
  // SSD1306 button address = the composer's existing Addr field (no hardcoded
  // 3C: some panels are not at 0x3C)
  function oledAddr(){ return ($('oled-addr').value||'3C').trim(); }
  function oledCtrl(){ return $('oled-ctrl').value; }
  // charge pump / DC-DC differs by controller: SSD1306 = 8D 14, SH1106 = AD 8B
  $('oled-cp').onclick=function(){ send('W '+oledAddr()+' 00 '+(oledCtrl()==='sh1106'?'AD 8B AF':'8D 14 AF')); };

  // direct-command buttons (data-cmd): "SCAN" as-is, otherwise prefix
  // "W <addr> " + the command body.
  document.querySelectorAll('button[data-cmd]').forEach(function(b){
    b.onclick=function(){
      var c=b.getAttribute('data-cmd');
      send(c==='SCAN' ? 'SCAN' : ('W '+oledAddr()+' '+c));
    };
  });

  // tabs: show the matching panel
  document.querySelectorAll('.tab[data-tab]').forEach(function(t){
    t.onclick=function(){
      var name=t.getAttribute('data-tab');
      document.querySelectorAll('.tab[data-tab]').forEach(function(x){x.className=(x===t)?'tab on':'tab';});
      document.querySelectorAll('.tabpanel[data-panel]').forEach(function(p){p.style.display=(p.getAttribute('data-panel')===name)?'':'none';});
    };
  });

  // Contrast: live label (input), send ONLY on release
  // (change) to avoid a burst of 81 vv while dragging.
  var ctS=$('ct-slider'), ctV=$('ct-val');
  ctS.oninput=function(){ ctV.textContent=hx(parseInt(ctS.value,10)); };
  ctS.onchange=function(){ send('W '+oledAddr()+' 00 81 '+hx(parseInt(ctS.value,10))); };

  // Flip H (A0/A1) and V (C0/C8): toggle on each click
  var flipH=false, flipV=false;
  $('flip-h').onclick=function(){ flipH=!flipH; send('W '+oledAddr()+' 00 '+(flipH?'A1':'A0')); };
  $('flip-v').onclick=function(){ flipV=!flipV; send('W '+oledAddr()+' 00 '+(flipV?'C8':'C0')); };

  // Content: full framebuffer sent via the paced sender
  $('scr-fill').onclick=function(){ oledPix=new Uint8Array(oledW*oledH).fill(1); oledSrcImg=null; oledSrcData=null; oledDraw(); };
  $('scr-damier').onclick=function(){
    oledPix=new Uint8Array(oledW*oledH); oledSrcImg=null; oledSrcData=null;
    for(var y=0;y<oledH;y++) for(var x=0;x<oledW;x++) oledPix[y*oledW+x]=((Math.floor(x/8)+Math.floor(y/8))%2===0)?1:0;
    oledDraw();
  };

  // insert control byte into the write field
  document.querySelectorAll('.chip[data-ins]').forEach(function(c){
    c.onclick=function(){var f=$('w-data');f.value=(f.value?f.value+' ':'')+c.getAttribute('data-ins');f.focus();};
  });

  // Enter in the data fields -> send
  $('w-data').addEventListener('keydown',function(e){if(e.key==='Enter')$('w-go').click();});

  // ==================================================================
  //  SSD1306 display: compose a 1 bpp image in the browser and push it
  //  to the real display via I2C writes (W <addr> 40 ...). Canvas
  //  rendering identical to oled_stream: SCALE=4, GAP=1, yellow on the
  //  top 16 rows, cyan below. GDDRAM is write-only: the preview shows
  //  what will be sent, never the real screen (a SSD1306's GDDRAM
  //  cannot be read back over I2C).
  // ==================================================================
  var OLED_SCALE=4, OLED_GAP=1, OLED_BLK=64;  // 64 bytes/block: OK ESP8266+ESP32
  var oledW=128, oledH=64, oledPix=null, oledSrcImg=null, oledSrcData=null;
  var oledCv=$('oledCanvas'), octx=oledCv.getContext('2d');
  var oledGallery=[];
  octx.imageSmoothingEnabled=false;

  function hx(n){return (n<16?'0':'')+(n&0xFF).toString(16).toUpperCase();}

  // --- I2C explorer helpers (English UI) ---
  // likely device names by 7-bit address (hints only, not authoritative)
  var I2C_HINTS={
    '3C':'SSD1306 / SH1106 OLED','3D':'SSD1306 OLED',
    '27':'PCF8574 (LCD backpack)','3F':'PCF8574A (LCD backpack)',
    '68':'MPU6050 / DS3231 / DS1307','69':'MPU6050 (AD0=1)',
    '76':'BME280 / BMP280','77':'BME280 / BMP280 / BMP180',
    '48':'ADS1115 / PCF8591 / LM75','40':'INA219 / PCA9685 / HTU21D',
    '50':'AT24Cxx EEPROM','57':'AT24Cxx EEPROM',
    '1E':'HMC5883L','0D':'QMC5883L','20':'MCP23017 / PCF8574',
    '29':'VL53L0X / TSL2561','23':'BH1750','5A':'MLX90614 / CCS811',
    '70':'TCA9548A (I2C mux)'
  };

  // read display format for R / RR results
  var readFmt='hex';
  function fmtBytes(hexStr,fmt){
    if(!hexStr) return '';
    var parts=hexStr.trim().split(/\s+/), bytes=[];
    for(var i=0;i<parts.length;i++){ var v=parseInt(parts[i],16); if(!isNaN(v)) bytes.push(v&0xFF); }
    if(fmt==='dec') return bytes.map(function(b){return b.toString(10);}).join(' ');
    if(fmt==='bin') return bytes.map(function(b){return ('0000000'+b.toString(2)).slice(-8);}).join(' ');
    if(fmt==='ascii') return bytes.map(function(b){return (b>=32&&b<127)?String.fromCharCode(b):'.';}).join('');
    return bytes.map(function(b){return hx(b);}).join(' ');
  }

  // --- register dump 0x00..0xFF: one RR read at a time, paced by responses ---
  // (grid is the output; the per-register reads are sent WITHOUT journal spam)
  var dumpAddr='', dumpReg=0, dumping=false, dumpData=new Array(256);
  function renderDumpGrid(){
    var g=$('dump-grid'), h='<div class="dcell dhdr"></div>';
    for(var c=0;c<16;c++) h+='<div class="dcell dhdr">'+c.toString(16).toUpperCase()+'</div>';
    for(var r=0;r<16;r++){
      h+='<div class="dcell dhdr">'+r.toString(16).toUpperCase()+'0</div>';
      for(var k=0;k<16;k++) h+='<div class="dcell" id="dc'+(r*16+k)+'"><span class="dnull">..</span></div>';
    }
    g.innerHTML=h;
  }
  function dumpSet(reg,v){ var e=$('dc'+reg); if(e) e.innerHTML=(v==null)?'<span class="dnull">--</span>':hx(v); }
  function dumpStart(){
    if(dumping){ log('l-err','Dump already running'); return; }
    if(!(ws&&ws.readyState===1)){ log('l-err','WebSocket not connected'); return; }
    dumpAddr=($('dump-addr').value||'3C').trim();
    dumpData=new Array(256); dumpReg=0; dumping=true;
    log('l-sys','Register dump 0x00-0xFF @ 0x'+dumpAddr);
    renderDumpGrid();
    dumpNext();
  }
  function dumpNext(){
    if(dumpReg>0xFF){ dumping=false; log('l-sys','Dump complete'); return; }
    if(ws&&ws.readyState===1) ws.send('RR '+dumpAddr+' '+hx(dumpReg)+' 1');
    else { dumping=false; log('l-err','WebSocket lost during dump'); }
  }

  // same rendering as oled_stream (pixel = SCALE-GAP, yellow top / cyan bottom)
  function oledDraw(){
    var io=$('import-opts'); if(io) io.style.display=oledSrcData?'':'none';
    octx.fillStyle='#000000';
    octx.fillRect(0,0,oledCv.width,oledCv.height);
    for(var y=0;y<oledH;y++){
      for(var x=0;x<oledW;x++){
        if(oledPix[y*oledW+x]){
          octx.fillStyle=(y<16)?'#F5C518':'#29E5FF';
          octx.fillRect(x*OLED_SCALE,y*OLED_SCALE,OLED_SCALE-OLED_GAP,OLED_SCALE-OLED_GAP);
        }
      }
    }
  }

  function oledSetSize(){
    var p=$('oled-size').value.split('x');
    oledW=parseInt(p[0],10); oledH=parseInt(p[1],10);
    oledCv.width=oledW*OLED_SCALE; oledCv.height=oledH*OLED_SCALE;
    octx.imageSmoothingEnabled=false;
    oledPix=new Uint8Array(oledW*oledH);
    if(oledRasterize()) oledConvert(); else oledDraw();
  }

  function oledError(){
    octx.fillStyle='#000000'; octx.fillRect(0,0,oledCv.width,oledCv.height);
    octx.fillStyle='#e06c75'; octx.font='14px monospace';
    octx.fillText('unreadable image',8,20);
    log('l-err','Image: unrecognized format or unreadable file');
  }

  function oledLoad(file){
    var img=new Image();
    img.onload=function(){
      oledSrcImg=img;
      if(oledRasterize()){ oledConvert(); log('l-sys','Image loaded: '+img.width+'x'+img.height+' -> '+oledW+'x'+oledH); }
    };
    img.onerror=function(){ oledSrcImg=null; oledSrcData=null; oledError(); };
    var fr=new FileReader();
    fr.onload=function(){ img.src=fr.result; };
    fr.onerror=function(){ oledSrcImg=null; oledSrcData=null; oledError(); };
    fr.readAsDataURL(file);
  }

  // draw the source image into an offscreen canvas at the target size
  function oledRasterize(){
    if(!oledSrcImg) return false;
    var off=document.createElement('canvas'); off.width=oledW; off.height=oledH;
    var c=off.getContext('2d'); c.imageSmoothingEnabled=true;
    c.fillStyle='#000000'; c.fillRect(0,0,oledW,oledH);
    var iw=oledSrcImg.width, ih=oledSrcImg.height, dx=0, dy=0, dw=oledW, dh=oledH;
    if($('oled-fit').checked){  // contain: keep aspect ratio, center
      var s=Math.min(oledW/iw, oledH/ih);
      dw=Math.max(1,Math.round(iw*s)); dh=Math.max(1,Math.round(ih*s));
      dx=Math.floor((oledW-dw)/2); dy=Math.floor((oledH-dh)/2);
    }
    try{ c.drawImage(oledSrcImg,dx,dy,dw,dh); oledSrcData=c.getImageData(0,0,oledW,oledH); }
    catch(e){ oledSrcData=null; oledError(); return false; }
    return true;
  }

  // RGBA -> luminance -> threshold or dithering (Floyd-Steinberg) -> 1 bpp.
  // Transparent pixels become off (composited on black).
  function oledConvert(){
    if(!oledSrcData){ oledDraw(); return; }
    var d=oledSrcData.data, n=oledW*oledH, g=new Float32Array(n), i;
    for(i=0;i<n;i++){
      var a=d[i*4+3]/255;
      g[i]=(0.299*d[i*4]+0.587*d[i*4+1]+0.114*d[i*4+2])*a;
    }
    var thr=parseInt($('oled-thresh').value,10);
    var inv=$('oled-invert').checked?1:0;
    if($('oled-dither').checked){
      for(var y=0;y<oledH;y++){
        for(var x=0;x<oledW;x++){
          var k=y*oledW+x, old=g[k], nv=old<thr?0:255, err=old-nv; g[k]=nv;
          if(x+1<oledW) g[k+1]+=err*7/16;
          if(y+1<oledH){ if(x>0) g[k+oledW-1]+=err*3/16; g[k+oledW]+=err*5/16; if(x+1<oledW) g[k+oledW+1]+=err*1/16; }
        }
      }
      for(i=0;i<n;i++) oledPix[i]=((g[i]>127?1:0)^inv);
    }else{
      for(i=0;i<n;i++) oledPix[i]=((g[i]>=thr?1:0)^inv);
    }
    oledDraw();
  }

  // pixels -> SSD1306 GDDRAM bytes (mono_page, LSB = top pixel)
  function oledEncode(){
    var pages=oledH/8, buf=new Uint8Array(oledW*pages);
    for(var p=0;p<pages;p++){
      for(var x=0;x<oledW;x++){
        var b=0;
        for(var bit=0;bit<8;bit++){ if(oledPix[(p*8+bit)*oledW+x]) b|=(1<<bit); }
        buf[p*oledW+x]=b;
      }
    }
    return buf;
  }

  // PACED send: only one W command in flight at a time. The next block is
  // sent only when the previous write response arrives (see the onmessage
  // handler). An ESP8266 does not survive a burst of 17 messages + I2C
  // writes at once (RAM/lwIP exhausted, reboot); so we serialize at the
  // rate the chip can handle, like one manual command at a time.
  var oledQueue=[], oledSending=false;

  // build the paced send queue from an already-encoded framebuffer
  // (windowing prelude + data blocks), then start sending.
  function oledSendBuffer(buf,label){
    if(!(ws&&ws.readyState===1)){ log('l-err','WebSocket not connected'); return; }
    if(oledSending){ log('l-err','Already sending'); return; }
    var addr=oledAddr(), pages=oledH/8;
    oledQueue=[];
    if(oledCtrl()==='sh1106'){
      // SH1106: 132-column controller, PAGE addressing (no 20/21/22),
      // column offset (panel centered on 132 -> +2 for 128-wide). DC-DC = AD 8B.
      var off=(132-oledW)>>1;
      oledQueue.push('W '+addr+' 00 AD 8B AF');
      for(var pg=0;pg<pages;pg++){
        // set page + start column (offset) before each page
        oledQueue.push('W '+addr+' 00 '+hx(0xB0+pg)+' '+hx(off&0x0F)+' '+hx(0x10|(off>>4)));
        for(var o=0;o<oledW;o+=OLED_BLK){
          var s='W '+addr+' 40';
          for(var kk=o;kk<Math.min(o+OLED_BLK,oledW);kk++) s+=' '+hx(buf[pg*oledW+kk]);
          oledQueue.push(s);
        }
      }
    } else {
      // SSD1306: horizontal addressing (column/page window, continuous stream).
      // charge pump 8D 14, display ON.
      oledQueue.push('W '+addr+' 00 2E 8D 14 20 00 21 00 '+hx(oledW-1)+' 22 00 '+hx(pages-1)+' AF');
      for(var o2=0;o2<buf.length;o2+=OLED_BLK){
        var s2='W '+addr+' 40';
        for(var k2=o2;k2<Math.min(o2+OLED_BLK,buf.length);k2++) s2+=' '+hx(buf[k2]);
        oledQueue.push(s2);
      }
    }
    log('l-sys',label+' '+oledW+'x'+oledH+' ('+buf.length+' bytes, '+(oledQueue.length-1)+' cmds)');
    oledSending=true;
    oledSendNext();
  }

  function oledSend(){ oledSendBuffer(oledEncode(),'Send image'); }

  // send the next command; called on each write response
  function oledSendNext(){
    if(oledQueue.length===0){ oledSending=false; log('l-sys','Send complete'); return; }
    // small pause between blocks: lets the ESP8266 breathe on a full-screen
    // send (16 blocks), on top of the per-block acknowledgement.
    setTimeout(function(){ if(oledSending && oledQueue.length) send(oledQueue.shift()); }, 50);
  }

  function oledThumb(entry){
    var cv=document.createElement('canvas'); cv.width=entry.w; cv.height=entry.h;
    var c=cv.getContext('2d'); c.imageSmoothingEnabled=false;
    c.fillStyle='#000000'; c.fillRect(0,0,entry.w,entry.h);
    for(var y=0;y<entry.h;y++){ for(var x=0;x<entry.w;x++){ if(entry.pix[y*entry.w+x]){ c.fillStyle=(y<16)?'#F5C518':'#29E5FF'; c.fillRect(x,y,1,1); } } }
    cv.style.width=Math.min(120,entry.w*1.5)+'px';
    return cv;
  }

  function oledRenderGallery(){
    var g=$('oled-gallery'); g.textContent='';
    $('oled-galhint').style.display=oledGallery.length?'none':'block';
    oledGallery.forEach(function(entry,idx){
      var t=document.createElement('div'); t.className='thumb'; t.title='click to edit / resend';
      t.appendChild(oledThumb(entry));
      var del=document.createElement('span'); del.className='del'; del.textContent='x';
      del.onclick=function(ev){ ev.stopPropagation(); oledGallery.splice(idx,1); oledRenderGallery(); };
      t.appendChild(del);
      t.onclick=function(){
        $('oled-size').value=entry.w+'x'+entry.h; oledSetSize();
        oledPix=new Uint8Array(entry.pix); oledSrcImg=null; oledSrcData=null; oledDraw();
        log('l-sys','Image reloaded from gallery ('+entry.w+'x'+entry.h+')');
      };
      g.appendChild(t);
    });
  }

  function oledSave(){
    if(!oledPix) return;
    oledGallery.push({w:oledW,h:oledH,pix:new Uint8Array(oledPix)});
    oledRenderGallery();
  }

  $('oled-size').onchange=oledSetSize;
  $('oled-load').onclick=function(){ $('oled-file').click(); };
  $('oled-file').onchange=function(e){ if(e.target.files&&e.target.files[0]) oledLoad(e.target.files[0]); e.target.value=''; };
  $('oled-thresh').oninput=function(){ $('oled-thresh-val').textContent=$('oled-thresh').value; oledConvert(); };
  $('oled-dither').onchange=oledConvert;
  $('oled-invert').onchange=oledConvert;
  $('oled-fit').onchange=function(){ if(oledRasterize()) oledConvert(); };
  $('oled-send').onclick=oledSend;
  $('oled-save').onclick=oledSave;

  // --- pixel-by-pixel draw / erase on the preview ---
  var oledTool='draw', oledDrawing=false, oledLX=-1, oledLY=-1;
  function oledSetTool(t){
    oledTool=t;
    $('oled-draw').className='sm'+(t==='draw'?' tsel':'');
    $('oled-erase').className='sm'+(t==='erase'?' tsel':'');
  }
  $('oled-draw').onclick=function(){ oledSetTool('draw'); };
  $('oled-erase').onclick=function(){ oledSetTool('erase'); };

  // Clear all: empties the temporary image (drawing + loaded image), local only
  // -- sends nothing to the panel.
  $('oled-clear').onclick=function(){
    oledPix=new Uint8Array(oledW*oledH);
    oledSrcImg=null; oledSrcData=null;
    oledDraw();
  };

  // set or clear ONE pixel (draws just that pixel, no full redraw)
  function oledSetPixel(x,y){
    if(x<0||y<0||x>=oledW||y>=oledH) return;
    var on=(oledTool==='draw')?1:0;
    oledPix[y*oledW+x]=on;
    octx.fillStyle=on?((y<16)?'#F5C518':'#29E5FF'):'#000000';
    octx.fillRect(x*OLED_SCALE,y*OLED_SCALE,OLED_SCALE-OLED_GAP,OLED_SCALE-OLED_GAP);
  }
  // screen coords -> pixel, with interpolation (Bresenham) to avoid
  // gaps when dragging; accounts for the displayed scale.
  function oledPaint(cx,cy){
    var r=oledCv.getBoundingClientRect();
    if(r.width===0||r.height===0) return;
    var x=Math.floor((cx-r.left)/r.width*oledW), y=Math.floor((cy-r.top)/r.height*oledH);
    if(x<0||y<0||x>=oledW||y>=oledH){ oledLX=-1; return; }
    if(oledLX<0){ oledSetPixel(x,y); }
    else {
      var dx=Math.abs(x-oledLX), dy=Math.abs(y-oledLY), sx=oledLX<x?1:-1, sy=oledLY<y?1:-1, err=dx-dy, px=oledLX, py=oledLY;
      for(;;){ oledSetPixel(px,py); if(px===x&&py===y) break; var e2=2*err; if(e2>-dy){err-=dy;px+=sx;} if(e2<dx){err+=dx;py+=sy;} }
    }
    oledLX=x; oledLY=y;
  }
  oledCv.addEventListener('mousedown',function(e){ oledDrawing=true; oledLX=-1; oledPaint(e.clientX,e.clientY); e.preventDefault(); });
  oledCv.addEventListener('mousemove',function(e){ if(oledDrawing) oledPaint(e.clientX,e.clientY); });
  window.addEventListener('mouseup',function(){ oledDrawing=false; oledLX=-1; });
  oledCv.addEventListener('touchstart',function(e){ oledDrawing=true; oledLX=-1; var t=e.touches[0]; oledPaint(t.clientX,t.clientY); e.preventDefault(); },{passive:false});
  oledCv.addEventListener('touchmove',function(e){ if(oledDrawing){ var t=e.touches[0]; oledPaint(t.clientX,t.clientY); e.preventDefault(); } },{passive:false});
  window.addEventListener('touchend',function(){ oledDrawing=false; oledLX=-1; });

  oledSetSize();
  oledRenderGallery();

  // ------------------------------------------------------------------
  //  One-way serial mirror (Web Serial). Copies each NEW log line, as
  //  plain text (no color) + CRLF, to a serial port on the PC. Browser
  //  -> port stream only (no reading back). Web Serial requires a secure
  //  context: on http://<ip>, you need the flag
  //  chrome://flags/#unsafely-treat-insecure-origin-as-secure.
  // ------------------------------------------------------------------
  var serPort=null, serWriter=null, serOn=false, serEnc=new TextEncoder();
  var serSupported=('serial' in navigator);

  function serSetStatus(txt,up){var b=$('ser');b.textContent=txt;b.className=up?'up':'';}

  function serForget(){
    serOn=false;
    if(serWriter){try{serWriter.releaseLock();}catch(e){}}
    serWriter=null;
    var p=serPort;serPort=null;
    if(p){try{p.close();}catch(e){}}
    serSetStatus('inactive',false);
    $('ser-btn').textContent='Connect serial port';
  }

  function serAttach(baud){
    serWriter=serPort.writable.getWriter();
    serOn=true;
    serSetStatus('active @'+baud,true);
    $('ser-btn').textContent='Disconnect serial port';
  }

  // mirror one line (called by log()); no-op if the port is closed
  function serMirror(line){
    if(!serOn||!serWriter)return;
    serWriter.write(serEnc.encode(line+'\r\n')).catch(function(){serForget();});
  }

  function serConnect(){
    if(!serSupported)return;
    navigator.serial.requestPort().then(function(p){
      serPort=p;
      var baud=parseInt($('baud').value,10)||115200;
      return serPort.open({baudRate:baud}).then(function(){
        serAttach(baud);
        log('l-sys','Serial mirror active ('+baud+' baud) -- new lines only');
      });
    }).catch(function(e){
      if(e&&e.name!=='NotFoundError')log('l-err','Serial: '+(e.message||e.name||'error'));
      serForget();
    });
  }

  // Web Serial can't change baud live: close and reopen the SAME
  // port (without re-prompting the port picker).
  function serChangeBaud(){
    if(!serOn||!serPort)return;
    var baud=parseInt($('baud').value,10)||115200;
    serOn=false;
    if(serWriter){try{serWriter.releaseLock();}catch(e){}}
    serWriter=null;
    Promise.resolve(serPort.close()).catch(function(){}).then(function(){
      return serPort.open({baudRate:baud});
    }).then(function(){
      serAttach(baud);
      log('l-sys','Mirror baud: '+baud);
    }).catch(function(e){
      log('l-err','Serial: reopen failed ('+(e.message||e.name||'error')+')');
      serForget();
    });
  }

  if(serSupported){
    $('ser-btn').onclick=function(){if(serOn){serForget();}else{serConnect();}};
    $('baud').onchange=function(){if(serOn){serChangeBaud();}};
  }else{
    $('ser-btn').disabled=true;
    $('baud').disabled=true;
    serSetStatus('unavailable',false);
    $('ser-banner').style.display='block';
  }

  // ================================================================
  //  On-demand I2C transaction decoder. Reads a FROZEN snapshot of the
  //  journal on click, pairs each "> W/R/WR/RR/SCAN" line with its result
  //  line, and renders a structured, colored breakdown beside the log.
  //  SSD1306/SH1106 command bytes get semantic decoding (control 00);
  //  data streams (control 40) are summarized in bulk. ACK is INFERRED
  //  from the transaction result (not sniffed from the wire).
  // ================================================================
  // SSD1306/SH1106 command table (chip-agnostic, operand-aware).
  function decCmd(b,ops){
    var op=function(k){ return ops[k]!==undefined?ops[k]:'?'; };
    if(b>=0xB0&&b<=0xB7) return {n:0,t:'Set page '+(b&7)};
    if(b>=0x00&&b<=0x0F) return {n:0,t:'Column low nibble '+(b&0x0F)};
    if(b>=0x10&&b<=0x1F) return {n:0,t:'Column high nibble '+(b&0x0F)};
    if(b>=0x40&&b<=0x7F) return {n:0,t:'Start line = '+(b&0x3F)};
    if(b>=0x30&&b<=0x33) return {n:0,t:'Pump voltage (SH1106)'};
    switch(b){
      case 0xAE:return{n:0,t:'Display OFF'};case 0xAF:return{n:0,t:'Display ON'};
      case 0xA4:return{n:0,t:'Resume to RAM'};case 0xA5:return{n:0,t:'All pixels ON'};
      case 0xA6:return{n:0,t:'Normal (not inverted)'};case 0xA7:return{n:0,t:'Inverted'};
      case 0xA0:return{n:0,t:'Segment remap off'};case 0xA1:return{n:0,t:'Segment remap (flip H)'};
      case 0xC0:return{n:0,t:'COM scan normal'};case 0xC8:return{n:0,t:'COM scan reversed (flip V)'};
      case 0x2E:return{n:0,t:'Scroll off'};case 0x2F:return{n:0,t:'Scroll on'};case 0xE3:return{n:0,t:'NOP'};
      case 0xD5:return{n:1,t:'Clock: divide '+((op(0)&0x0F)+1)+', osc '+(op(0)>>4)};
      case 0xA8:return{n:1,t:'Multiplex ratio = '+((op(0)&0x3F)+1)};
      case 0xD3:return{n:1,t:'Display offset = '+op(0)};
      case 0x81:return{n:1,t:'Contrast = '+op(0)};
      case 0x8D:return{n:1,t:'Charge pump '+(op(0)===0x14?'ON':op(0)===0x10?'OFF':'0x'+hx(op(0)))};
      case 0xAD:return{n:1,t:'DC-DC '+(op(0)===0x8B?'ON':op(0)===0x8A?'OFF':'0x'+hx(op(0)))+' (SH1106)'};
      case 0xD9:return{n:1,t:'Pre-charge = 0x'+hx(op(0))};
      case 0xDB:return{n:1,t:'VCOMH deselect = 0x'+hx(op(0))};
      case 0xDA:return{n:1,t:'COM pins = 0x'+hx(op(0))};
      case 0x20:return{n:1,t:'Addressing = '+(['Horizontal','Vertical','Page'][op(0)]||'0x'+hx(op(0)))};
      case 0xA3:return{n:2,t:'Vertical scroll area'};
      case 0x21:return{n:2,t:'Column address '+op(0)+'..'+op(1)};
      case 0x22:return{n:2,t:'Page address '+op(0)+'..'+op(1)};
      case 0x26:return{n:6,t:'Scroll setup (right)'};case 0x27:return{n:6,t:'Scroll setup (left)'};
      case 0x29:return{n:5,t:'Scroll setup (vert+right)'};case 0x2A:return{n:5,t:'Scroll setup (vert+left)'};
    }
    return {n:0,t:'unknown command'};
  }
  function decHexList(toks){ return toks.map(function(t){return parseInt(t.replace(/^0x/i,''),16)&0xFF;}); }
  function decEsc(s){ return String(s).replace(/&/g,'&amp;').replace(/</g,'&lt;'); }

  // parse a frozen array of log line strings into transactions
  function decParse(lines,deltas,tos){
    var txns=[];
    for(var i=0;i<lines.length;i++){
      var L=lines[i], DT=(deltas&&deltas[i])||'', TO=(tos&&tos[i])||null;
      if(TO){ TO.dt=DT; txns.push(TO); continue; }
      // tap full-data line (starts with the guillemet marker + " : bytes")
      if(L.indexOf('\u00bb ')===0){
        var Lt=L.slice(2).trim().replace(/\s+x\d+$/,'');  // drop batch "x300" count
        var mm=Lt.match(/^(\w+)\s+0x([0-9A-Fa-f]{1,2})\s*:\s*([0-9A-Fa-f ]*?)\s+(\S+)\s*$/);
        if(mm){
          var kwT=mm[1].toUpperCase(), adT=mm[2].toUpperCase(), byT=mm[3].trim(), rsT=mm[4];
          var bts=byT?byT.split(/\s+/):[];
          if(kwT==='R'){ txns.push({kw:'R',toks:['R',adT,String(bts.length)],res:rsT,data:byT,scan:[],dt:DT}); }
          else { txns.push({kw:'W',toks:['W',adT].concat(bts),res:rsT,data:null,scan:[],dt:DT}); }
        }
        continue;
      }
      if(L.indexOf('> ')!==0) continue;
      var toks=L.slice(2).trim().split(/\s+/), kw=(toks[0]||'').toUpperCase();
      var res=null,data=null,scan=[];
      for(var j=i+1;j<lines.length;j++){
        var R=lines[j]; if(R.indexOf('> ')===0) break;
        if(kw==='SCAN'){
          if(/^SCAN:/.test(R)) res=R;
          else if(/^\s+0x[0-9A-Fa-f]{2}/.test(R)) scan.push(R.trim());
          else if(res) break;
          continue;
        }
        var a=R.indexOf('->');
        if(a>=0){ var af=R.slice(a+2), di=af.indexOf(' : ');
          res=(di>=0?af.slice(0,di):af).trim(); data=di>=0?af.slice(di+3).trim():null; break; }
      }
      if(kw!=='TAP'&&kw!=='MIRROR') txns.push({kw:kw,toks:toks,res:res,data:data,scan:scan});
    }
    return txns;
  }
  // one decoded row: colored hex column + meaning + optional ACK
  function decRow(hxCls,hxTxt,mnCls,mn,ack){
    return '<div class="drow"><span class="dhx '+hxCls+'">'+hxTxt+'</span><span class="dmn '+(mnCls||'')+'">'+mn+'</span>'+(ack||'')+'</div>';
  }
  function decTxnHtml(t,oledAddrV,chip){
    var ok=t.res==='OK', ack=ok?'<span class="d-ack">ACK</span>':'', o=[];
    if(t.dt) o.push('<div class="d-ts">'+decEsc(t.dt)+'</div>');
    if(t.kw==='SCAN'){
      o.push('<div class="d-start">SCAN 0x08-0x77</div>');
      t.scan.forEach(function(s){ var a=s.split(/\s+/)[0], h=s.slice(a.length).trim();
        o.push(decRow('d-addr',decEsc(a)+' W','d-addr','device present'+(h?'  '+decEsc(h):''),'<span class="d-ack">ACK</span>')); });
      o.push('<div class="dsum">'+decEsc(t.res||'')+'</div>');
      return '<div class="dtx">'+o.join('')+'</div>';
    }
    if(t.kind==='data'){
      var szc=(t.count>1)?(t.count+' \u00d7 '+t.n+' B'):(t.n+' B');
      o.push('<div class="d-start">START</div>');
      o.push(decRow('d-addr',decEsc(t.toks[1]||'')+' W','d-data','pixel data \u00b7 '+szc,ack));
      o.push('<div class="dsum">data stream to GDDRAM (not a command)</div>');
      o.push('<div class="d-start">STOP</div>');
      return '<div class="dtx">'+o.join('')+'</div>';
    }
    var addr=(t.toks[1]||'').replace(/^0x/i,'');
    var isOled=(chip==='ssd1306'||chip==='sh1106')&&addr.toUpperCase()===oledAddrV.toUpperCase();
    o.push('<div class="d-start">START</div>');
    if(t.kw==='W'){
      var b=decHexList(t.toks.slice(2)), nc=0;
      o.push(decRow('d-addr','0x'+decEsc(addr),'d-addr','ADDR &middot; Write',ack));
      if(isOled&&b.length&&(b[0]===0x00||b[0]===0x40||b[0]===0x80||b[0]===0xC0)){
        var ctrl=b[0], mode=(ctrl===0x40||ctrl===0xC0)?'data':'command';
        o.push(decRow('d-ctrl',hx(ctrl),'','CONTROL &middot; '+mode+' stream',ack));
        if(mode==='data'){ o.push(decRow('d-data','&middot;&middot;&middot;','d-data',(b.length-1)+' bytes pixel data (block)',ack)); }
        else { var i=1; while(i<b.length){ var d=decCmd(b[i],b.slice(i+1,i+3)), hs=hx(b[i]);
          for(var k=1;k<=d.n&&i+k<b.length;k++) hs+=' '+hx(b[i+k]);
          var inc=(i+d.n<b.length)?'':(d.n>0?' \u2014 params sent separately':'');
          o.push(decRow('d-cmd',hs,'',decEsc(d.t)+inc,ack)); nc++; i+=1+d.n; } }
      } else { b.forEach(function(x,ix){ o.push(decRow('d-data',hx(x),'d-data',ix===0?'reg / pointer?':'data',ack)); }); }
      o.push('<div class="d-stop">STOP</div>');
      o.push('<div class="dsum">W 0x'+decEsc(addr)+' &middot; '+b.length+' B'+(nc?' &middot; '+nc+' cmds':'')+' &middot; '+(ok?'OK':decEsc(t.res||'no result'))+'</div>');
      o.push('<div class="d-note">'+(ok?'ACK inferred from result':(t.res?'not acknowledged ('+decEsc(t.res)+') &mdash; exact byte not observable':'awaiting result'))+'</div>');
    } else if(t.kw==='R'){
      o.push(decRow('d-addr','0x'+decEsc(addr),'d-addr','ADDR &middot; Read',ack));
      o.push(decRow('d-data','&middot;&middot;&middot;','d-data','read '+decEsc(t.toks[2]||'?')+' bytes'+(t.data?': '+decEsc(t.data):''),''));
      o.push('<div class="d-stop">STOP</div>');
      o.push('<div class="dsum">R 0x'+decEsc(addr)+' &middot; '+(ok?'OK':decEsc(t.res||'no result'))+'</div>');
    } else if(t.kw==='WR'){
      var wb=decHexList(t.toks.slice(3)), rg=(t.toks[2]||'').replace(/^0x/i,'');
      o.push(decRow('d-addr','0x'+decEsc(addr),'d-addr','ADDR &middot; Write',ack));
      o.push(decRow('d-ctrl',decEsc(t.toks[2]||''),'','register 0x'+decEsc(rg),ack));
      wb.forEach(function(x){ o.push(decRow('d-data',hx(x),'d-data','data',ack)); });
      o.push('<div class="d-stop">STOP</div>');
      o.push('<div class="dsum">WR 0x'+decEsc(addr)+' reg 0x'+decEsc(rg)+' &middot; '+(ok?'OK':decEsc(t.res||'no result'))+'</div>');
    } else if(t.kw==='RR'){
      var rg2=(t.toks[2]||'').replace(/^0x/i,'');
      o.push(decRow('d-addr','0x'+decEsc(addr),'d-addr','ADDR &middot; Write',ack));
      o.push(decRow('d-ctrl',decEsc(t.toks[2]||''),'','register 0x'+decEsc(rg2),ack));
      o.push('<div class="d-restart">RESTART</div>');
      o.push(decRow('d-addr','0x'+decEsc(addr),'d-addr','ADDR &middot; Read',ack));
      o.push(decRow('d-data','&middot;&middot;&middot;','d-data','read '+decEsc(t.toks[3]||'?')+' bytes'+(t.data?': '+decEsc(t.data):''),''));
      o.push('<div class="d-stop">STOP</div>');
      o.push('<div class="dsum">RR 0x'+decEsc(addr)+' reg 0x'+decEsc(rg2)+' &middot; '+(ok?'OK':decEsc(t.res||'no result'))+'</div>');
    } else {
      o.push('<div class="dsum">'+decEsc(t.kw)+' (not decoded)</div>');
    }
    return '<div class="dtx">'+o.join('')+'</div>';
  }
  var decFilter='';
  function applyDecFilter(){ var bl=$('dec-out').querySelectorAll('.dtxn'); for(var i=0;i<bl.length;i++){ bl[i].style.display=(!decFilter||bl[i].textContent.toLowerCase().indexOf(decFilter)>=0)?'':'none'; } }
  // In Live mode, ESPHome sends each SSD1306 command byte as its own write, so a
  // multi-byte command (e.g. Set Column Address) spans several transactions. Merge
  // consecutive command writes to the same address into one byte stream so the
  // decoder can group them. Batch coalescing scrambles the order, so skip it then.
  function decMerge(txns){
    var out=[], i=0;
    while(i<txns.length){
      var t=txns[i];
      if(t.kw==='W'&&t.kind==='cmd'){
        var addr=t.toks[1], bytes=t.toks.slice(2), j=i+1;
        while(j<txns.length&&txns[j].kw==='W'&&txns[j].kind==='cmd'&&txns[j].toks[1]===addr){
          var nb=txns[j].toks.slice(2); bytes=bytes.concat(nb.length>1?nb.slice(1):nb); j++;
        }
        if(j>i+1) out.push({kw:'W',toks:['W',addr].concat(bytes),res:t.res,data:null,scan:[],kind:'cmd',n:bytes.length,count:1,dt:t.dt});
        else out.push(t);
        i=j;
      } else { out.push(t); i++; }
    }
    return out;
  }
  function decRun(){
    var lines=[], deltas=[], tos=[]; Array.prototype.forEach.call(out.children,function(d){ lines.push(d.dataset.raw!==undefined?d.dataset.raw:d.textContent); deltas.push(d.dataset.dt||''); var pt=null; if(d.dataset.txn){ try{ pt=JSON.parse(d.dataset.txn); }catch(e){} } tos.push(pt); });
    var parsed=decParse(lines,deltas,tos), av=oledAddr(), cp=oledCtrl();
    var batched=parsed.some(function(t){return t.count>1;}), hasCmd=parsed.some(function(t){return t.kind==='cmd';});
    var dropping=(Date.now()-lastDropTime<3000);
    var txns, note='';
    if(dropping&&hasCmd){ txns=parsed; note='<div class="dnote">Bus flooded &mdash; transactions are being dropped, so the captured command sequence has gaps and cannot be grouped reliably. Expected for a continuously refreshing display on ESP8266; use the Screen mirror to see what the display shows.</div>'; }
    else if(batched&&hasCmd){ txns=parsed; note='<div class="dnote">Batched capture: command sequences are coalesced, so multi-byte commands cannot be grouped. Switch Delivery to Live for exact decoding.</div>'; }
    else { txns=decMerge(parsed); }
    var html = txns.length ? note+txns.map(function(t){return '<div class="dtxn">'+decTxnHtml(t,av,cp)+'</div>';}).join('')
                           : '<div class="dsum">No transactions in the log to decode.</div>';
    $('dec-out').innerHTML=html;
    applyDecFilter();
    $('dec-panel').classList.remove('dcol');
  }
  var DEC_EMPTY='<div class="dempty">No decode yet<br>click &#8594; to decode the log</div>';
  $('dec-btn').onclick=decRun;
  $('dec-clear').onclick=function(){ $('dec-out').innerHTML=DEC_EMPTY; };
  $('dec-filter').oninput=function(){ decFilter=this.value.trim().toLowerCase(); applyDecFilter(); };
  $('dec-collapse').onclick=function(){ $('dec-panel').classList.toggle('dcol'); };
  $('recover-btn').onclick=function(){ send('RECOVER'); };
  $('tap-full-btn').onclick=function(){ var a=$('tap-filter').value.trim(); send('TAP FULL '+a); log('l-sys','tap: full data '+(a?('on 0x'+a):'on all addresses')); };
  $('tap-sum-btn').onclick=function(){ send('TAP SUMMARY'); log('l-sys','tap: summary only'); };
  $('tap-batch-btn').onclick=function(){ send('TAP BATCH'); log('l-sys','tap: batch (coalesced every 200ms)'); };
  $('tap-live-btn').onclick=function(){ send('TAP LIVE'); log('l-sys','tap: live (one per transaction)'); };
  $('tap-takeover-btn').onclick=function(){ send('TAP TAKEOVER'); log('l-sys','tap: TAKE OVER -- frontend owns the bus, device traffic blocked'); };
  $('tap-relay-btn').onclick=function(){ send('TAP RELAY'); log('l-sys','tap: relay -- device traffic reaches the bus normally'); };
  $('tap-mirror-btn').onclick=function(){ send('TAP MIRROR 1'); log('l-sys','tap: screen mirror on -- reconstructing the framebuffer from the bus'); };
  $('tap-mirroroff-btn').onclick=function(){ send('TAP MIRROR 0'); log('l-sys','tap: screen mirror off'); };

  // ---- Info tab: fill fields from the {"t":"info",...} push; live values refresh ----
  function fmtFreq(f){ return f>=1000000 ? (Math.round(f/1000)/1000)+' MHz' : Math.round(f/1000)+' kHz'; }
  function fmtUp(s){ s=s|0; var h=Math.floor(s/3600), m=Math.floor((s%3600)/60), ss=s%60; return h>0?(h+'h '+m+'m'):(m>0?(m+'m '+ss+'s'):(ss+'s')); }
  function iLvl(el,v){ if(!el)return; el.textContent=(v===1?'HIGH':v===0?'LOW':'--'); el.style.color=(v===1?'#56d364':v===0?'#f85149':''); }
  function renderFb(hex){
    var c=$('fb-canvas'); if(!c||!hex) return;
    var ctx=c.getContext('2d'), img=ctx.createImageData(128,64), d=img.data;
    for(var i=0;i<1024;i++){
      var b=parseInt(hex.substr(i*2,2),16)||0, page=(i/128)|0, col=i%128;
      for(var bit=0;bit<8;bit++){
        var on=(b>>bit)&1, y=page*8+bit, idx=(y*128+col)*4;
        d[idx]=on?79:0; d[idx+1]=on?209:0; d[idx+2]=on?197:0; d[idx+3]=255;
      }
    }
    ctx.putImageData(img,0,0);
    c.style.display='block';
  }
  var SVGNS='http://www.w3.org/2000/svg';
  function svel(t,a){ var e=document.createElementNS(SVGNS,t); for(var k in a) e.setAttribute(k,a[k]); return e; }
  function gPolar(cx,cy,r,deg){ var a=(deg-90)*Math.PI/180; return [cx+r*Math.cos(a), cy+r*Math.sin(a)]; }
  function gArc(cx,cy,r,d0,d1){ var s=gPolar(cx,cy,r,d1), e=gPolar(cx,cy,r,d0), lg=(d1-d0)>180?1:0; return 'M'+s[0].toFixed(1)+' '+s[1].toFixed(1)+' A '+r+' '+r+' 0 '+lg+' 0 '+e[0].toFixed(1)+' '+e[1].toFixed(1); }
  var GA=[{k:'obs',cx:50,lab:'bus /s',max:1000,z:[[0,.7,'#2f9e77'],[.7,.9,'#c99a2e'],[.9,1,'#c0564f']]},{k:'loop',cx:150,lab:'loop Hz',max:80,z:[[0,.25,'#c0564f'],[.25,.5,'#c99a2e'],[.5,1,'#2f9e77']]},{k:'heap',cx:250,lab:'heap KB',max:45000,z:[[0,.2,'#c0564f'],[.2,.4,'#c99a2e'],[.4,1,'#2f9e77']]}];
  var GCY=50, GR=38, gEls={};
  function buildGauges(){
    var svg=document.getElementById('gauges'); if(!svg||svg.childNodes.length)return;
    GA.forEach(function(g){
      svg.appendChild(svel('path',{d:gArc(g.cx,GCY,GR,-135,135),fill:'none',stroke:'#252c36','stroke-width':6,'stroke-linecap':'round'}));
      g.z.forEach(function(z){ svg.appendChild(svel('path',{d:gArc(g.cx,GCY,GR,-135+z[0]*270,-135+z[1]*270),fill:'none',stroke:z[2],'stroke-width':6})); });
      for(var i=0;i<=4;i++){ var d=-135+i*67.5, p1=gPolar(g.cx,GCY,GR-7,d), p2=gPolar(g.cx,GCY,GR-2,d); svg.appendChild(svel('line',{x1:p1[0].toFixed(1),y1:p1[1].toFixed(1),x2:p2[0].toFixed(1),y2:p2[1].toFixed(1),stroke:'#3a424e','stroke-width':1})); }
      var ndl=svel('line',{x1:g.cx,y1:GCY,x2:g.cx,y2:(GCY-(GR-8)),stroke:'#f6a821','stroke-width':2.5,'stroke-linecap':'round'}); svg.appendChild(ndl);
      svg.appendChild(svel('circle',{cx:g.cx,cy:GCY,r:4,fill:'#f6a821'}));
      var val=svel('text',{x:g.cx,y:(GCY+40),'text-anchor':'middle',fill:'#e6eaef','font-size':15,'font-weight':600}); val.textContent='--'; svg.appendChild(val);
      var lab=svel('text',{x:g.cx,y:(GCY+53),'text-anchor':'middle',fill:'#7f8797','font-size':10}); lab.textContent=g.lab; svg.appendChild(lab);
      gEls[g.k]={ndl:ndl,val:val,cfg:g};
    });
  }
  function updGauges(m){
    for(var k in gEls){ var e=gEls[k], v=(k==='obs'?m.obsrate:k==='loop'?m.loophz:m.heap); if(typeof v!=='number')continue;
      var frac=Math.max(0,Math.min(1,v/e.cfg.max)), tip=gPolar(e.cfg.cx,GCY,GR-8,-135+frac*270);
      e.ndl.setAttribute('x2',tip[0].toFixed(1)); e.ndl.setAttribute('y2',tip[1].toFixed(1));
      e.val.textContent=(k==='heap')?Math.round(v/1000):Math.round(v);
    }
  }
  var wEl={};
  function wbox(svg,x,y,w,h,txt){ svg.appendChild(svel('rect',{x:x,y:y,width:w,height:h,rx:5,fill:'#171b22',stroke:'#3a424e','stroke-width':0.8})); var t=svel('text',{x:x+w/2,y:y+h/2+3.5,'text-anchor':'middle',fill:'#c8cdd6','font-size':10}); t.textContent=txt; svg.appendChild(t); }
  function buildWiring(){
    var svg=document.getElementById('wiring'); if(!svg||svg.childNodes.length)return;
    wEl.relay=svel('line',{x1:70,y1:21,x2:116,y2:21,stroke:'#5dcaa5','stroke-width':1.8}); svg.appendChild(wEl.relay);
    wEl.manual=svel('line',{x1:230,y1:21,x2:184,y2:21,stroke:'#5dcaa5','stroke-width':1.8}); svg.appendChild(wEl.manual);
    wEl.capture=svel('line',{x1:150,y1:34,x2:150,y2:60,stroke:'#f6a821','stroke-width':1.8}); svg.appendChild(wEl.capture);
    wbox(svg,8,8,62,26,'device'); wbox(svg,118,8,64,26,'I2C bus'); wbox(svg,232,8,60,26,'you');
    wEl.mirbox=svel('g',{}); svg.appendChild(wEl.mirbox); wbox(wEl.mirbox,118,60,64,26,'mirror');
    wEl.relaylab=svel('text',{x:93,y:16,'text-anchor':'middle',fill:'#5dcaa5','font-size':8.5}); wEl.relaylab.textContent='relay'; svg.appendChild(wEl.relaylab);
    wEl.manlab=svel('text',{x:207,y:16,'text-anchor':'middle',fill:'#5dcaa5','font-size':8.5}); wEl.manlab.textContent='manual'; svg.appendChild(wEl.manlab);
    wEl.caplab=svel('text',{x:186,y:52,'text-anchor':'start',fill:'#a6712a','font-size':8.5}); wEl.caplab.textContent='from device'; svg.appendChild(wEl.caplab);
  }
  function updWiring(m){
    if(!wEl.relay)return;
    var tko=!!m.taptakeover, mir=!!m.tapfb;
    if(tko){ wEl.relay.setAttribute('stroke','#c0564f'); wEl.relay.setAttribute('stroke-dasharray','3 3'); wEl.relaylab.textContent='blocked'; wEl.relaylab.setAttribute('fill','#c0564f'); }
    else { wEl.relay.setAttribute('stroke','#5dcaa5'); wEl.relay.removeAttribute('stroke-dasharray'); wEl.relaylab.textContent='relay'; wEl.relaylab.setAttribute('fill','#5dcaa5'); }
    var vis=mir?'visible':'hidden';
    wEl.capture.setAttribute('visibility',vis); wEl.mirbox.setAttribute('visibility',vis); wEl.caplab.setAttribute('visibility',vis);
    wEl.caplab.textContent=tko?'from you':'from device';
  }
  function updInfo(m){
    var set=function(id,t){ var e=$(id); if(e) e.textContent=t; };
    set('i-freq', m.freq?fmtFreq(m.freq):'--');
    set('i-sda', m.sda>=0?('GPIO'+m.sda):'--');
    set('i-scl', m.scl>=0?('GPIO'+m.scl):'--');
    set('i-scan', m.scan?'enabled':'disabled');
    set('i-pullup', (m.sdapu<0&&m.sclpu<0)?'n/a (Arduino)':('SDA '+(m.sdapu?'on':'off')+' / SCL '+(m.sclpu?'on':'off')));
    // bus monitor
    var tc=m.tapcount||0, obs=m.observed||0;
    set('i-attached', tc+' device'+(tc===1?'':'s'));
    set('i-observed', obs.toLocaleString()+' txn');
    set('i-obsrate', (m.obsrate||0).toLocaleString()+'/s');
    set('i-dropped', (m.dropped||0).toLocaleString());
    set('i-loophz', (m.loophz||0).toLocaleString()+' Hz');
    set('i-bootheld', (m.bootheld||0).toLocaleString());
    var flt=(m.tapfilter>=0)?('0x'+m.tapfilter.toString(16).toUpperCase()):'all';
    var cap=m.tapoff?'logging off':('logging '+(m.tapfull?'full data':'summary')+' on '+flt+(m.tapbatch?', batched':''));
    if(m.taptakeover) cap+=' \u00b7 you control the bus';
    var tm=$('i-tapmode'); if(tm){ tm.textContent=cap; tm.style.color=m.taptakeover?'#f6c177':(m.tapfull?'#4fd1c5':'#8a90a0'); }
    set('i-batch', m.tapbatch?((m.batchrate||0)+' msg/s ('+(m.obsrate||0)+'/s in)'):'live (off)');
    var seg=function(id,on){ var e=$(id); if(e) e.classList[on?'add':'remove']('on'); };
    seg('tap-full-btn', m.tapfull && !m.tapoff);
    seg('tap-sum-btn', !m.tapfull && !m.tapoff);
    seg('tap-batch-btn', !!m.tapbatch);
    seg('tap-live-btn', !m.tapbatch);
    seg('tap-relay-btn', !m.taptakeover);
    var te=$('tap-takeover-btn'); if(te) te.classList[m.taptakeover?'add':'remove']('own');
    seg('tap-mirror-btn', !!m.tapfb);
    seg('tap-mirroroff-btn', !m.tapfb);
    if(!m.tapfb){ var fc=$('fb-canvas'); if(fc) fc.style.display='none'; }
    updGauges(m); updWiring(m);
    var ts=$('i-tapstatus');
    if(ts){
      if(tc===0&&obs===0){ ts.textContent='standalone terminal'; ts.style.color='#8b929c'; }
      else if(m.tapskip){ ts.textContent='skipping'; ts.style.color='#e3a13a'; }
      else if(obs>0){ ts.textContent='live'; ts.style.color='#56d364'; }
      else { ts.textContent='attached (idle)'; ts.style.color='#8b929c'; }
    }
    iLvl($('i-sdalvl'), m.sdalvl); iLvl($('i-scllvl'), m.scllvl);
    var bs=$('i-busstatus');
    if(bs){
      if(m.sdalvl===1&&m.scllvl===1){ bs.textContent='idle / ok'; bs.style.color='#56d364'; }
      else if(m.sdalvl===0||m.scllvl===0){ bs.textContent='line held!'; bs.style.color='#f85149'; }
      else { bs.textContent='--'; bs.style.color=''; }
    }
    var held=(m.sdalvl===0||m.scllvl===0);
    if(m.sdalvl===1&&m.scllvl===1) setPill('st-bus','#5dcaa5','idle / ok');
    else if(held) setPill('st-bus','#e06c75','line held!');
    else setPill('st-bus','#3a424e','--');
    setPill('st-tap', m.taptakeover?'#f6c177':'#4fd1c5', m.taptakeover?'take over':'relay');
    setPill('st-dev', '#3a424e', tc+' device'+(tc===1?'':'s'));
    var flooding=(typeof m.dropped==='number'&&m.dropped>floodPrev&&(m.obsrate||0)>150);
    if(typeof m.dropped==='number'){ if(m.dropped>floodPrev) lastDropTime=Date.now(); floodPrev=m.dropped; }
    var sf=$('st-flood'); if(sf) sf.style.display=flooding?'':'none';
    var tb=$('topbar'); if(tb) tb.classList.toggle('alert', held||flooding);
    set('i-heap', (typeof m.heap==='number')?(m.heap.toLocaleString()+' B'):'--');
    set('i-minheap', (typeof m.minheap==='number')?(m.minheap.toLocaleString()+' B'):'--');
    set('i-mcu', m.mcu||'--'); set('i-fw', m.fw||'--'); set('i-reset', m.reset||'--');
    set('i-uptime', fmtUp(m.uptime));
    set('i-port', m.port||'--');
    var cl=$('i-client'); if(cl){ cl.textContent='connected'; cl.style.color='#56d364'; }
  }

  buildGauges(); buildWiring();
  connectWS();
})();
</script>
</body>
</html>)HTMLDOC";

// Length computed at compile time (sizeof works here, complete definition).
// Avoids a strlen on a PROGMEM string on ESP8266.
extern const uint32_t WEB_I2C_PAGE_LEN = sizeof(WEB_I2C_PAGE) - 1;

}  // namespace web_i2c
}  // namespace esphome
