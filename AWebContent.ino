/*
  File: AWebContent.ino
  Purpose:
  This file stores the full HTML page for the built in web dashboard.
*/

namespace webcontent {

const char INDEX_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>Inclina</title>
  <style>
    :root {
      color-scheme: dark;
      --bg1: #08131d;
      --bg2: #12324a;
      --card: rgba(8, 16, 26, 0.82);
      --line: rgba(255, 255, 255, 0.12);
      --text: #f4fbff;
      --muted: #9fb6c8;
      --warn: #ff8f70;
      --ok: #7ef7a2;
    }
    * { box-sizing: border-box; }
    body {
      margin: 0;
      min-height: 100vh;
      font-family: "Segoe UI", system-ui, sans-serif;
      color: var(--text);
      background:
        radial-gradient(circle at top left, rgba(110, 231, 255, 0.18), transparent 30%),
        linear-gradient(160deg, var(--bg2), var(--bg1) 65%);
      display: grid;
      place-items: center;
      padding: 18px;
    }
    .panel {
      width: min(720px, 100%);
      background: var(--card);
      border: 1px solid var(--line);
      border-radius: 22px;
      padding: 18px;
      backdrop-filter: blur(10px);
      box-shadow: 0 22px 70px rgba(0, 0, 0, 0.35);
    }
    .top {
      display: flex;
      justify-content: space-between;
      align-items: center;
      gap: 12px;
      margin-bottom: 14px;
    }
    .title { font-size: 1.35rem; font-weight: 700; }
    .meta { color: var(--muted); font-size: 0.95rem; }
    .grid {
      display: grid;
      grid-template-columns: repeat(2, minmax(0, 1fr));
      gap: 12px;
    }
    .card {
      border: 1px solid var(--line);
      border-radius: 18px;
      padding: 14px;
      background: rgba(255, 255, 255, 0.03);
    }
    .label {
      color: var(--muted);
      font-size: 0.82rem;
      text-transform: uppercase;
      letter-spacing: 0.08em;
      margin-bottom: 8px;
    }
    .value {
      font-size: clamp(1.7rem, 6vw, 3rem);
      font-weight: 700;
      line-height: 1;
    }
    .unit { font-size: 1rem; color: var(--muted); margin-left: 6px; }
    .statusRow {
      display: flex;
      flex-wrap: wrap;
      gap: 10px;
      margin-top: 14px;
      margin-bottom: 14px;
    }
    .viewRow {
      display: grid;
      grid-template-columns: repeat(3, minmax(0, 1fr));
      gap: 8px;
      margin-bottom: 12px;
    }
    .chip {
      border: 1px solid var(--line);
      border-radius: 999px;
      padding: 8px 12px;
      color: var(--muted);
      background: rgba(255, 255, 255, 0.04);
    }
    .chip.ok { color: var(--ok); }
    .chip.bad { color: var(--warn); }
    button {
      width: 100%;
      border: 0;
      font-weight: 700;
      cursor: pointer;
    }
    button:active { transform: scale(0.99); }
    .viewBtn {
      border-radius: 12px;
      padding: 10px;
      background: rgba(255, 255, 255, 0.06);
      color: var(--text);
      border: 1px solid var(--line);
      font-size: 0.92rem;
    }
    .viewBtn.active {
      background: linear-gradient(135deg, #6ee7ff, #4cc9f0);
      color: #062235;
      border-color: transparent;
    }
    .actionBtn {
      border-radius: 16px;
      padding: 14px;
      background: linear-gradient(135deg, #6ee7ff, #4cc9f0);
      color: #062235;
      font-size: 1rem;
    }
    .actionRow {
      display: grid;
      grid-template-columns: repeat(2, minmax(0, 1fr));
      gap: 10px;
      margin-top: 12px;
    }
    .restartBtn {
      background: linear-gradient(135deg, #ffb080, #ff8f70);
      color: #2d1207;
    }
    .foot {
      margin-top: 12px;
      color: var(--muted);
      font-size: 0.92rem;
      text-align: center;
    }
    @media (max-width: 560px) {
      .grid { grid-template-columns: 1fr; }
    }
  </style>
</head>
<body>
  <main class="panel">
    <div class="top">
      <div>
        <div class="title">Inclina Monitor</div>
        <div class="meta" id="mode">Connecting...</div>
      </div>
      <div class="meta" id="updated">-</div>
    </div>

    <section class="grid">
      <div class="card">
        <div class="label">Height</div>
        <div class="value"><span id="height">0.0</span><span class="unit">m</span></div>
      </div>
      <div class="card">
        <div class="label">Temperature</div>
        <div class="value"><span id="temp">0.0</span><span class="unit">C</span></div>
      </div>
      <div class="card">
        <div class="label">Roll</div>
        <div class="value"><span id="roll">0.0</span><span class="unit">deg</span></div>
      </div>
      <div class="card">
        <div class="label">Pitch</div>
        <div class="value"><span id="pitch">0.0</span><span class="unit">deg</span></div>
      </div>
    </section>

    <div class="statusRow">
      <div class="chip" id="baro">BMP388</div>
      <div class="chip" id="imu">MPU6050</div>
      <div class="chip" id="move">Still</div>
    </div>

    <section class="grid">
      <div class="card">
        <div class="label">Gyro Level</div>
        <div class="value"><span id="gyro">0.00</span><span class="unit">dps</span></div>
      </div>
      <div class="card">
        <div class="label">Accel Level</div>
        <div class="value"><span id="accel">0.00</span><span class="unit">g</span></div>
      </div>
    </section>

    <div class="viewRow">
      <button class="viewBtn" id="viewNormal">Normal</button>
      <button class="viewBtn" id="viewPrecise">Precise</button>
      <button class="viewBtn" id="viewStatus">Status</button>
    </div>

    <div class="actionRow">
      <button class="actionBtn" id="zeroBtn">Set Zero</button>
      <button class="actionBtn restartBtn" id="restartBtn">Restart ESP</button>
    </div>
    <div class="foot" id="errorText">Ready</div>
  </main>

  <script>
    function setActiveView(view) {
      document.getElementById('viewNormal').classList.toggle('active', view === 'Normal');
      document.getElementById('viewPrecise').classList.toggle('active', view === 'Precise');
      document.getElementById('viewStatus').classList.toggle('active', view === 'Status');
    }

    async function setView(view) {
      try {
        await fetch('/view?value=' + encodeURIComponent(view), { method: 'POST' });
        updateData();
      } catch (err) {
        document.getElementById('errorText').textContent = 'Could not change view';
      }
    }

    async function updateData() {
      try {
        const res = await fetch('/data', { cache: 'no-store' });
        const data = await res.json();

        document.getElementById('height').textContent = Number(data.height).toFixed(2);
        document.getElementById('temp').textContent = Number(data.temp).toFixed(1);
        document.getElementById('roll').textContent = Number(data.roll).toFixed(1);
        document.getElementById('pitch').textContent = Number(data.pitch).toFixed(1);
        document.getElementById('gyro').textContent = Number(data.gyro).toFixed(2);
        document.getElementById('accel').textContent = Number(data.accel).toFixed(3);
        document.getElementById('mode').textContent = data.mode + ' - ' + data.view + ' - ' + data.ip;
        document.getElementById('updated').textContent = new Date().toLocaleTimeString();
        setActiveView(data.view);

        const baro = document.getElementById('baro');
        baro.className = 'chip ' + (data.baro ? 'ok' : 'bad');
        baro.textContent = data.baro ? 'BMP388 OK' : 'BMP388 Error';

        const imu = document.getElementById('imu');
        imu.className = 'chip ' + (data.imu ? 'ok' : 'bad');
        imu.textContent = data.imu ? 'MPU6050 OK' : 'MPU6050 Error';

        const move = document.getElementById('move');
        move.className = 'chip ' + (data.still ? 'ok' : 'bad');
        move.textContent = data.still ? 'Still' : 'Moving';

        document.getElementById('errorText').textContent = data.error;
      } catch (err) {
        document.getElementById('errorText').textContent = 'No connection to ESP32';
      }
    }

    document.getElementById('zeroBtn').addEventListener('click', async () => {
      try {
        await fetch('/zero', { method: 'POST' });
        document.getElementById('errorText').textContent = 'Zero set';
      } catch (err) {
        document.getElementById('errorText').textContent = 'Could not set zero';
      }
    });

    document.getElementById('restartBtn').addEventListener('click', async () => {
      try {
        document.getElementById('errorText').textContent = 'Restarting ESP...';
        await fetch('/restart', { method: 'POST' });
        setTimeout(() => window.location.reload(), 4000);
      } catch (err) {
        document.getElementById('errorText').textContent = 'Could not restart ESP';
      }
    });

    document.getElementById('viewNormal').addEventListener('click', () => setView('normal'));
    document.getElementById('viewPrecise').addEventListener('click', () => setView('precise'));
    document.getElementById('viewStatus').addEventListener('click', () => setView('status'));

    updateData();
    setInterval(updateData, 500);
  </script>
</body>
</html>
)rawliteral";

}  // namespace webcontent
