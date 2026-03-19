#include "pages.h"

// ========================================================================
//  Common CSS and JS (shared styles/helpers embedded in each page)
// ========================================================================

#define CSS_COMMON \
"<style>" \
"*{margin:0;padding:0;box-sizing:border-box}" \
":root{--bg:#0a0a1a;--bg2:#12122a;--border:#2a2a4e;--accent:#0f4;--text:#e0e0e0;" \
"--text2:#888;--red:#f44;--yellow:#fa0;--green:#0f4}" \
"body{background:var(--bg);color:var(--text);font-family:'Courier New',monospace;font-size:14px}" \
"a{color:var(--accent);text-decoration:none}" \
".container{max-width:900px;margin:0 auto;padding:12px}" \
"nav{background:var(--bg2);border-bottom:1px solid var(--border);padding:10px 20px;" \
"display:flex;align-items:center;gap:20px;flex-wrap:wrap}" \
"nav .title{color:var(--accent);font-size:18px;font-weight:bold}" \
"nav a{padding:6px 14px;border-radius:4px;border:1px solid var(--border);color:var(--text2)}" \
"nav a:hover,nav a.active{color:var(--accent);border-color:var(--accent)}" \
".card{background:var(--bg2);border:1px solid var(--border);border-radius:8px;padding:16px;margin:12px 0}" \
".card h2{color:var(--accent);font-size:15px;margin-bottom:12px;border-bottom:1px solid var(--border);padding-bottom:8px}" \
"button,input[type=submit]{background:transparent;color:var(--accent);border:1px solid var(--accent);" \
"padding:8px 18px;border-radius:4px;cursor:pointer;font-family:inherit;font-size:13px}" \
"button:hover{background:var(--accent);color:var(--bg)}" \
"button.danger{color:var(--red);border-color:var(--red)}" \
"button.danger:hover{background:var(--red);color:#fff}" \
"button.primary{background:var(--accent);color:var(--bg);font-weight:bold}" \
"input,select{background:var(--bg);color:var(--text);border:1px solid var(--border);" \
"padding:6px 10px;border-radius:4px;font-family:inherit;font-size:13px}" \
"input:focus,select:focus{border-color:var(--accent);outline:none}" \
"label{color:var(--text2);font-size:12px;display:block;margin-bottom:3px}" \
".row{display:flex;gap:12px;flex-wrap:wrap;align-items:flex-end}" \
".field{flex:1;min-width:120px;margin-bottom:10px}" \
".dot{width:10px;height:10px;border-radius:50%;display:inline-block;margin-right:6px}" \
".dot.on{background:var(--green)}.dot.off{background:#555}.dot.warn{background:var(--yellow)}" \
".toast{position:fixed;top:16px;right:16px;padding:12px 20px;border-radius:6px;" \
"color:#fff;font-size:13px;z-index:999;opacity:0;transition:opacity .3s}" \
".toast.show{opacity:1}" \
".toast.ok{background:#164;border:1px solid var(--green)}" \
".toast.err{background:#411;border:1px solid var(--red)}" \
".wsbar{position:fixed;bottom:0;left:0;right:0;padding:4px 12px;" \
"background:var(--bg2);border-top:1px solid var(--border);font-size:11px;color:var(--text2)}" \
"</style>"

#define JS_HELPERS \
"<script>" \
"function $(id){return document.getElementById(id)}" \
"function safeFetch(url,opts){return fetch(url,opts).then(r=>{if(!r.ok)throw new Error(r.statusText);return r})}" \
"function toast(msg,ok){var t=document.createElement('div');t.className='toast '+(ok?'ok':'err')+' show';" \
"t.textContent=msg;document.body.appendChild(t);setTimeout(()=>{t.classList.remove('show');setTimeout(()=>t.remove(),400)},3000)}" \
"var ws,wsOk=false;" \
"function wsConnect(){" \
"ws=new WebSocket('ws://'+location.host+'/ws');" \
"ws.onopen=function(){wsOk=true;if($('ws-status'))$('ws-status').textContent='ws: connected'};" \
"ws.onclose=function(){wsOk=false;if($('ws-status'))$('ws-status').textContent='ws: disconnected';setTimeout(wsConnect,3000)};" \
"ws.onmessage=function(e){try{var m=JSON.parse(e.data);if(typeof onWsMsg==='function')onWsMsg(m)}catch(ex){}};" \
"}" \
"wsConnect();" \
"</script>"

// ========================================================================
//  PAGE: Dashboard (/)
// ========================================================================

const char PAGE_DASHBOARD[] =
"<!DOCTYPE html><html><head><meta charset='UTF-8'>"
"<meta name='viewport' content='width=device-width,initial-scale=1'>"
"<title>UWB650 Tester</title>"
CSS_COMMON
"<style>"
".range-box{text-align:center;padding:20px}"
".dist{font-size:64px;font-weight:bold;color:var(--green);line-height:1.1}"
".dist.fail{color:var(--red)}"
".rssi{font-size:28px;color:var(--yellow);margin-top:4px}"
".rssi.good{color:var(--green)}.rssi.med{color:var(--yellow)}.rssi.bad{color:var(--red)}"
".target-info{color:var(--text2);font-size:13px;margin-bottom:8px}"
".range-stats{display:flex;gap:20px;justify-content:center;margin-top:12px;font-size:13px;color:var(--text2)}"
".range-stats span{color:var(--text)}"
".start-btn{font-size:18px;padding:12px 40px;margin:12px 0}"
".status-grid{display:grid;grid-template-columns:1fr 1fr;gap:8px;font-size:13px}"
".status-grid .lbl{color:var(--text2)}"
".status-grid .val{color:var(--text);text-align:right}"
"</style>"
"</head><body>"
"<nav>"
"<span class='title'>UWB650 Tester</span>"
"<a href='/' class='active'>Dashboard</a>"
"<a href='/settings'>Settings</a>"
"<a href='/wifi'>WiFi</a>"
"<a href='/datatest'>Data Test</a>"
"</nav>"
"<div class='container'>"

