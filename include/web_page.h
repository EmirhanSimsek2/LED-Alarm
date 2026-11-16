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
  body{font-family:system-ui,-apple-system,sans-serif;background:#111;color:#eee;margin:0 auto;padding:16px;max-width:480px}
  h1{font-size:1.3em;margin:4px 0 14px}
  h2{font-size:1.1em;margin:0 0 8px}
  .card{background:#1e1e1e;border-radius:14px;padding:16px;margin-bottom:14px}
  #clock{font-size:2.6em;text-align:center;font-variant-numeric:tabular-nums}
  #date{text-align:center;color:#aaa}
  .warn{color:#ff8a65;text-align:center;font-size:.9em;margin-top:6px}
  button{font-size:1em;padding:13px;border:0;border-radius:10px;width:100%;margin-top:8px;background:#333;color:#eee}
  button:active{opacity:.7}
  #panelBtn{font-size:1.3em;padding:20px}
  #panelBtn.on{background:#f5c518;color:#111}
  #alarmBox{background:#3a2a00;border-radius:10px;padding:10px;margin-top:10px;text-align:center}
  .row{display:flex;align-items:center;justify-content:space-between;padding:7px 0;border-bottom:1px solid #2a2a2a}
  .row label{display:flex;align-items:center;gap:10px}
  input[type=checkbox]{width:22px;height:22px}
  input[type=time]{font-size:1em;padding:6px;background:#2a2a2a;color:#eee;border:1px solid #444;border-radius:6px}
  #saved{text-align:center;color:#8bc34a;height:1.2em;margin-top:6px}
  .primary{background:#1976d2}
</style>
</head>
<body>
<h1>&#128161; LED Alarm</h1>

<div class="card">
  <div id="clock">--:--:--</div>
  <div id="date"></div>
  <div id="warn" class="warn"></div>
  <button onclick="syncTime()">Sync time from phone</button>
</div>

<div class="card">
  <button id="panelBtn" onclick="togglePanel()">Panel</button>
  <div id="alarmBox" hidden>
    <div id="alarmText"></div>
    <button onclick="post('/api/snooze')">Snooze 5 min</button>
    <button onclick="post('/api/dismiss')">Stop alarm</button>
  </div>
</div>

<div class="card">
  <h2>Alarms</h2>
  <div id="days"></div>
  <button class="primary" onclick="saveAlarms()">Save alarms</button>
  <div id="saved"></div>
</div>

<script>
const NAMES=['Sunday','Monday','Tuesday','Wednesday','Thursday','Friday','Saturday'];
const ORDER=[1,2,3,4,5,6,0];   // show Monday first
let st=null, built=false;
const $=id=>document.getElementById(id);
const pad=n=>String(n).padStart(2,'0');

function post(url,data){
  return fetch(url,{method:'POST',body:new URLSearchParams(data||{})})
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

function refresh(){
  fetch('/api/status').then(r=>r.json()).then(s=>{
    st=s;
    if(s.valid){
      const [date,time]=s.time.split(' ');
      $('clock').textContent=time;
      $('date').textContent=NAMES[s.wday]+'  '+date;
    }else{
      $('clock').textContent='--:--:--';
      $('date').textContent='';
    }
    let w=[];
    if(!s.valid) w.push('Clock is not set.');
    if(!s.rtc)   w.push('RTC (DS1302) not detected: time will be lost on power loss.');
    $('warn').textContent=w.join(' ');

    const b=$('panelBtn');
    b.textContent=s.panel?'Panel ON - tap to turn off':'Panel OFF - tap to turn on';
    b.className=s.panel?'on':'';

    if(s.mode==='idle'){ $('alarmBox').hidden=true; }
    else{
      $('alarmBox').hidden=false;
      const m=Math.floor(s.remain/60), sec=s.remain%60;
      $('alarmText').textContent=(s.mode==='ringing'?'Alarm ringing! Auto-off in ':'Snoozed. Rings again in ')+m+':'+pad(sec);
    }
    if(!built) buildDays(s.alarms);
  }).catch(()=>{ $('warn').textContent='Connection lost...'; });
}

function togglePanel(){ post('/api/panel',{state:st&&st.panel?'off':'on'}); }

function syncTime(){
  const n=new Date();
  return post('/api/time',{y:n.getFullYear(),mo:n.getMonth()+1,d:n.getDate(),
                           h:n.getHours(),mi:n.getMinutes(),s:n.getSeconds()});
}

function saveAlarms(){
  const data={};
  for(let d=0;d<7;d++){
    data['en'+d]=$('en'+d).checked?'1':'0';
    data['t'+d]=$('t'+d).value||'07:00';
  }
  post('/api/alarms',data).then(r=>{
    $('saved').textContent=r.ok?'Saved \u2713':'Error!';
    setTimeout(()=>$('saved').textContent='',2500);
  });
}

syncTime();                 // phone's clock -> ESP32 every time the page opens
setInterval(refresh,1000);
</script>
</body>
</html>
)rawliteral";
