const state = {
    connected: false,
    lastSync: "--",
    sensors: {
        gas: 0,
        sound: 0,
        flame: false,
        water: 0,
        temperature: 0,
        humidity: 0,
        light: 0,
        door: false,
        motion: false,
        stripLed: false,
        buzzer: false,
        warningLed: false,
        pump: false,
        fan: false
    },
    lcdLines: [
        "SISTEM BASLIYOR    ",
        "ESP32 BEKLENIYOR   ",
        "SENSORLER OKUNUYOR ",
        "LUTFEN BEKLEYIN    "
    ]
};

const ui = {
    banner: document.getElementById("connection-banner"),
    lastSync: document.getElementById("last-sync"),
    gasValue: document.getElementById("gas-value"),
    fanStatus: document.getElementById("fan-status"),
    soundValue: document.getElementById("sound-value"),
    soundStatus: document.getElementById("sound-status"),
    flameStatus: document.getElementById("flame-status"),
    waterValue: document.getElementById("water-value"),
    pumpStatus: document.getElementById("pump-status"),
    tempValue: document.getElementById("temp-value"),
    humidityValue: document.getElementById("humidity-value"),
    lightValue: document.getElementById("light-value"),
    stripStatus: document.getElementById("strip-status"),
    doorStatus: document.getElementById("door-status"),
    motionStatus: document.getElementById("motion-status"),
    buzzerStatus: document.getElementById("buzzer-status"),
    warningLedStatus: document.getElementById("warning-led-status"),
    systemStatus: document.getElementById("system-status"),
    lcdLines: document.querySelectorAll("#lcd-screen span"),
    refreshButton: document.getElementById("refresh-button"),
    quickButtons: document.querySelectorAll(".quick-button")
};

function setPanelTone(id, tone) {
    const card = document.querySelector(`[data-card="${id}"]`);
    if (!card) return;
    card.classList.remove("panel--amber", "panel--red", "panel--green", "panel--cyan");
    card.classList.add(`panel--${tone}`);
}

function setConnection(online, message) {
    state.connected = online;
    ui.banner.classList.toggle("is-online", online);
    ui.banner.classList.toggle("is-offline", !online);
    ui.banner.textContent = message;
}

function writeLCD(lines) {
    ui.lcdLines.forEach((line, index) => {
        line.textContent = (lines[index] || "").padEnd(20, " ").slice(0, 20);
    });
}

function render() {
    const s = state.sensors;
    const alertMode = s.flame || s.gas >= 300 || s.door || s.motion;

    ui.gasValue.innerHTML = `${s.gas} <span>PPM</span>`;
    ui.fanStatus.textContent = s.fan ? "ACIK" : "KAPALI";
    ui.soundValue.innerHTML = `${s.sound} <span>dB</span>`;
    ui.soundStatus.textContent = s.sound >= 70 ? "YUKSEK" : "SESSIZ";
    ui.flameStatus.textContent = s.flame ? "DURUM: YANGIN RISKI" : "DURUM: GUVENLI";
    ui.waterValue.textContent = `% ${s.water}`;
    ui.pumpStatus.textContent = s.pump ? "AKTIF" : "DURDU";
    ui.tempValue.innerHTML = `${s.temperature}<span>C</span>`;
    ui.humidityValue.textContent = `% ${s.humidity}`;
    ui.lightValue.textContent = `% ${s.light}`;
    ui.stripStatus.textContent = s.stripLed ? "ACIK" : "KAPALI";
    ui.doorStatus.textContent = s.door ? "ACIK" : "KAPALI";
    ui.motionStatus.textContent = s.motion ? "ALGILANDI" : "YOK";
    ui.buzzerStatus.textContent = s.buzzer ? "AKTIF" : "KAPALI";
    ui.warningLedStatus.textContent = s.warningLed ? "UYARI MODU" : "NORMAL";
    ui.systemStatus.textContent = alertMode ? "SISTEM ALARMDA" : "SISTEM CALISIYOR";
    ui.systemStatus.className = alertMode ? "status-line danger" : "status-line ok";
    ui.lastSync.textContent = `Son guncelleme: ${state.lastSync}`;

    setPanelTone("gas", s.gas >= 450 ? "red" : "amber");
    setPanelTone("sound", s.sound >= 70 ? "amber" : "amber");
    setPanelTone("flame", s.flame ? "red" : "cyan");
    setPanelTone("water", s.water <= 25 ? "amber" : "green");
    setPanelTone("temperature", s.temperature >= 35 ? "amber" : "cyan");
    setPanelTone("humidity", s.humidity <= 35 ? "amber" : "cyan");
    setPanelTone("light", s.light <= 20 ? "amber" : "amber");
    setPanelTone("strip", s.stripLed ? "green" : "cyan");
    setPanelTone("door", s.door ? "amber" : "cyan");
    setPanelTone("motion", s.motion ? "amber" : "cyan");
    setPanelTone("buzzer", s.buzzer ? "red" : "cyan");
    setPanelTone("warning-led", s.warningLed ? "red" : "cyan");
    setPanelTone("system", alertMode ? "red" : "cyan");

    ui.quickButtons.forEach((button) => {
        const active = Boolean(s[button.dataset.device]);
        button.classList.toggle("is-active", active);
    });

    writeLCD(state.lcdLines);
}