// Ranging card
"<div class='card'>"
"<h2>Ranging</h2>"
"<div class='range-box'>"
"<div class='target-info'>Target: <span id='target'>----:----</span></div>"
"<div class='dist' id='dist'>---</div>"
"<div class='rssi' id='rssi'>--- dBm</div>"
"<div style='margin-top:16px'>"
"<button class='start-btn primary' id='btn-range' onclick='toggleRanging()'>Start</button>"
"</div>"
"<div class='range-stats'>"
"OK: <span id='r-ok'>0</span> / "
"Fail: <span id='r-fail'>0</span> &nbsp; "
"Seq: <span id='r-seq'>0</span>"
"</div>"
"</div>"
"</div>"

// System Status card
"<div class='card'>"
"<h2>System Status</h2>"
"<div class='status-grid'>"
"<div class='lbl'>UWB Module</div><div class='val' id='s-uwb'><span class='dot off'></span>---</div>"
"<div class='lbl'>WiFi STA</div><div class='val' id='s-wifi'><span class='dot off'></span>---</div>"
"<div class='lbl'>Ranging</div><div class='val' id='s-ranging'>Idle</div>"
"<div class='lbl'>Free Heap</div><div class='val' id='s-heap'>---</div>"
"<div class='lbl'>Uptime</div><div class='val' id='s-uptime'>---</div>"
"<div class='lbl'>Version</div><div class='val' id='s-ver'>v---</div>"
"<div class='lbl'>WS Clients</div><div class='val' id='s-ws'>0</div>"
"</div>"
"</div>"

// Stats card
"<div class='card'>"
"<h2>UWB Statistics</h2>"
"<div class='status-grid'>"
"<div class='lbl'>TX</div><div class='val' id='st-tx'>0</div>"
"<div class='lbl'>RX</div><div class='val' id='st-rx'>0</div>"
"<div class='lbl'>OK</div><div class='val' id='st-ok'>0</div>"
"<div class='lbl'>Errors</div><div class='val' id='st-err'>0</div>"
"<div class='lbl'>Timeouts</div><div class='val' id='st-tmo'>0</div>"
"<div class='lbl'>Ranging OK</div><div class='val' id='st-rok'>0</div>"
"<div class='lbl'>Ranging Fail</div><div class='val' id='st-rfail'>0</div>"
"</div>"
"<div style='margin-top:10px;text-align:right'>"
"<button onclick='resetStats()'>Reset Stats</button>"
"</div>"
"</div>"

"</div>" // container

"<div class='wsbar'><span id='ws-status'>ws: connecting...</span></div>"

JS_HELPERS

"<script>"
"var ranging=false,rOk=0,rFail=0;"

"function toggleRanging(){"
"if(ranging){"
"safeFetch('/api/ranging/stop',{method:'POST'}).then(()=>{ranging=false;$('btn-range').textContent='Start';$('btn-range').classList.add('primary');toast('Ranging stopped',true)}).catch(e=>toast('Error: '+e,false));"
"}else{"
"rOk=0;rFail=0;"
"safeFetch('/api/ranging/start',{method:'POST'}).then(()=>{ranging=true;$('btn-range').textContent='Stop';$('btn-range').classList.remove('primary');toast('Ranging started',true)}).catch(e=>toast('Error: '+e,false));"
"}}"

"function onWsMsg(m){"
"if(m.type==='ranging'){"
"var d=m.data;"
"if(d.valid){rOk++;$('dist').textContent=d.distance.toFixed(3)+' m';$('dist').className='dist';"
"}else{rFail++;$('dist').textContent='FAIL';$('dist').className='dist fail'}"
"var rssiVal=d.rssi;"
"$('rssi').textContent=rssiVal.toFixed(1)+' dBm';"
"$('rssi').className='rssi '+(rssiVal>-60?'good':(rssiVal>-80?'med':'bad'));"
"$('r-ok').textContent=rOk;$('r-fail').textContent=rFail;$('r-seq').textContent=d.seq;"
"}"
"if(m.type==='status'){"
"var d=m.data;"
"$('s-uwb').innerHTML='<span class=\"dot '+(d.uwb?'on':'off')+'\"></span>'+(d.uwb?'Ready':'Offline');"
"$('s-wifi').innerHTML='<span class=\"dot '+(d.wifi?'on':'off')+'\"></span>'+(d.wifi?'Connected':'Disconnected');"
"$('s-ranging').textContent=d.ranging;"
"$('s-heap').textContent=(d.heap/1024).toFixed(0)+' KB';"
"var s=d.uptime;var h=Math.floor(s/3600);var m2=Math.floor((s%3600)/60);var ss=s%60;"
"$('s-uptime').textContent=h+'h '+m2+'m '+ss+'s';"
"}}"

