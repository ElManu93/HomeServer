// html_page.h - KOMPLETTES HTML mit SD-Chart Kachel
#ifndef HTML_PAGE_H
#define HTML_PAGE_H

const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="de">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Smart Home Dashboard</title>
    
    <script src="https://cdn.jsdelivr.net/npm/chart.js"></script>
    
    <script>
        let sdChart = null;
        let chartData = [];
        let currentView = 'alle';
    
        function zeitAktualisieren() {
            const jetzt = new Date();
            const optionen = { 
                weekday: 'long', 
                year: 'numeric', 
                month: 'long', 
                day: 'numeric'
            };
            document.getElementById('zeit').innerHTML = jetzt.toLocaleDateString('de-DE', optionen);
        }
    
        function updateSensorData() {
            fetch('/sensors')
            .then(response => response.json())
            .then(data => {
                // ESP32 Daten (bei "alle" oder "esp32")
                if (currentView === 'alle' || currentView === 'esp32') {
                    document.getElementById('temperature').textContent = data.temperature.toFixed(1);
                    document.getElementById('humidity').textContent = data.humidity.toFixed(1);
                    document.getElementById('pressure').textContent = data.pressure.toFixed(1);
                    document.getElementById('esp32-title').style.display = 'block';
                    document.querySelectorAll('.esp32-card').forEach(card => card.style.display =   'block');
                } else {
                    document.getElementById('esp32-title').style.display = 'none';
                    document.querySelectorAll('.esp32-card').forEach(card => card.style.display =   'none');
                }
    
                // Pico W Daten (bei "alle" oder "pico") - IMMER ANZEIGEN
                if (currentView === 'alle' || currentView === 'pico') {
                    document.getElementById('pico_temperature').textContent = (data.pico_temperature ||     0).toFixed(1);
                    document.getElementById('pico_humidity').textContent = (data.pico_humidity || 0).   toFixed(1);
                    document.getElementById('pico_pressure').textContent = (data.pico_pressure || 0).   toFixed(1);
                    document.getElementById('pico-title').style.display = 'block';
                    document.querySelectorAll('.pico-card').forEach(card => card.style.display =    'block');
                } else {
                    document.getElementById('pico-title').style.display = 'none';
                    document.querySelectorAll('.pico-card').forEach(card => card.style.display =    'none');
                }
    
                document.getElementById('wetter').textContent = data.weather;
                
                const now = new Date();
                document.getElementById('last-update').textContent = now.toLocaleTimeString('de-DE');
                
                // Status nur nach Auswahl
                if (currentView === 'alle') {
                    document.querySelector('.status-indicator').style.backgroundColor = '#4CAF50';
                    document.getElementById('status-text').textContent = 'Alle Sensoren';
                } else if (currentView === 'esp32') {
                    document.querySelector('.status-indicator').style.backgroundColor = '#2196F3';
                    document.getElementById('status-text').textContent = 'ESP32 Indoor';
                } else if (currentView === 'pico') {
                    document.querySelector('.status-indicator').style.backgroundColor = '#ff9800';
                    document.getElementById('status-text').textContent = 'Pico Outdoor';
                }
            })
            .catch(error => {
                console.error('Fehler:', error);
                document.querySelectorAll('.esp32-title, .pico-title, .esp32-card, .pico-card').forEach (el => {
                    el.style.display = 'none';
                });
                document.querySelector('.status-indicator').style.backgroundColor = '#f44336';
                document.getElementById('status-text').textContent = 'Verbindungsfehler';
            });
        }
    
        function updateSDChart() {
            fetch('/sd-data')
            .then(response => response.json())
            .then(data => {
                if (data.status === 'ok' && data.data) {
                    chartData = data.data.trim().split('\n').map(line => {
                        let parts = line.split(';');
                        if (parts.length >= 4) {
                            return {
                                ts: Date.parse(parts[0]) || Date.now(),
                                temp: parseFloat(parts[1]) || 0,
                                hum: parseFloat(parts[2]) || 0,
                                press: parseFloat(parts[3]) || 0
                            };
                        }
                        return null;
                    }).filter(row => row && !isNaN(row.temp));
    
                    if (chartData.length > 0) {
                        document.getElementById('sdStats').innerHTML = 
                            `⌀ ${chartData.length} Werte | ` +
                            `🌡️ ${Math.min(...chartData.map(d=>d.temp)).toFixed(1)}° - ` +
                            `${Math.max(...chartData.map(d=>d.temp)).toFixed(1)}°C | ` +
                            `💧 ${Math.min(...chartData.map(d=>d.hum)).toFixed(0)} - ` +
                            `${Math.max(...chartData.map(d=>d.hum)).toFixed(0)}%`;
    
                        updateChart();
                    }
                } else {
                    document.getElementById('sdStats').innerHTML = 'Keine SD-Daten';
                }
            })
            .catch(err => {
                document.getElementById('sdStats').innerHTML = 'SD nicht verfügbar';
            });
        }
    
        function updateChart() {
            const ctx = document.getElementById('sdChart').getContext('2d');
            
            if (sdChart) sdChart.destroy();
            
            if (chartData.length === 0) return;
            
            sdChart = new Chart(ctx, {
                type: 'line',
                data: {
                    labels: chartData.map(d => {
                        let date = new Date(d.ts);
                        return date.toLocaleTimeString('de-DE', {hour: '2-digit', minute:'2-digit'});
                    }),
                    datasets: [{
                        label: 'Temperatur (°C)',
                        data: chartData.map(d => d.temp),
                        borderColor: '#ff6384',
                        backgroundColor: 'rgba(255,99,132,0.1)',
                        tension: 0.4,
                        fill: true,
                        pointRadius: 0,
                        borderWidth: 3
                    }]
                },
                options: {
                    responsive: true,
                    maintainAspectRatio: false,
                    plugins: { 
                        legend: { display: false },
                        tooltip: {
                            backgroundColor: 'rgba(0,0,0,0.8)',
                            titleColor: '#fff',
                            bodyColor: '#fff'
                        }
                    },
                    scales: {
                        x: { 
                            display: true,
                            ticks: { color: '#ccc', maxTicksLimit: 10 },
                            grid: { color: 'rgba(255,255,255,0.1)' }
                        },
                        y: { 
                            ticks: { color: '#ccc' },
                            grid: { color: 'rgba(255,255,255,0.1)' }
                        }
                    }
                }
            });
        }
    
        function changeSensorView(view) {
            currentView = view;
            document.getElementById('raumAuswahl').value = view;
            
            // Status sofort aktualisieren
            if (view === 'alle') {
                document.querySelector('.status-indicator').style.backgroundColor = '#4CAF50';
                document.getElementById('status-text').textContent = 'Alle Sensoren';
            } else if (view === 'esp32') {
                document.querySelector('.status-indicator').style.backgroundColor = '#2196F3';
                document.getElementById('status-text').textContent = 'ESP32 Indoor';
            } else if (view === 'pico') {
                document.querySelector('.status-indicator').style.backgroundColor = '#ff9800';
                document.getElementById('status-text').textContent = 'Pico Outdoor';
            }
            
            updateSensorData();
        }
    
        window.onload = function() {
            zeitAktualisieren();
            updateSensorData();
            updateSDChart();
            
            setInterval(zeitAktualisieren, 60000);      // 1 Minute
            setInterval(updateSensorData, 5000);        // 5 Sekunden
            setInterval(updateSDChart, 300000);         // 5 Minuten
        }
    </script>

    <style>
        @import url('https://fonts.googleapis.com/css2?family=Inter:wght@300;400;500;600;700&   display=swap');

        * {
            margin: 0;
            padding: 0;
            box-sizing: border-box;
        }

        body {
            background: linear-gradient(135deg, #0f0c29 0%, #302b63 50%, #24243e 100%);
            color: #ffffff;
            font-family: 'Inter', sans-serif;
            padding: 40px 20px;
            min-height: 100vh;
            position: relative;
            overflow-x: hidden;
        }

        body::before,
        body::after {
            content: '';
            position: fixed;
            border-radius: 50%;
            filter: blur(120px);
            opacity: 0.15;
            animation: float 20s infinite ease-in-out;
            z-index: 0;
        }

        body::before {
            width: 500px;
            height: 500px;
            background: #667eea;
            top: -200px;
            left: -200px;
            animation-delay: 0s;
        }

        body::after {
            width: 400px;
            height: 400px;
            background: #764ba2;
            bottom: -150px;
            right: -150px;
            animation-delay: 10s;
        }

        @keyframes float {
            0%, 100% { transform: translate(0, 0) scale(1); }
            33% { transform: translate(100px, -100px) scale(1.1); }
            66% { transform: translate(-50px, 100px) scale(0.9); }
        }

        .content-wrapper {
            max-width: 1200px;
            margin: 0 auto;
            position: relative;
            z-index: 1;
        }

        .welcome-card {
            background: rgba(255, 255, 255, 0.05);
            backdrop-filter: blur(20px);
            -webkit-backdrop-filter: blur(20px);
            border: 1px solid rgba(255, 255, 255, 0.1);
            border-radius: 30px;
            padding: 50px;
            box-shadow: 0 8px 32px rgba(0, 0, 0, 0.3);
            margin-bottom: 30px;
            transition: all 0.3s ease;
            animation: slideUp 0.6s ease-out;
        }

        @keyframes slideUp {
            from {
                opacity: 0;
                transform: translateY(30px);
            }
            to {
                opacity: 1;
                transform: translateY(0);
            }
        }

        .welcome-card:hover {
            transform: translateY(-5px);
            box-shadow: 0 12px 40px rgba(0, 0, 0, 0.4);
            border-color: rgba(255, 255, 255, 0.15);
        }

        h1 {
            font-size: 3em;
            font-weight: 700;
            margin-bottom: 15px;
            background: linear-gradient(135deg, #ffffff 0%, #a8b2ff 100%);
            -webkit-background-clip: text;
            -webkit-text-fill-color: transparent;
            background-clip: text;
        }

        .subtitle {
            font-size: 1.1em;
            font-weight: 300;
            opacity: 0.8;
            margin-bottom: 20px;
            line-height: 1.6;
            color: #d0d0e0;
        }

        .time-display {
            font-size: 0.95em;
            opacity: 0.7;
            margin-bottom: 25px;
            font-weight: 400;
            color: #b0b0c0;
        }

        .weather-badge {
            display: inline-flex;
            align-items: center;
            gap: 10px;
            background: rgba(255, 255, 255, 0.08);
            backdrop-filter: blur(10px);
            padding: 12px 24px;
            border-radius: 50px;
            font-size: 1.2em;
            font-weight: 500;
            border: 1px solid rgba(255, 255, 255, 0.1);
        }

        .weather-icon {
            font-size: 1.5em;
        }

        .dashboard-header {
            display: flex;
            justify-content: space-between;
            align-items: center;
            margin-bottom: 15px;
            animation: slideUp 0.7s ease-out;
        }

        h2 {
            font-size: 1.5em;
            font-weight: 600;
            opacity: 0.9;
            color: #e0e0f0;
        }

        .status {
            display: flex;
            align-items: center;
            gap: 8px;
            font-size: 0.95em;
            color: #b0b0c0;
            margin-bottom: 20px;
        }

        .status-indicator {
            width: 10px;
            height: 10px;
            border-radius: 50%;
            background-color: #4CAF50;
            animation: pulse 2s infinite;
            box-shadow: 0 0 10px rgba(76, 175, 80, 0.5);
        }

        @keyframes pulse {
            0%, 100% { opacity: 1; }
            50% { opacity: 0.5; }
        }

        .dropdown {
            padding: 12px 20px;
            background: rgba(255, 255, 255, 0.08);
            backdrop-filter: blur(10px);
            border: 1px solid rgba(255, 255, 255, 0.1);
            border-radius: 15px;
            color: white;
            font-size: 1em;
            font-weight: 500;
            cursor: pointer;
            transition: all 0.3s ease;
            font-family: 'Inter', sans-serif;
        }

        .dropdown:hover {
            background: rgba(255, 255, 255, 0.12);
            transform: translateY(-2px);
            box-shadow: 0 5px 15px rgba(0, 0, 0, 0.3);
            border-color: rgba(255, 255, 255, 0.2);
        }

        .dropdown:focus {
            outline: none;
            border-color: rgba(102, 126, 234, 0.6);
        }

        .dropdown option {
            background: #1a1a2e;
            color: white;
        }

        /* 🔥 GRID SYSTEM - ALLE Karten direkt im Grid */
        .sensor-grid {
            display: grid;
            grid-template-columns: repeat(auto-fit, minmax(220px, 1fr));
            grid-template-rows: auto auto auto auto auto auto auto auto auto;
            gap: 25px;
            margin-bottom: 30px;
            animation: slideUp 0.8s ease-out;
        }

        /* 🔥 GRUPPEN TITEL */
        .esp32-title { 
            grid-column: 1 / -1;
            font-size: 1.4em;
            font-weight: 600;
            margin-bottom: 15px;
            padding: 12px 0;
            border-bottom: 2px solid rgba(33, 150, 243, 0.5);
            color: #2196F3;
            opacity: 0.95;
            text-align: center;
            grid-row: 1;
        }

        .pico-title { 
            grid-column: 1 / -1;
            font-size: 1.4em;
            font-weight: 600;
            margin-bottom: 15px;
            padding: 12px 0;
            border-bottom: 2px solid rgba(255, 152, 0, 0.5);
            color: #ff9800;
            opacity: 0.95;
            text-align: center;
            grid-row: 5;
        }

        /* 🔥 ESP32 Karten */
        .esp32-card:nth-of-type(1) { 
            grid-row: 2;
            background: linear-gradient(135deg, rgba(146, 114, 245, 0.25) 0%, rgba(146, 114, 245, 0.    08) 100%);
        }
        .esp32-card:nth-of-type(2) { 
            grid-row: 3;
            background: linear-gradient(135deg, rgba(17, 179, 248, 0.25) 0%, rgba(17, 179, 248, 0.08)   100%);
        }
        .esp32-card:nth-of-type(3) { 
            grid-row: 4;
            background: linear-gradient(135deg, rgba(16, 185, 129, 0.25) 0%, rgba(16, 185, 129, 0.08)   100%);
        }

        /* 🔥 Pico Karten */
        .pico-card:nth-of-type(1) { 
            grid-row: 6;
            background: linear-gradient(135deg, rgba(255,152,0,0.25) 0%, rgba(255,152,0,0.08) 100%);
        }
        .pico-card:nth-of-type(2) { 
            grid-row: 7;
            background: linear-gradient(135deg, rgba(255,193,7,0.25) 0%, rgba(255,193,7,0.08) 100%);
        }
        .pico-card:nth-of-type(3) { 
            grid-row: 8;
            background: linear-gradient(135deg, rgba(255,235,59,0.25) 0%, rgba(255,235,59,0.08) 100%);
        }

        /* 🔥 Chart immer ganz unten */
        .sensor-card.full-width {
            background: linear-gradient(135deg, rgba(255,193,7,0.25) 0%, rgba(255,193,7,0.08) 100%);
            grid-column: 1 / -1 !important;
            grid-row: 9 !important;
            order: 999;
        }

        .sensor-card.full-width canvas {
            max-height: 200px;
            border-radius: 15px;
            margin: 15px 0;
        }

        /* 🔥 SENOR KARTEN */
        .sensor-card {
            background: rgba(255, 255, 255, 0.08);
            backdrop-filter: blur(20px);
            -webkit-backdrop-filter: blur(20px);
            border: 1px solid rgba(255, 255, 255, 0.15);
            border-radius: 25px;
            padding: 35px 30px;
            text-align: center;
            transition: all 0.4s cubic-bezier(0.175, 0.885, 0.32, 1.275);
            box-shadow: 0 8px 32px rgba(0, 0, 0, 0.3);
            position: relative;
            overflow: hidden;
        }

        .sensor-card::before {
            content: '';
            position: absolute;
            top: 0;
            left: 0;
            right: 0;
            height: 3px;
            background: linear-gradient(90deg, transparent, rgba(102, 126, 234, 0.6), transparent);
            transform: translateX(-100%);
            transition: transform 0.6s ease;
        }

        .sensor-card:hover::before {
            transform: translateX(100%);
        }

        .sensor-card:hover {
            transform: translateY(-10px) scale(1.02);
            box-shadow: 0 20px 60px rgba(0, 0, 0, 0.5);
            border-color: rgba(255, 255, 255, 0.25);
            background: rgba(255, 255, 255, 0.12);
        }

        .sensor-icon {
            font-size: 2.5em;
            margin-bottom: 15px;
            opacity: 0.9;
        }

        .sensor-label {
            font-size: 0.95em;
            opacity: 0.7;
            font-weight: 400;
            margin-bottom: 12px;
            text-transform: uppercase;
            letter-spacing: 1px;
            color: #b0b0c0;
        }

        .sensor-value {
            font-size: 2.2em;
            font-weight: 700;
            margin: 10px 0;
            background: linear-gradient(135deg, #ffffff 0%, #a8b2ff 100%);
            -webkit-background-clip: text;
            -webkit-text-fill-color: transparent;
            background-clip: text;
        }

        .last-update {
            text-align: center;
            margin-top: 30px;
            color: #90909f;
            font-size: 0.9em;
            opacity: 0.7;
        }

        /* 🔥 RESPONSIVE */
        @media (min-width: 769px) {
            .sensor-grid {
                grid-template-columns: repeat(3, 1fr);
                grid-template-rows: repeat(9, auto);
            }
        }

        @media (max-width: 768px) {
            body { padding: 20px 15px; }
            .welcome-card { padding: 30px 25px; }
            h1 { font-size: 2em; }
            .sensor-grid { 
                grid-template-columns: 1fr;
                gap: 20px;
            }
            .sensor-card { padding: 25px 20px; }
            .dashboard-header { 
                flex-direction: column;
                gap: 15px;
                align-items: flex-start;
            }
        }
    </style>

</head>
<body>
    <div class="content-wrapper">
        <!-- Welcome Card -->
        <div class="welcome-card">
            <h1>Hallo Leni & Manu! 👋</h1>
            <div class="time-display" id="zeit">Lädt...</div>
            <p class="subtitle">
                Willkommen zu Hause. Heute wird ein super Tag! Makes es euch gemütlich.
            </p>
            <div class="weather-badge">
                <span class="weather-icon">🌤️</span>
                <span id="wetter">Lädt...</span>
            </div>
        </div>

        <!-- Dashboard Header -->
        <div class="dashboard-header">
            <h2>📊 Home Dashboard</h2>
            <select class="dropdown" id="raumAuswahl" onchange="changeSensorView(this.value)">
                <option value="alle">👥 Alle Sensoren</option>
                <option value="esp32">💙 ESP32 Indoor</option>
                <option value="pico">🧡 Pico W Outdoor</option>
            </select>
        </div>

        <!-- Status Indicator -->
        <div class="status">
            <span class="status-indicator"></span>
            <span id="status-text">Verbunden</span>
        </div>

        <!-- 🔥 SENSOR GRID - FLACHES GRID SYSTEM -->
        <div class="sensor-grid" id="sensor-container">
            <!-- ESP32 Überschrift -->
            <div class="esp32-title" id="esp32-title">💙 ESP32 Indoor</div>
            
            <!-- ESP32 Sensoren (3 Karten nebeneinander) -->
            <div class="sensor-card esp32-card">
                <div class="sensor-icon">🌡️</div>
                <div class="sensor-label">Temperatur</div>
                <div class="sensor-value"><span id="temperature">--</span>°C</div>
            </div>
            <div class="sensor-card esp32-card">
                <div class="sensor-icon">💧</div>
                <div class="sensor-label">Luftfeuchtigkeit</div>
                <div class="sensor-value"><span id="humidity">--</span>%</div>
            </div>
            <div class="sensor-card esp32-card">
                <div class="sensor-icon">🌬️</div>
                <div class="sensor-label">Luftdruck</div>
                <div class="sensor-value"><span id="pressure">--</span> hPa</div>
            </div>

            <!-- Pico W Überschrift -->
            <div class="pico-title" id="pico-title">🧡 Pico W Outdoor</div>
            
            <!-- Pico W Sensoren (3 Karten nebeneinander) -->
            <div class="sensor-card pico-card">
                <div class="sensor-icon">🌡️</div>
                <div class="sensor-label">Temperatur</div>
                <div class="sensor-value"><span id="pico_temperature">--</span>°C</div>
            </div>
            <div class="sensor-card pico-card">
                <div class="sensor-icon">💧</div>
                <div class="sensor-label">Luftfeuchtigkeit</div>
                <div class="sensor-value"><span id="pico_humidity">--</span>%</div>
            </div>
            <div class="sensor-card pico-card">
                <div class="sensor-icon">🌬️</div>
                <div class="sensor-label">Luftdruck</div>
                <div class="sensor-value"><span id="pico_pressure">--</span> hPa</div>
            </div>

            <!-- 🔥 SD-Chart (IMMER ganz unten) -->
            <div class="sensor-card full-width">
                <div class="sensor-icon">📊</div>
                <div class="sensor-label">SD-Log (letzte Messungen)</div>
                <canvas id="sdChart"></canvas>
                <div id="sdStats" style="font-size:1.1em;margin-top:10px;font-weight:500;color:#ffc107;">
                    Lädt...
                </div>
            </div>
        </div>

        <!-- Last Update -->
        <div class="last-update">
            Letzte Aktualisierung: <span id="last-update">--</span>
        </div>
    </div>
</body>
</html>
)rawliteral";

#endif