function applyPayload(payload) {
    state.sensors.gas = Number(payload.gas ?? state.sensors.gas);
    state.sensors.sound = Number(payload.sound ?? state.sensors.sound);
    state.sensors.flame = Boolean(payload.flame ?? state.sensors.flame);
    state.sensors.water = Number(payload.water ?? state.sensors.water);
    state.sensors.temperature = Number(payload.temperature ?? state.sensors.temperature);
    state.sensors.humidity = Number(payload.humidity ?? state.sensors.humidity);
    state.sensors.light = Number(payload.light ?? state.sensors.light);
    state.sensors.door = Boolean(payload.door ?? state.sensors.door);
    state.sensors.motion = Boolean(payload.motion ?? state.sensors.motion);
    state.sensors.stripLed = Boolean(payload.stripLed ?? state.sensors.stripLed);
    state.sensors.buzzer = Boolean(payload.buzzer ?? state.sensors.buzzer);
    state.sensors.warningLed = Boolean(payload.warningLed ?? state.sensors.warningLed);
    state.sensors.pump = Boolean(payload.pump ?? state.sensors.pump);
    state.sensors.fan = Boolean(payload.fan ?? state.sensors.fan);
    state.lcdLines = Array.isArray(payload.lcdLines) ? payload.lcdLines.slice(0, 4) : state.lcdLines;
    state.lastSync = new Date().toLocaleTimeString("tr-TR", {
        hour: "2-digit",
        minute: "2-digit",
        second: "2-digit"
    });
}

async function fetchStatus() {
    try {
        const response = await fetch("/api/status", { cache: "no-store" });
        if (!response.ok) {
            throw new Error(`HTTP ${response.status}`);
        }

        const payload = await response.json();
        applyPayload(payload);
        setConnection(true, "ESP32 BAGLI - CANLI VERI AKIYOR");
        render();
    } catch (error) {
        setConnection(false, "ESP32 BAGLANTISI KESIK");
        render();
    }
}

async function sendDevice(device, value) {
    try {
        const response = await fetch("/api/device", {
            method: "POST",
            headers: {
                "Content-Type": "application/json"
            },
            body: JSON.stringify({ device, value })
        });

        if (!response.ok) {
            throw new Error(`HTTP ${response.status}`);
        }

        await fetchStatus();
    } catch (error) {
        setConnection(false, "KOMUT GONDERILEMEDI");
        render();
    }
}

ui.refreshButton.addEventListener("click", fetchStatus);
ui.quickButtons.forEach((button) => {
    button.addEventListener("click", () => {
        const device = button.dataset.device;
        sendDevice(device, !state.sensors[device]);
    });
});

render();
fetchStatus();
window.setInterval(fetchStatus, 3000);