// Load initial data
"function loadStatus(){"
"safeFetch('/api/status').then(r=>r.json()).then(d=>{"
"$('s-uwb').innerHTML='<span class=\"dot '+(d.uwbReady?'on':'off')+'\"></span>'+(d.uwbReady?'Ready':'Offline');"
"$('s-wifi').innerHTML='<span class=\"dot '+(d.wifiSta?'on':'off')+'\"></span>'+(d.wifiSta?'Connected':'Disconnected');"
"$('s-ranging').textContent=d.rangingState;"
"$('s-heap').textContent=(d.freeHeap/1024).toFixed(0)+' KB';"
"var s=Math.floor(d.uptime);var h=Math.floor(s/3600);var m2=Math.floor((s%3600)/60);var ss=s%60;"
"$('s-uptime').textContent=h+'h '+m2+'m '+ss+'s';"
"$('s-ws').textContent=d.wsClients;$('s-ver').textContent='v'+d.version;"
"if(d.rangingState==='running'){ranging=true;$('btn-range').textContent='Stop';$('btn-range').classList.remove('primary')}"
"}).catch(()=>{})}"

"function loadConfig(){"
"safeFetch('/api/config').then(r=>r.json()).then(d=>{"
"$('target').textContent=d.targetPanId+':'+d.targetAddr;"
"}).catch(()=>{})}"

"function loadStats(){"
"safeFetch('/api/uwb/stats').then(r=>r.json()).then(d=>{"
"$('st-tx').textContent=d.txCount;$('st-rx').textContent=d.rxCount;"
"$('st-ok').textContent=d.okCount;$('st-err').textContent=d.errorCount;"
"$('st-tmo').textContent=d.timeoutCount;"
"$('st-rok').textContent=d.rangingOk;$('st-rfail').textContent=d.rangingFail;"
"}).catch(()=>{})}"

"function resetStats(){"
"safeFetch('/api/uwb/stats/reset',{method:'POST'}).then(()=>{toast('Stats reset',true);loadStats()}).catch(e=>toast('Error',false))}"

"loadStatus();loadConfig();loadStats();"
"setInterval(loadStatus,5000);"
"setInterval(loadStats,3000);"
"</script>"
"</body></html>";

// ========================================================================
//  PAGE: Settings (/settings)
// ========================================================================

const char PAGE_SETTINGS[] =
"<!DOCTYPE html><html><head><meta charset='UTF-8'>"
"<meta name='viewport' content='width=device-width,initial-scale=1'>"
"<title>UWB650 Settings</title>"
CSS_COMMON
"<style>"
".section{color:var(--accent);font-size:12px;margin:14px 0 6px;border-bottom:1px solid var(--border);padding-bottom:4px}"
".switch{display:flex;align-items:center;gap:8px;margin-bottom:8px;font-size:13px}"
".switch input[type=checkbox]{width:16px;height:16px;accent-color:var(--accent)}"
".btn-row{display:flex;gap:8px;flex-wrap:wrap;margin-top:14px}"
".console-out{background:var(--bg);border:1px solid var(--border);padding:8px;min-height:60px;"
"font-size:12px;color:var(--green);white-space:pre-wrap;margin-top:8px;border-radius:4px;max-height:200px;overflow-y:auto}"
"</style>"
"</head><body>"
"<nav>"
"<span class='title'>UWB650 Tester</span>"
"<a href='/'>Dashboard</a>"
"<a href='/settings' class='active'>Settings</a>"
"<a href='/wifi'>WiFi</a>"
"<a href='/datatest'>Data Test</a>"
"</nav>"
"<div class='container'>"

// UWB650 Configuration
"<div class='card'>"
"<h2>UWB650 Module Configuration</h2>"

// Communication
"<div class='section'>Communication</div>"
"<div class='row'>"
"<div class='field'><label>Baud Rate</label>"
"<select id='baudrate'>"
"<option value='0'>230400</option>"
"<option value='1' selected>115200</option>"
"<option value='2'>57600</option>"
"<option value='3'>38400</option>"
"<option value='4'>19200</option>"
"<option value='5'>9600</option>"
"</select></div>"
"<div class='field'><label>Data Rate</label>"
"<select id='dataRate'>"
"<option value='0'>850 Kbps</option>"
"<option value='1' selected>6.8 Mbps</option>"
"</select></div>"
"</div>"

// Identity
"<div class='section'>Identity</div>"
"<div class='row'>"
"<div class='field'><label>PAN ID (hex)</label><input id='panId' maxlength='4' value='0000' style='width:80px'></div>"
"<div class='field'><label>Node Address (hex)</label><input id='nodeAddr' maxlength='4' value='0000' style='width:80px'></div>"
"</div>"

