"use strict";

const enabledEl = document.getElementById("enabled");
const captureAlertToastsEl = document.getElementById("capture-alert-toasts");
const capturePrivateAlertsEl = document.getElementById("capture-private-alerts");
const captureChartStudyAlertsEl = document.getElementById("capture-chart-study-alerts");
const includeTabUrlEl = document.getElementById("include-tab-url");
const endpointEl = document.getElementById("endpoint");
const secretEl = document.getElementById("secret");
const saveEl = document.getElementById("save");
const testEl = document.getElementById("test");
const clearEl = document.getElementById("clear");
const logsEl = document.getElementById("logs");
const statusEl = document.getElementById("status");
let healthTimer = null;

document.addEventListener("DOMContentLoaded", init);
chrome.storage.onChanged.addListener(handleStorageChanged);
window.addEventListener("beforeunload", () => {
  if (healthTimer !== null) clearInterval(healthTimer);
  chrome.storage.onChanged.removeListener(handleStorageChanged);
});
saveEl.addEventListener("click", saveConfig);
testEl.addEventListener("click", sendTestSignal);
clearEl.addEventListener("click", clearLogs);

async function init() {
  const config = await chrome.storage.local.get(OptionXDefaults.DEFAULTS);
  enabledEl.checked = Boolean(config.enabled);
  captureAlertToastsEl.checked = config.capture_alert_toasts !== false;
  capturePrivateAlertsEl.checked = config.capture_private_alerts !== false;
  captureChartStudyAlertsEl.checked = config.capture_chart_study_alerts !== false;
  includeTabUrlEl.checked = Boolean(config.include_tab_url);
  endpointEl.value = config.endpoint;
  secretEl.value = config.secret;
  await renderLogs();
  await checkBridge();
  healthTimer = setInterval(() => checkBridge({ showChecking: false }), 5000);
}

async function saveConfig() {
  await chrome.storage.local.set({
    enabled: enabledEl.checked,
    capture_alert_toasts: captureAlertToastsEl.checked,
    capture_private_alerts: capturePrivateAlertsEl.checked,
    capture_chart_study_alerts: captureChartStudyAlertsEl.checked,
    include_tab_url: includeTabUrlEl.checked,
    endpoint: endpointEl.value.trim() || OptionXDefaults.DEFAULTS.endpoint,
    secret: secretEl.value
  });
  await checkBridge();
}

function sendTestSignal() {
  const now = new Date();
  chrome.runtime.sendMessage({
    type: "tradingview_alert",
    payload: {
      version: 1,
      source: "tradingview_extension",
      source_kind: "manual_test",
      event_id: `manual_test:${now.getTime()}`,
      // manual_test fingerprints intentionally free-form (timestamp-based). FNV1a contract
      // applies to production tv_toast payloads only; bridge must not rely on this shape.
      fingerprint: `manual_test:${now.getTime()}`,
      symbol: "EURUSD",
      action: "buy",
      raw_action: "BUY",
      direction: null,
      raw_direction: null,
      price: null,
      time: now.toISOString(),
      title: "Manual test",
      message: "BUY EURUSD",
      raw: {
        title: "Manual test",
        description: "BUY EURUSD"
      }
    }
  }, async (response) => {
    if (chrome.runtime.lastError) {
      setStatus("error", "error");
      return;
    }
    setStatus(response && response.ok ? "ok" : "error", response && response.ok ? "sent" : "error");
    await renderLogs();
  });
}

function clearLogs() {
  chrome.runtime.sendMessage({ type: "clear_logs" }, async () => {
    await renderLogs();
  });
}

async function renderLogs() {
  const { logs } = await chrome.storage.local.get({ logs: [] });
  logsEl.innerHTML = "";
  const items = Array.isArray(logs) ? logs.slice().reverse() : [];
  if (items.length === 0) {
    const empty = document.createElement("div");
    empty.className = "log-entry";
    empty.textContent = "No events yet.";
    logsEl.appendChild(empty);
    return;
  }

  for (const entry of items) {
    const item = document.createElement("div");
    item.className = `log-entry ${entry.level === "error" ? "error" : ""}`;

    const time = document.createElement("span");
    time.className = "log-time";
    time.textContent = entry.time || "";

    const text = document.createElement("span");
    text.textContent = entry.text || "";

    item.appendChild(time);
    item.appendChild(text);
    logsEl.appendChild(item);
  }
}

function handleStorageChanged(changes, areaName) {
  if (areaName !== "local" || !changes || !changes.logs) return;
  void renderLogs();
}

function setStatus(kind, text) {
  statusEl.className = `status ${kind}`;
  statusEl.textContent = text;
}

function checkBridge({ showChecking = true } = {}) {
  if (showChecking) setStatus("checking", "checking");
  return new Promise((resolve) => {
    chrome.runtime.sendMessage({ type: "check_bridge" }, (response) => {
      if (chrome.runtime.lastError) {
        setStatus("error", "error");
        resolve();
        return;
      }
      if (response && response.disabled) {
        setStatus("idle", "disabled");
        resolve();
        return;
      }
      if (response && response.ok) {
        setStatus("ok", "online");
        resolve();
        return;
      }
      if (response && response.error_kind === "invalid_endpoint") {
        setStatus("error", "bad url");
        resolve();
        return;
      }
      setStatus("error", "offline");
      resolve();
    });
  });
}
