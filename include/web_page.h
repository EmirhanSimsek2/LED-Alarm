#pragma once
#include <Arduino.h>

// The web page served to the phone. Stored in flash as a raw string.
const char INDEX_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>LED Alarm</title>
<style>
  *{box-sizing:border-box}
  body{font-family:system-ui,-apple-system,sans-serif;background:#111;color:#eee;margin:0 auto;padding:16px;max-width:480px}
  h1{font-size:1.3em;margin:4px 0 14px}
  h2{font-size:1.1em;margin:0 0 10px}
  .card{background:#1e1e1e;border-radius:14px;padding:16px;margin-bottom:14px}
  #clock{font-size:2.6em;text-align:center;font-variant-numeric:tabular-nums}
  #date,#nextAlarm{text-align:center;color:#aaa;font-size:.95em;margin-top:2px}
  #nextAlarm{color:#90caf9;margin-top:6px}
  .warn{color:#ff8a65;text-align:center;font-size:.9em;margin-top:6px}
  .note{color:#aaa;text-align:center;font-size:.9em;margin-top:8px}
  button{font-size:1em;padding:12px;border:0;border-radius:10px;width:100%;margin-top:8px;background:#333;color:#eee;cursor:pointer}
  button:active{opacity:.7}
  #panelBtn{font-size:1.3em;padding:20px;margin-top:0}
  #panelBtn.on{background:#f5c518;color:#111;font-weight:600}
  #alarmBox{background:#3a2a00;border-radius:10px;padding:10px;margin-top:10px;text-align:center}
  .row{display:flex;align-items:center;justify-content:space-between;padding:7px 0;border-bottom:1px solid #2a2a2a;gap:10px}
  .row:last-child{border-bottom:none}
  .row label{display:flex;align-items:center;gap:10px;flex:1}
  input[type=checkbox]{width:22px;height:22px}
  input[type=time],input[type=date],input[type=number]{font-size:1em;padding:7px;background:#2a2a2a;color:#eee;border:1px solid #444;border-radius:8px}
  input[type=number]{width:84px;text-align:center}
  .btn-row{display:flex;gap:8px;margin-top:6px}
  .btn-row button{flex:1;font-size:.9em;padding:10px;background:#2a2a2a}
  .danger{background:#5c2222 !important}
  .primary{background:#1976d2}
  .toast{text-align:center;color:#8bc34a;min-height:1.2em;margin-top:6px;font-size:.9em}
  details{margin-top:10px;border-top:1px solid #2a2a2a;padding-top:8px}
  summary{color:#aaa;font-size:.9em;cursor:pointer}
  .man-grid{display:flex;gap:8px;margin-top:8px}
  .man-grid input{flex:1}
</style>
</head>
<body>
<h1>&#128161; LED Alarm</h1>

<div class="card">
  <div id="clock">--:--:--</div>
  <div id="date"></div>
  <div id="nextAlarm"></div>
  <div id="warn" class="warn"></div>
  <button onclick="syncTime(true)">Sync time from phone</button>
  <div id="timeMsg" class="toast"></div>
  <details>
    <summary>Manual date &amp; time</summary>
    <div class="man-grid">
      <input type="date" id="manDate">
      <input type="time" id="manTime">
    </div>
    <button onclick="setManualTime()">Set clock manually</button>
  </details>
</div>

<div class="card">
  <button id="panelBtn" onclick="togglePanel()">Panel</button>
  <div id="panelNote" class="note" hidden></div>
  <div id="alarmBox" hidden>
    <div id="alarmText"></div>
    <button id="snoozeBtn" onclick="post('/api/snooze')">Snooze</button>
    <button onclick="post('/api/dismiss')">Stop alarm</button>
  </div>
</div>

<div class="card">
  <h2>Alarms</h2>
  <div id="days"></div>
  <div class="btn-row">
    <button onclick="copyMonToFri()">Copy Mon &rarr; Fri</button>
    <button class="danger" onclick="disableAll()">Disable all</button>
  </div>
  <button class="primary" onclick="saveAlarms()">Save alarms</button>
  <div id="saved" class="toast"></div>
</div>

<div class="card">
  <h2>Settings</h2>
  <div class="row">
    <label for="alarmMin">Alarm duration (min, 0 = manual)</label>
    <input type="number" id="alarmMin" min="0" max="180">
  </div>
  <div class="row">
    <label for="snoozeMin">Snooze duration (min)</label>
    <input type="number" id="snoozeMin" min="1" max="60">
  </div>
  <div class="row">
    <label for="manualOffMin">Manual auto-off (min, 0 = off)</label>
    <input type="number" id="manualOffMin" min="0" max="180">
  </div>
  <button class="primary" onclick="saveSettings()">Save settings</button>
  <div id="cfgSaved" class="toast"></div>
</div>

<script>
const NAMES=['Sunday','Monday','Tuesday','Wednesday','Thursday','Friday','Saturday'];
const SHORT=['Sun','Mon','Tue','Wed','Thu','Fri','Sat'];
const ORDER=[1,2,3,4,5,6,0];
let st=null, built=false, cfgBuilt=false, busy=false;
const $=id=>document.getElementById(id);
const pad=n=>String(n).padStart(2,'0');

function toast(id,msg,ok=true){
  const el=$(id);
  el.style.color=ok?'#8bc34a':'#ff8a65';
  el.textContent=msg;
  setTimeout(()=>{ if(el.textContent===msg) el.textContent=''; },2500);
}

function post(url,data){
  return fetch(url,{method:'POST',cache:'no-store',body:new URLSearchParams(data||{})})
    .then(r=>{refresh();return r;});
}

function buildDays(alarms){
  let h='';
  for(const d of ORDER){
    const a=alarms[d];
    h+=`<div class="row"><label><input type="checkbox" id="en${d}" ${a.en?'checked':''}>${NAMES[d]}</label>`+
       `<input type="time" id="t${d}" value="${pad(a.h)}:${pad(a.m)}"></div>`;
  }
  $('days').innerHTML=h;
  built=true;
}

function formatNext(n){
  if(!n||n.day<0) return 'Next alarm: None';
  let when=n.offset===0?'Today':(n.offset===1?'Tomorrow':SHORT[n.day]);
  const h=Math.floor(n.inMin/60), m=n.inMin%60;
  const rel=h>0?`${h}h ${m}m`:`${m}m`;
  return `Next alarm: ${when} ${pad(n.h)}:${pad(n.m)} (in ${rel})`;
}

function fmtSec(sec){ return Math.floor(sec/60)+':'+pad(sec%60); }

function refresh(){
  if(busy)return;
  busy=true;
  fetch('/api/status',{cache:'no-store'}).then(r=>r.json()).then(s=>{
    st=s;
    if(s.valid){
      const [date,time]=s.time.split(' ');
      $('clock').textContent=time;
      $('date').textContent=NAMES[s.wday]+'  '+date;
      $('nextAlarm').textContent=formatNext(s.next);
    }else{
      $('clock').textContent='--:--:--';
      $('date').textContent='';
      $('nextAlarm').textContent='';
    }
    let w=[];
    if(!s.valid) w.push('Clock is not set.');
    if(!s.rtc)   w.push('RTC (DS1302) not detected: time will be lost on power loss.');
    $('warn').textContent=w.join(' ');

    const b=$('panelBtn');
    b.textContent=s.panel?'Panel ON - tap to turn off':'Panel OFF - tap to turn on';
    b.className=s.panel?'on':'';

    const pn=$('panelNote');
    if(s.mode==='idle'&&s.panel&&s.remain>0){
      pn.hidden=false;
      pn.textContent='Auto-off in '+fmtSec(s.remain);
    }else{ pn.hidden=true; }

    if(s.mode==='idle'){ $('alarmBox').hidden=true; }
    else{
      $('alarmBox').hidden=false;
      $('snoozeBtn').textContent=`Snooze ${s.cfg.snoozeMin} min`;
      if(s.mode==='ringing'){
        $('alarmText').textContent=s.cfg.alarmMin>0
          ? 'Alarm ringing! Auto-off in '+fmtSec(s.remain)
          : 'Alarm ringing! (manual off)';
      }else{
        $('alarmText').textContent='Snoozed. Rings again in '+fmtSec(s.remain);
      }
    }

    if(!built) buildDays(s.alarms);
    if(!cfgBuilt&&s.cfg){
      $('alarmMin').value=s.cfg.alarmMin;
      $('snoozeMin').value=s.cfg.snoozeMin;
      $('manualOffMin').value=s.cfg.manualOffMin;
      cfgBuilt=true;
    }
  }).catch(()=>{ $('warn').textContent='Connection lost...'; })
    .finally(()=>{ busy=false; });
}

function togglePanel(){ post('/api/panel',{state:'toggle'}); }

function sendTime(y,mo,d,h,mi,s,msg){
  return post('/api/time',{y,mo,d,h,mi,s}).then(r=>{
    if(msg) toast('timeMsg',r.ok?msg:'Failed to set time!',r.ok);
  });
}

function syncTime(show){
  const n=new Date();
  return sendTime(n.getFullYear(),n.getMonth()+1,n.getDate(),
                  n.getHours(),n.getMinutes(),n.getSeconds(),
                  show?'Time synced \u2713':'');
}

function setManualTime(){
  const d=$('manDate').value, t=$('manTime').value;
  if(!d||!t){ toast('timeMsg','Select date and time',false); return; }
  const [y,mo,da]=d.split('-').map(Number);
  const [h,mi]=t.split(':').map(Number);
  sendTime(y,mo,da,h,mi,0,'Manual time set \u2713');
}

function saveAlarms(msg='Saved \u2713'){
  const data={};
  for(let d=0;d<7;d++){
    data['en'+d]=$('en'+d).checked?'1':'0';
    data['t'+d]=$('t'+d).value||'07:00';
  }
  post('/api/alarms',data).then(r=>{
    toast('saved',r.ok?msg:'Error!',r.ok);
  });
}

function copyMonToFri(){
  const t=$('t1').value||'07:00', en=$('en1').checked;
  for(let d=2;d<=5;d++){ $('t'+d).value=t; $('en'+d).checked=en; }
  saveAlarms('Copied Mon \u2192 Fri & saved \u2713');
}

function disableAll(){
  for(let d=0;d<7;d++) $('en'+d).checked=false;
  saveAlarms('All alarms disabled \u2713');
}

function saveSettings(){
  const a=$('alarmMin').value, s=$('snoozeMin').value, m=$('manualOffMin').value;
  post('/api/settings',{alarmMin:a,snoozeMin:s,manualOffMin:m}).then(r=>{
    toast('cfgSaved',r.ok?'Settings saved \u2713':'Error!',r.ok);
  });
}

(function init(){
  const n=new Date();
  $('manDate').value=n.getFullYear()+'-'+pad(n.getMonth()+1)+'-'+pad(n.getDate());
  $('manTime').value=pad(n.getHours())+':'+pad(n.getMinutes());
  syncTime(false);
  setInterval(refresh,1000);
})();
</script>
</body>
</html>
)rawliteral";