// RF Parameters
"<div class='section'>RF Parameters</div>"
"<div class='row'>"
"<div class='field'><label>TX Power</label>"
"<select id='txPower'>"
"<option value='0'>0: -5.0 dBm</option>"
"<option value='1'>1: -2.5 dBm</option>"
"<option value='2'>2: 0.0 dBm</option>"
"<option value='3'>3: 2.5 dBm</option>"
"<option value='4'>4: 5.0 dBm</option>"
"<option value='5'>5: 7.5 dBm</option>"
"<option value='6'>6: 10.0 dBm</option>"
"<option value='7'>7: 12.5 dBm</option>"
"<option value='8'>8: 15.0 dBm</option>"
"<option value='9'>9: 20.0 dBm</option>"
"<option value='10' selected>10: 27.7 dBm (max)</option>"
"</select></div>"
"<div class='field'><label>Preamble Code (9-24)</label>"
"<select id='preambleCode'>"
"<option value='9' selected>9</option><option value='10'>10</option>"
"<option value='11'>11</option><option value='12'>12</option>"
"<option value='13'>13</option><option value='14'>14</option>"
"<option value='15'>15</option><option value='16'>16</option>"
"<option value='17'>17</option><option value='18'>18</option>"
"<option value='19'>19</option><option value='20'>20</option>"
"<option value='21'>21</option><option value='22'>22</option>"
"<option value='23'>23</option><option value='24'>24</option>"
"</select></div>"
"</div>"
"<div class='row'>"
"<div class='field'><label>Antenna Delay (0-65535)</label><input type='number' id='antDelay' min='0' max='65535' value='16400' style='width:100px'></div>"
"<div class='field'><label>Dist Offset, cm (-500..500)</label><input type='number' id='distOffsetCm' min='-500' max='500' value='0' style='width:100px'></div>"
"</div>"

// Features (toggles)
"<div class='section'>Features</div>"
"<div class='switch'><input type='checkbox' id='ccaEnable'><label style='display:inline'>CCA (Clear Channel Assessment)</label></div>"
"<div class='switch'><input type='checkbox' id='ackEnable'><label style='display:inline'>ACK Enable</label></div>"
"<div class='switch'><input type='checkbox' id='securityEnable' onchange='$(\"secKeyRow\").style.display=this.checked?\"flex\":\"none\"'>"
"<label style='display:inline'>AES-128 Security</label></div>"
"<div class='row' id='secKeyRow' style='display:none'>"
"<div class='field'><label>AES Key (32 hex chars)</label><input id='securityKey' maxlength='32' placeholder='00112233445566778899AABBCCDDEEFF' style='width:300px'></div>"
"</div>"
"<div class='switch'><input type='checkbox' id='rxShowSrc' checked><label style='display:inline'>RX Show Source Address</label></div>"
"<div class='switch'><input type='checkbox' id='ledStatus' checked><label style='display:inline'>LED Status</label></div>"
"<div class='switch'><input type='checkbox' id='rxEnable' checked><label style='display:inline'>RX Enable</label></div>"
"<div class='switch'><input type='checkbox' id='sniffEnable'><label style='display:inline'>Sniff Mode</label></div>"

// Coordinates
"<div class='section'>Coordinates (cm)</div>"
"<div class='row'>"
"<div class='field'><label>X</label><input type='number' id='coordX' value='0' style='width:80px'></div>"
"<div class='field'><label>Y</label><input type='number' id='coordY' value='0' style='width:80px'></div>"
"<div class='field'><label>Z</label><input type='number' id='coordZ' value='0' style='width:80px'></div>"
"</div>"

// Ranging Target
"<div class='section'>Ranging Target</div>"
"<div class='row'>"
"<div class='field'><label>Target PAN ID (hex)</label><input id='targetPanId' maxlength='4' value='0000' style='width:80px'></div>"
"<div class='field'><label>Target Address (hex)</label><input id='targetAddr' maxlength='4' value='0001' style='width:80px'></div>"
"</div>"

// Action buttons
"<div class='btn-row'>"
"<button class='primary' onclick='saveConfig()'>Save to NVS</button>"
"<button onclick='applyConfig()'>Apply to Module</button>"
"<button onclick='flashSave()'>Save to Module Flash</button>"
"<button onclick='readFromModule()'>Read from Module</button>"
"<button class='danger' onclick='factoryDefault()'>Factory Default</button>"
"</div>"

"</div>" // card

// Raw Command Console
"<div class='card'>"
"<h2>Raw AT Command Console</h2>"
"<div class='row'>"
"<div class='field' style='flex:3'><label>Command (without UWBRFAT+ prefix)</label>"
"<input id='raw-cmd' placeholder='POWER?' style='width:100%'></div>"
"<div class='field' style='flex:0'><label>&nbsp;</label><button onclick='sendRaw()'>Send</button></div>"
"</div>"
"<div class='console-out' id='raw-out'>Ready.\n</div>"
"</div>"

// UART Bridge
"<div class='card'>"
"<h2>UART Bridge (TCP:3333)</h2>"
"<div style='font-size:13px;margin-bottom:8px'>"
"Status: <span id='br-status' style='color:var(--text2)'>---</span>"
"</div>"
"<button id='br-start' onclick='bridgeStart()'>Start Bridge</button>"
"<button id='br-stop' onclick='bridgeStop()' style='margin-left:6px;display:none'>Stop Bridge</button>"
"<div style='font-size:12px;color:var(--text2);margin-top:8px'>"
"Connect with TCP client to <span id='br-addr'>IP</span>:3333<br>"
"Use HW VSP / com0com for virtual COM port, or PuTTY raw TCP mode"
"</div>"
"</div>"

