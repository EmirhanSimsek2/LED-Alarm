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
  body{font-family:system-ui,-apple-system,sans-serif;background:#000;color:#fff;margin:0 auto;padding:16px;max-width:480px}
  h1{font-size:1.3em;margin:4px 0 14px}
  h2{font-size:1.1em;margin:0}
  .card{background:#1c1c1e;border-radius:16px;padding:16px;margin-bottom:14px}
  #clock{font-size:2.6em;text-align:center;font-variant-numeric:tabular-nums}
  #date,#nextAlarm{text-align:center;color:#98989f;font-size:.95em;margin-top:2px}
  #nextAlarm{color:#ff9f0a;margin-top:6px}
  .warn{color:#ff6961;text-align:center;font-size:.9em;margin-top:6px}
  .note{color:#98989f;text-align:center;font-size:.9em;margin-top:8px}
  button{font-size:1em;padding:12px;border:0;border-radius:12px;width:100%;margin-top:8px;background:#2c2c2e;color:#fff;cursor:pointer}
  button:active{opacity:.7}
  #panelBtn{font-size:1.25em;padding:18px;margin-top:0}
  #panelBtn.on{background:#f5c518;color:#111;font-weight:600}
  #alarmBox{background:#3a2a00;border-radius:12px;padding:12px;margin-top:10px;text-align:center}
  .row{display:flex;align-items:center;justify-content:space-between;padding:8px 0;border-bottom:1px solid #2c2c2e;gap:10px}
  .row:last-child{border-bottom:none}
  .row label{flex:1}
  input[type=time],input[type=date],input[type=number],input[type=text]{font-size:1em;padding:8px 10px;background:#2c2c2e;color:#fff;border:1px solid #3a3a3c;border-radius:10px}
  input[type=number]{width:84px;text-align:center}
  .primary{background:#0a84ff}
  .danger{background:#3a1c1c;color:#ff453a}
  .toast{text-align:center;color:#30d158;min-height:1.2em;margin-top:6px;font-size:.9em}
  details{margin-top:10px;border-top:1px solid #2c2c2e;padding-top:8px}
  summary{color:#98989f;font-size:.9em;cursor:pointer}
  .man-grid{display:flex;gap:8px;margin-top:8px}
  .man-grid input{flex:1}

  /* iOS-style Alarm List */
  .al-head{display:flex;align-items:center;justify-content:space-between;margin-bottom:10px}
  .pill-btn{width:auto;margin:0;padding:6px 14px;border-radius:999px;background:#2c2c2e;color:#ff9f0a;font-size:.92em;font-weight:600}
  .icon-btn{width:36px;height:36px;margin:0;padding:0;border-radius:50%;background:#2c2c2e;color:#ff9f0a;font-size:1.4em;line-height:36px;text-align:center}
  .al-item{display:flex;align-items:center;gap:12px;padding:10px 0;border-bottom:1px solid #2c2c2e}
  .al-item:last-child{border-bottom:none}
  .al-del{width:28px;height:28px;margin:0;padding:0;border-radius:50%;background:#ff453a;color:#fff;font-size:1.1em;font-weight:700;flex-shrink:0}
  .al-info{flex:1;cursor:pointer;min-width:0}
  .al-time{font-size:2.3em;font-weight:300;line-height:1.05;font-variant-numeric:tabular-nums;color:#fff}
  .al-item.off .al-time,.al-item.off .al-sub{color:#636366}
  .al-sub{font-size:.88em;color:#98989f;margin-top:2px;white-space:nowrap;overflow:hidden;text-overflow:ellipsis}
  .empty{color:#636366;text-align:center;padding:18px 0;font-size:.95em}

  /* iOS Toggle Switch */
  .sw{position:relative;display:inline-block;width:51px;height:31px;flex-shrink:0}
  .sw input{opacity:0;width:0;height:0}
  .sl{position:absolute;cursor:pointer;inset:0;background:#39393d;border-radius:31px;transition:.2s}
  .sl:before{content:"";position:absolute;height:27px;width:27px;left:2px;bottom:2px;background:#fff;border-radius:50%;transition:.2s}
  .sw input:checked+.sl{background:#30d158}
  .sw input:checked+.sl:before{transform:translateX(20px)}

  /* Add / Edit Modal */
  .modal-bg{position:fixed;inset:0;background:rgba(0,0,0,.72);display:flex;align-items:flex-end;justify-content:center;z-index:50}
  .modal-bg[hidden]{display:none}
  .modal{background:#1c1c1e;width:100%;max-width:480px;border-radius:20px 20px 0 0;padding:16px 16px 24px;max-height:92vh;overflow-y:auto}
  .m-top{display:flex;align-items:center;justify-content:space-between;margin-bottom:14px}
  .m-title{font-weight:600;font-size:1.05em}
  .m-x,.m-ok{width:38px;height:38px;margin:0;padding:0;border-radius:50%;font-size:1.15em;font-weight:700}
  .m-x{background:#2c2c2e;color:#fff}
  .m-ok{background:#ff9f0a;color:#000}
  .m-time-wrap{display:flex;justify-content:center;margin:8px 0 16px}
  #mTime{font-size:2.3em;padding:10px 18px;border-radius:14px;text-align:center;font-variant-numeric:tabular-nums}
  .m-group{background:#2c2c2e;border-radius:14px;padding:12px 14px;margin-bottom:12px}
  .m-row{display:flex;align-items:center;justify-content:space-between;padding:8px 0;border-bottom:1px solid #3a3a3c;gap:10px}
  .m-row:last-child{border-bottom:none;padding-bottom:0}
  .m-row:first-child{padding-top:0}
  .days-bar{display:flex;justify-content:space-between;gap:6px;margin-top:8px}
  .day-btn{width:38px;height:38px;margin:0;padding:0;border-radius:50%;background:#3a3a3c;color:#fff;font-size:.88em;font-weight:600}
  .day-btn.on{background:#ff9f0a;color:#000}
  .presets{display:flex;gap:6px;flex-wrap:wrap;margin-top:10px}
  .pre-btn{width:auto;margin:0;padding:5px 10px;border-radius:999px;background:#3a3a3c;color:#ddd;font-size:.8em}
  #mLabel{flex:1;text-align:right;background:transparent;border:0;color:#ff9f0a;font-size:1em;padding:4px 0}
  #mLabel:focus{outline:none}
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
  <div class="al-head">
    <button class="pill-btn" id="editBtn" onclick="toggleEdit()">Edit</button>
    <h2>Alarms</h2>
    <button class="icon-btn" onclick="openModal(-1)" aria-label="Add alarm">+</button>
  </div>
  <div id="alarmList"></div>
  <div id="saved" class="toast"></div>
</div>

<div class="card">
  <h2 style="margin-bottom:8px">Settings</h2>
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

<div id="modalBg" class="modal-bg" hidden onclick="if(event.target===this)closeModal()">
  <div class="modal">
    <div class="m-top">
      <button class="m-x" onclick="closeModal()">&#10005;</button>
      <div class="m-title" id="mTitle">Add Alarm</div>
      <button class="m-ok" onclick="saveModal()">&#10003;</button>
    </div>
    <div class="m-time-wrap">
      <input type="time" id="mTime" value="07:00">
    </div>
    <div class="m-group">
      <div>
        <div style="display:flex;justify-content:space-between;align-items:center">
          <span>Repeat</span>
          <span id="mRepeatText" style="color:#ff9f0a;font-size:.9em">Once</span>
        </div>
        <div class="days-bar" id="mDaysBar"></div>
        <div class="presets">
          <button class="pre-btn" onclick="setMask(0)">Once</button>
          <button class="pre-btn" onclick="setMask(62)">Weekdays</button>
          <button class="pre-btn" onclick="setMask(65)">Weekends</button>
          <button class="pre-btn" onclick="setMask(127)">Every day</button>
        </div>
      </div>
      <div class="m-row" style="margin-top:10px;padding-top:10px;border-top:1px solid #3a3a3c">
        <span>Label</span>
        <input type="text" id="mLabel" maxlength="23" placeholder="Alarm">
      </div>
      <div class="m-row">
        <span>Snooze</span>
        <label class="sw">
          <input type="checkbox" id="mSnooze" checked>
          <span class="sl"></span>
        </label>
      </div>
    </div>
    <button id="mDelBtn" class="danger" onclick="deleteModalAlarm()" hidden>Delete Alarm</button>
  </div>
</div>

<script>
const NAMES=['Sunday','Monday','Tuesday','Wednesday','Thursday','Friday','Saturday'];
const SHORT=['Sun','Mon','Tue','Wed','Thu','Fri','Sat'];
const DAY_BITS=[1,2,3,4,5,6,0]; // Mon..Sun
const DAY_LBL=['M','T','W','T','F','S','S'];

let st=null, cfgBuilt=false, busy=false, editMode=false;
let editId=-1, modalMask=0, lastAlarmKey='';
const $=id=>document.getElementById(id);
const pad=n=>String(n).padStart(2,'0');
const esc=s=>String(s||'').replace(/[&<>"']/g,c=>({'&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;',"'":'&#39;'}[c]));

function toast(id,msg,ok=true){
  const el=$(id);
  el.style.color=ok?'#30d158':'#ff6961';
  el.textContent=msg;
  setTimeout(()=>{ if(el.textContent===msg) el.textContent=''; },2500);
}

function post(url,data){
  return fetch(url,{method:'POST',cache:'no-store',body:new URLSearchParams(data||{})})
    .then(r=>{refresh();return r;});
}

function maskText(mask){
  if(mask===0) return 'Once';
  if(mask===127) return 'Every day';
  if(mask===62) return 'Weekdays';
  if(mask===65) return 'Weekends';
  const out=[];
  for(const d of DAY_BITS) if(mask&(1<<d)) out.push(SHORT[d]);
  return out.join(', ');
}

function renderAlarms(list){
  const key=editMode+'|'+JSON.stringify(list);
  if(key===lastAlarmKey) return;
  lastAlarmKey=key;

  if(!list||list.length===0){
    $('alarmList').innerHTML='<div class="empty">No alarms yet. Tap + to add one.</div>';
    return;
  }
  const sorted=[...list].sort((a,b)=>(a.h*60+a.m)-(b.h*60+b.m));
  let h='';
  for(const a of sorted){
    const sub=esc(a.lbl||'Alarm')+' &bull; '+maskText(a.days);
    h+=`<div class="al-item ${a.en?'':'off'}">`+
       (editMode?`<button class="al-del" onclick="delAlarm(${a.id})">&#8722;</button>`:'')+
       `<div class="al-info" onclick="openModal(${a.id})">`+
         `<div class="al-time">${pad(a.h)}:${pad(a.m)}</div>`+
         `<div class="al-sub">${sub}</div>`+
       `</div>`+
       `<label class="sw"><input type="checkbox" ${a.en?'checked':''} onchange="toggleAlarm(${a.id},this.checked)"><span class="sl"></span></label>`+
       `</div>`;
  }
  $('alarmList').innerHTML=h;
}

function formatNext(n){
  if(!n||n.day<0) return 'Next alarm: None';
  let when=n.offset===0?'Today':(n.offset===1?'Tomorrow':SHORT[n.day]);
  const h=Math.floor(n.inMin/60), m=n.inMin%60;
  const rel=h>0?`${h}h ${m}m`:`${m}m`;
  const lbl=n.lbl&&n.lbl!=='Alarm'?` (${n.lbl})`:'';
  return `Next alarm: ${when} ${pad(n.h)}:${pad(n.m)}${lbl} \u2022 in ${rel}`;
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
      const sb=$('snoozeBtn');
      sb.hidden=!s.snoozable;
      sb.textContent=`Snooze ${s.cfg.snoozeMin} min`;
      const lbl=s.activeLbl?`${s.activeLbl} \u2014 `:'';
      if(s.mode==='ringing'){
        $('alarmText').textContent=s.cfg.alarmMin>0
          ? lbl+'Alarm ringing! Auto-off in '+fmtSec(s.remain)
          : lbl+'Alarm ringing! (manual off)';
      }else{
        $('alarmText').textContent=lbl+'Snoozed. Rings again in '+fmtSec(s.remain);
      }
    }

    renderAlarms(s.alarms);
    if(!cfgBuilt&&s.cfg){
      $('alarmMin').value=s.cfg.alarmMin;
      $('snoozeMin').value=s.cfg.snoozeMin;
      $('manualOffMin').value=s.cfg.manualOffMin;
      cfgBuilt=true;
    }
  }).catch(()=>{ $('warn').textContent='Connection lost...'; })
    .finally(()=>{ busy=false; });
}

function toggleEdit(){
  editMode=!editMode;
  $('editBtn').textContent=editMode?'Done':'Edit';
  if(st) renderAlarms(st.alarms);
}

function toggleAlarm(id,en){
  post('/api/alarm/toggle',{id,en:en?'1':'0'}).then(r=>{
    if(!r.ok) toast('saved','Failed to update alarm',false);
  });
}

function delAlarm(id){
  post('/api/alarm/delete',{id}).then(r=>{
    toast('saved',r.ok?'Alarm deleted':'Error!',r.ok);
  });
}

function renderDaysBar(){
  let h='';
  DAY_BITS.forEach((bit,i)=>{
    const on=(modalMask&(1<<bit))!==0;
    h+=`<button type="button" class="day-btn ${on?'on':''}" onclick="toggleDay(${bit})">${DAY_LBL[i]}</button>`;
  });
  $('mDaysBar').innerHTML=h;
  $('mRepeatText').textContent=maskText(modalMask);
}

function toggleDay(bit){ modalMask^=(1<<bit); renderDaysBar(); }
function setMask(m){ modalMask=m; renderDaysBar(); }

function openModal(id){
  editId=id;
  if(id<0){
    $('mTitle').textContent='Add Alarm';
    $('mTime').value='07:00';
    $('mLabel').value='Alarm';
    $('mSnooze').checked=true;
    $('mDelBtn').hidden=true;
    modalMask=0;
  }else{
    const a=st&&st.alarms?st.alarms.find(x=>x.id===id):null;
    if(!a) return;
    $('mTitle').textContent='Edit Alarm';
    $('mTime').value=pad(a.h)+':'+pad(a.m);
    $('mLabel').value=a.lbl||'Alarm';
    $('mSnooze').checked=!!a.snz;
    $('mDelBtn').hidden=false;
    modalMask=a.days||0;
  }
  renderDaysBar();
  $('modalBg').hidden=false;
}

function closeModal(){ $('modalBg').hidden=true; }

function saveModal(){
  const tm=$('mTime').value||'07:00';
  const [h,m]=tm.split(':').map(Number);
  const lbl=$('mLabel').value.trim()||'Alarm';
  const snz=$('mSnooze').checked?'1':'0';
  post('/api/alarm/save',{id:editId,en:'1',h,m,days:modalMask,snz,lbl}).then(r=>{
    if(r.ok){ closeModal(); toast('saved','Alarm saved \u2713'); }
    else r.text().then(t=>toast('saved',t||'Error!',false));
  });
}

function deleteModalAlarm(){
  if(editId<0) return;
  closeModal();
  delAlarm(editId);
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