// OTA Firmware Update
"<div class='card'>"
"<h2>Firmware Update (OTA)</h2>"
"<div class='row'>"
"<div class='field' style='flex:3'><label>Firmware binary (.bin)</label>"
"<input type='file' id='ota-file' accept='.bin' style='width:100%'></div>"
"<div class='field' style='flex:0'><label>&nbsp;</label>"
"<button class='danger' onclick='uploadOta()' id='ota-btn'>Upload &amp; Flash</button></div>"
"</div>"
"<div id='ota-progress' style='display:none;margin-top:8px'>"
"<div style='background:var(--bg);border:1px solid var(--border);border-radius:4px;height:20px;overflow:hidden'>"
"<div id='ota-bar' style='height:100%;background:var(--accent);width:0%;transition:width .3s'></div>"
"</div>"
"<div style='font-size:12px;color:var(--text2);margin-top:4px'><span id='ota-status'>Uploading...</span></div>"
"</div>"
"</div>"

"</div>" // container

"<div class='wsbar'><span id='ws-status'>ws: connecting...</span></div>"

JS_HELPERS

"<script>"
// Load config into form
"function loadConfig(){"
"safeFetch('/api/config').then(r=>r.json()).then(d=>{"
"$('baudrate').value=d.baudrate;"
"$('dataRate').value=d.dataRate;"
"$('panId').value=d.panId;"
"$('nodeAddr').value=d.nodeAddr;"
"$('txPower').value=d.txPower;"
"$('preambleCode').value=d.preambleCode;"
"$('antDelay').value=d.antDelay;"
"$('distOffsetCm').value=d.distOffsetCm;"
"$('ccaEnable').checked=!!d.ccaEnable;"
"$('ackEnable').checked=!!d.ackEnable;"
"$('securityEnable').checked=!!d.securityEnable;"
"$('securityKey').value=d.securityKey||'';"
"$('secKeyRow').style.display=d.securityEnable?'flex':'none';"
"$('rxShowSrc').checked=!!d.rxShowSrc;"
"$('ledStatus').checked=!!d.ledStatus;"
"$('rxEnable').checked=!!d.rxEnable;"
"$('sniffEnable').checked=!!d.sniffEnable;"
"$('coordX').value=d.coordX;"
"$('coordY').value=d.coordY;"
"$('coordZ').value=d.coordZ;"
"$('targetPanId').value=d.targetPanId;"
"$('targetAddr').value=d.targetAddr;"
"}).catch(e=>toast('Failed to load config',false))}"

// Collect form data
"function getFormData(){"
"return{"
"baudrate:parseInt($('baudrate').value),"
"dataRate:parseInt($('dataRate').value),"
"panId:$('panId').value.toUpperCase(),"
"nodeAddr:$('nodeAddr').value.toUpperCase(),"
"txPower:parseInt($('txPower').value),"
"preambleCode:parseInt($('preambleCode').value),"
"antDelay:parseInt($('antDelay').value),"
"distOffsetCm:parseInt($('distOffsetCm').value),"
"ccaEnable:$('ccaEnable').checked?1:0,"
"ackEnable:$('ackEnable').checked?1:0,"
"securityEnable:$('securityEnable').checked?1:0,"
"securityKey:$('securityKey').value,"
"rxShowSrc:$('rxShowSrc').checked?1:0,"
"ledStatus:$('ledStatus').checked?1:0,"
"rxEnable:$('rxEnable').checked?1:0,"
"sniffEnable:$('sniffEnable').checked?1:0,"
"coordX:parseInt($('coordX').value)||0,"
"coordY:parseInt($('coordY').value)||0,"
"coordZ:parseInt($('coordZ').value)||0,"
"targetPanId:$('targetPanId').value.toUpperCase(),"
"targetAddr:$('targetAddr').value.toUpperCase()"
"}}"

"function saveConfig(){"
"safeFetch('/api/config',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify(getFormData())})"
".then(r=>r.json()).then(d=>{toast(d.message,d.success)}).catch(e=>toast('Save failed',false))}"

"function applyConfig(){"
"safeFetch('/api/config/apply',{method:'POST'})"
".then(r=>r.json()).then(d=>{toast(d.message,d.success)}).catch(e=>toast('Apply failed',false))}"

"function flashSave(){"
"safeFetch('/api/uwb/command',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({cmd:'FLASH'})})"
".then(r=>r.json()).then(d=>{toast(d.success?'Saved to module flash':'Flash save failed',d.success)}).catch(e=>toast('Error',false))}"

"function readFromModule(){"
"var params=['BAUDRATE','DATARATE','DEVICEID','POWER','PREAMBLECODE','CCAENABLE','ACKENABLE',"
"'RXSHOWSRC','LEDSTATUS','RXENABLE','SNIFFEN','ANTDELAY','DISTOFFSET','SECURITY'];"
"var out='Reading from module...\\n';"
"$('raw-out').textContent=out;"
"var chain=Promise.resolve();"
"params.forEach(function(p){"
"chain=chain.then(function(){return safeFetch('/api/uwb/query',{method:'POST',headers:{'Content-Type':'application/json'},"
"body:JSON.stringify({param:p})}).then(r=>r.json()).then(d=>{"
"out+=p+' = '+d.value+'\\n';$('raw-out').textContent=out;"
"})})});"
"chain.then(function(){out+='\\nDone.\\n';$('raw-out').textContent=out;toast('Read complete',true)})"
".catch(e=>{toast('Read failed',false)})}"

"function factoryDefault(){"
"if(!confirm('Reset UWB650 module to factory defaults?'))return;"
"safeFetch('/api/uwb/command',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({cmd:'DEFAULT'})})"
".then(r=>r.json()).then(d=>{toast(d.success?'Module reset to defaults':'Failed',d.success);if(d.success)setTimeout(loadConfig,2000)})"
".catch(e=>toast('Error',false))}"

"function sendRaw(){"
"var cmd=$('raw-cmd').value.trim();"
"if(!cmd)return;"
"var out=$('raw-out').textContent;"
"out+='> '+cmd+'\\n';$('raw-out').textContent=out;"
"safeFetch('/api/uwb/command',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({cmd:cmd})})"
".then(r=>r.json()).then(d=>{"
"out+=(d.response||'(no data)')+'\\n'+(d.success?'OK':'ERROR')+'\\n';$('raw-out').textContent=out;"
"$('raw-out').scrollTop=$('raw-out').scrollHeight;"
"}).catch(e=>{out+='ERROR: '+e+'\\n';$('raw-out').textContent=out})}"

// Enter key to send
"$('raw-cmd').addEventListener('keydown',function(e){if(e.key==='Enter')sendRaw()});"

"function bridgeStart(){"
"safeFetch('/api/bridge/start',{method:'POST'}).then(r=>r.json()).then(d=>{"
"toast(d.message||d.error,d.success);bridgeStatus()}).catch(e=>toast('Error',false))}"

"function bridgeStop(){"
"safeFetch('/api/bridge/stop',{method:'POST'}).then(r=>r.json()).then(d=>{"
"toast(d.message,d.success);bridgeStatus()}).catch(e=>toast('Error',false))}"

"function bridgeStatus(){"
"safeFetch('/api/bridge/status').then(r=>r.json()).then(d=>{"
"var s=d.active?(d.clientConnected?'Active (client connected)':'Active (waiting for client)'):'Stopped';"
"$('br-status').textContent=s;"
"$('br-status').style.color=d.active?'var(--green)':'var(--text2)';"
"$('br-start').style.display=d.active?'none':'inline';"
"$('br-stop').style.display=d.active?'inline':'none';"
"}).catch(()=>{})}"

"$('br-addr').textContent=location.hostname;"
"bridgeStatus();setInterval(bridgeStatus,3000);"

"function uploadOta(){"
"var file=$('ota-file').files[0];"
"if(!file){toast('Select a .bin file first',false);return}"
"if(!file.name.endsWith('.bin')){toast('File must be .bin',false);return}"
"if(!confirm('Upload '+file.name+' ('+Math.round(file.size/1024)+' KB) and flash?'))return;"
"$('ota-btn').disabled=true;"
"$('ota-progress').style.display='block';"
"$('ota-bar').style.width='0%';"
"$('ota-status').textContent='Uploading...';"
"var xhr=new XMLHttpRequest();"
"xhr.open('POST','/api/system/ota');"
"xhr.setRequestHeader('Content-Type','application/octet-stream');"
"xhr.upload.onprogress=function(e){"
"if(e.lengthComputable){var pct=Math.round(e.loaded*100/e.total);"
"$('ota-bar').style.width=pct+'%';"
"$('ota-status').textContent='Uploading... '+pct+'%'}"
"};"
"xhr.onload=function(){"
"if(xhr.status===200){"
"$('ota-bar').style.width='100%';"
"$('ota-status').textContent='Firmware uploaded! Device is rebooting...';"
"toast('OTA success! Rebooting...',true);"
"setTimeout(function(){location.reload()},8000)"
"}else{"
"try{var r=JSON.parse(xhr.responseText);toast('OTA failed: '+r.error,false)}catch(e){toast('OTA failed',false)}"
"$('ota-btn').disabled=false;$('ota-status').textContent='Failed'}"
"};"
"xhr.onerror=function(){toast('Upload error',false);$('ota-btn').disabled=false;$('ota-status').textContent='Error'};"
"xhr.send(file);"
"}"

"loadConfig();"
"</script>"
"</body></html>";

// ========================================================================
//  PAGE: WiFi (/wifi)
// ========================================================================

const char PAGE_WIFI[] =
"<!DOCTYPE html><html><head><meta charset='UTF-8'>"
"<meta name='viewport' content='width=device-width,initial-scale=1'>"
"<title>UWB650 WiFi</title>"
CSS_COMMON
"<style>"
".ap-list{width:100%}"
".ap-list tr{border-bottom:1px solid var(--border)}"
".ap-list td,.ap-list th{padding:6px 10px;text-align:left;font-size:13px}"
".ap-list th{color:var(--text2);font-weight:normal;font-size:11px}"
".ap-list tr:hover{background:rgba(0,255,68,0.05);cursor:pointer}"
"</style>"
"</head><body>"
"<nav>"
"<span class='title'>UWB650 Tester</span>"
"<a href='/'>Dashboard</a>"
"<a href='/settings'>Settings</a>"
"<a href='/wifi' class='active'>WiFi</a>"
"</nav>"
"<div class='container'>"

// Current Status
"<div class='card'>"
"<h2>Current WiFi Status</h2>"
"<div class='status-grid'>"
"<div class='lbl'>AP SSID</div><div class='val' id='ap-ssid'>---</div>"
"<div class='lbl'>AP IP</div><div class='val' id='ap-ip'>192.168.4.1</div>"
"<div class='lbl'>STA Status</div><div class='val' id='sta-status'>---</div>"
"<div class='lbl'>STA IP</div><div class='val' id='sta-ip'>---</div>"
"</div>"
"</div>"

// Connect
"<div class='card'>"
"<h2>Connect to Network</h2>"
"<div class='row'>"
"<div class='field'><label>SSID</label><input id='sta-ssid' style='width:200px'></div>"
"<div class='field'><label>Password</label><input id='sta-pass' type='password' style='width:200px'></div>"
"<div class='field'><label>&nbsp;</label>"
"<button class='primary' onclick='connectWifi()'>Connect</button>"
"<button onclick='disconnectWifi()' style='margin-left:6px'>Disconnect</button>"
"</div>"
"</div>"
"</div>"

// Scan
"<div class='card'>"
"<h2>Available Networks <button onclick='scanWifi()' style='font-size:11px;padding:4px 10px;margin-left:10px'>Scan</button></h2>"
"<table class='ap-list'>"
"<thead><tr><th>SSID</th><th>RSSI</th><th>Channel</th><th>Auth</th></tr></thead>"
"<tbody id='scan-body'><tr><td colspan='4' style='color:var(--text2)'>Press Scan to search...</td></tr></tbody>"
"</table>"
"</div>"

"</div>" // container

"<div class='wsbar'><span id='ws-status'>ws: connecting...</span></div>"

JS_HELPERS

"<script>"
"var authModes=['Open','WEP','WPA','WPA2','WPA/WPA2','WPA3','WPA2/WPA3','WAPI','OWE'];"

"function loadWifiStatus(){"
"safeFetch('/api/status').then(r=>r.json()).then(d=>{"
"$('ap-ssid').textContent=d.apSsid;$('ap-ip').textContent=d.apIp;"
"$('sta-status').innerHTML='<span class=\"dot '+(d.wifiSta?'on':'off')+'\"></span>'+(d.wifiSta?'Connected':'Disconnected');"
"$('sta-ip').textContent=d.staIp||'---';"
"}).catch(()=>{})}"

"function scanWifi(){"
"$('scan-body').innerHTML='<tr><td colspan=\"4\" style=\"color:var(--text2)\">Scanning...</td></tr>';"
"safeFetch('/api/wifi/scan').then(r=>r.json()).then(list=>{"
"if(!list.length){$('scan-body').innerHTML='<tr><td colspan=\"4\">No networks found</td></tr>';return}"
"var html='';"
"list.forEach(function(ap){"
"html+='<tr onclick=\"$(\\x27sta-ssid\\x27).value=\\x27'+ap.ssid+'\\x27\"><td>'+ap.ssid+'</td><td>'+ap.rssi+' dBm</td><td>'+ap.channel+'</td><td>'+(authModes[ap.auth]||ap.auth)+'</td></tr>';"
"});"
"$('scan-body').innerHTML=html;"
"}).catch(e=>{$('scan-body').innerHTML='<tr><td colspan=\"4\">Scan failed</td></tr>'})}"

"function connectWifi(){"
"var ssid=$('sta-ssid').value.trim();if(!ssid){toast('Enter SSID',false);return}"
"safeFetch('/api/wifi/connect',{method:'POST',headers:{'Content-Type':'application/json'},"
"body:JSON.stringify({ssid:ssid,password:$('sta-pass').value})})"
".then(r=>r.json()).then(d=>{toast(d.message,d.success);setTimeout(loadWifiStatus,3000)})"
".catch(e=>toast('Error',false))}"

"function disconnectWifi(){"
"safeFetch('/api/wifi/disconnect',{method:'POST'}).then(()=>{toast('Disconnected',true);loadWifiStatus()}).catch(e=>toast('Error',false))}"

"loadWifiStatus();"
"setInterval(loadWifiStatus,5000);"
"</script>"
"</body></html>";

// ========================================================================
//  PAGE: Data Test (/datatest)
// ========================================================================

const char PAGE_DATATEST[] =
"<!DOCTYPE html><html><head><meta charset='UTF-8'>"
"<meta name='viewport' content='width=device-width,initial-scale=1'>"
"<title>UWB650 Data Test</title>"
CSS_COMMON
"<style>"
".console-out{background:var(--bg);border:1px solid var(--border);padding:8px;min-height:100px;"
"font-size:12px;color:var(--green);white-space:pre-wrap;margin-top:8px;border-radius:4px;max-height:300px;overflow-y:auto}"
".stat-val{font-size:24px;font-weight:bold;color:var(--accent)}"
".stat-label{font-size:11px;color:var(--text2)}"
".stat-box{text-align:center;padding:10px}"
".stats-row{display:grid;grid-template-columns:repeat(4,1fr);gap:8px}"
"</style>"
"</head><body>"
"<nav>"
"<span class='title'>UWB650 Tester</span>"
"<a href='/'>Dashboard</a>"
"<a href='/settings'>Settings</a>"
"<a href='/wifi'>WiFi</a>"
"<a href='/datatest' class='active'>Data Test</a>"
"</nav>"
"<div class='container'>"

// Control card
"<div class='card'>"
"<h2>UWB Data Transmission Test</h2>"
"<div class='row'>"
"<div class='field'><label>Mode</label>"
"<select id='mode'><option value='tx'>TX (Send)</option><option value='rx'>RX (Receive)</option></select></div>"
"<div class='field'><label>Target Address (hex)</label>"
"<input id='target' value='0001' maxlength='4' style='width:80px'></div>"
"<div class='field'><label>Interval (ms)</label>"
"<input type='number' id='interval' value='200' min='50' max='5000' style='width:80px'></div>"
"<div class='field'><label>&nbsp;</label>"
"<button class='primary' id='btn-start' onclick='startTest()'>Start</button>"
"<button id='btn-stop' onclick='stopTest()' style='margin-left:6px;display:none'>Stop</button></div>"
"</div>"
"<div style='font-size:12px;color:var(--text2);margin-top:6px'>"
"TX: sends simulated NMEA coordinates to target via UWB<br>"
"RX: listens for incoming UWB data and displays it"
"</div>"
"</div>"

// Stats card
"<div class='card'>"
"<h2>Statistics</h2>"
"<div class='stats-row'>"
"<div class='stat-box'><div class='stat-val' id='st-pkt'>0</div><div class='stat-label'>Packets</div></div>"
"<div class='stat-box'><div class='stat-val' id='st-bytes'>0</div><div class='stat-label'>Bytes</div></div>"
"<div class='stat-box'><div class='stat-val' id='st-rate'>0</div><div class='stat-label'>Bytes/sec</div></div>"
"<div class='stat-box'><div class='stat-val' id='st-time'>0s</div><div class='stat-label'>Elapsed</div></div>"
"</div>"
"</div>"

// Received data console (for RX mode)
"<div class='card' id='rx-card' style='display:none'>"
"<h2>Received Data</h2>"
"<div class='console-out' id='rx-out'></div>"
"</div>"

"</div>" // container

"<div class='wsbar'><span id='ws-status'>ws: connecting...</span></div>"

JS_HELPERS

"<script>"
"var running=false,curMode='tx',pollTimer=null;"

"function startTest(){"
"var mode=$('mode').value;"
"var body={mode:mode};"
"if(mode==='tx'){"
"body.targetAddr=$('target').value.toUpperCase();"
"body.intervalMs=parseInt($('interval').value)||200;"
"}"
"safeFetch('/api/datatest/start',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify(body)})"
".then(r=>r.json()).then(d=>{"
"if(d.success){"
"running=true;curMode=mode;"
"$('btn-start').style.display='none';$('btn-stop').style.display='inline';"
"$('rx-card').style.display=(mode==='rx')?'block':'none';"
"$('rx-out').textContent='';"
"toast(d.message,true);"
"pollTimer=setInterval(pollStats,1000);"
"}else{toast(d.error||'Failed',false)}"
"}).catch(e=>toast('Error: '+e,false))}"

"function stopTest(){"
"safeFetch('/api/datatest/stop',{method:'POST'})"
".then(r=>r.json()).then(d=>{"
"running=false;"
"$('btn-start').style.display='inline';$('btn-stop').style.display='none';"
"if(pollTimer){clearInterval(pollTimer);pollTimer=null}"
"toast(d.message,true);"
"pollStats();"
"}).catch(e=>toast('Error: '+e,false))}"

"function pollStats(){"
"safeFetch('/api/datatest/status').then(r=>r.json()).then(d=>{"
"var isTx=(d.state==='tx');"
"var pkts=isTx?d.packetsSent:d.packetsReceived;"
"var bytes=isTx?d.bytesSent:d.bytesReceived;"
"var rate=isTx?d.txBytesPerSec:d.rxBytesPerSec;"
"$('st-pkt').textContent=pkts;"
"$('st-bytes').textContent=bytes>1024?(bytes/1024).toFixed(1)+'K':bytes;"
"$('st-rate').textContent=rate>1024?(rate/1024).toFixed(1)+'K':Math.round(rate);"
"var s=Math.floor(d.elapsedMs/1000);$('st-time').textContent=s+'s';"
"if(d.state==='idle'&&running){"
"running=false;$('btn-start').style.display='inline';$('btn-stop').style.display='none';"
"if(pollTimer){clearInterval(pollTimer);pollTimer=null}}"
"}).catch(()=>{})}"

"function onWsMsg(m){"
"if(m.type==='datatest'&&curMode==='rx'){"
"var el=$('rx-out');"
"el.textContent+=m.data+'\\n';"
"if(el.scrollHeight>el.clientHeight)el.scrollTop=el.scrollHeight;"
"}}"

// Load initial state
"safeFetch('/api/datatest/status').then(r=>r.json()).then(d=>{"
"if(d.state!=='idle'){"
"running=true;curMode=d.state;"
"$('mode').value=d.state;"
"$('btn-start').style.display='none';$('btn-stop').style.display='inline';"
"$('rx-card').style.display=(d.state==='rx')?'block':'none';"
"pollTimer=setInterval(pollStats,1000);"
"pollStats();"
"}}).catch(()=>{});"
"</script>"
"</body></html>";

// ========================================================================
//  Favicon (SVG)
// ========================================================================

const char PAGE_FAVICON[] =
"<svg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 32 32'>"
"<rect width='32' height='32' rx='4' fill='#0a0a1a'/>"
"<circle cx='16' cy='16' r='4' fill='#0f4'/>"
"<circle cx='16' cy='16' r='8' fill='none' stroke='#0f4' stroke-width='1.5' opacity='.6'/>"
"<circle cx='16' cy='16' r='12' fill='none' stroke='#0f4' stroke-width='1' opacity='.3'/>"
"</svg>";
