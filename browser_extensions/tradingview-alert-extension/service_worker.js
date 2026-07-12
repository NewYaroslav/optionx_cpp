"use strict";

importScripts("content_scripts/lib/defaults.js");

const MAX_LOGS = 50;
const FETCH_TIMEOUT_MS = 3000;
const HEALTH_PATH = "/health";

// Classifies fetch errors into honest categories. Browser's opaque
// TypeError "Failed to fetch" cannot distinguish between network failures
// (bridge offline, DNS, port closed) and CORS preflight rejection from
// a single signal-send error object. The popup uses /health as a separate
// reachability check.
function classifyFetchError(error) {
  if (error && error.name === "AbortError") return "timeout";
  if (error instanceof TypeError) return "network_or_cors";
  return "other";
}

async function postSignal(endpoint, body, secret) {
  const controller = new AbortController();
  const timeoutId = setTimeout(() => controller.abort(), FETCH_TIMEOUT_MS);
  try {
    // mode: "cors" makes the preflight requirements explicit (any 4xx/5xx
    // or missing CORS headers will surface as a real error). credentials:
    // "omit" prevents extension cookies from reaching the local bridge.
    return await fetch(endpoint, {
      method: "POST",
      mode: "cors",
      credentials: "omit",
      headers: {
        "Content-Type": "application/json",
        "X-OptionX-Secret": secret || ""
      },
      body: JSON.stringify(body),
      signal: controller.signal
    });
  } finally {
    clearTimeout(timeoutId);
  }
}

function healthEndpointFromSignalEndpoint(endpoint) {
  const url = new URL(endpoint || OptionXDefaults.DEFAULTS.endpoint);
  url.pathname = HEALTH_PATH;
  url.search = "";
  url.hash = "";
  return url.toString();
}

async function getHealth(endpoint) {
  const controller = new AbortController();
  const timeoutId = setTimeout(() => controller.abort(), FETCH_TIMEOUT_MS);
  try {
    return await fetch(endpoint, {
      method: "GET",
      mode: "cors",
      credentials: "omit",
      signal: controller.signal
    });
  } finally {
    clearTimeout(timeoutId);
  }
}

chrome.runtime.onInstalled.addListener(async (details) => {
  if (details.reason === "install") {
    await chrome.storage.local.set(OptionXDefaults.DEFAULTS);
  } else {
    const current = await chrome.storage.local.get(OptionXDefaults.DEFAULTS);
    await chrome.storage.local.set(current);
  }
  await setBadge("idle");
});

chrome.runtime.onMessage.addListener((message, sender, sendResponse) => {
  if (!message || typeof message !== "object") return false;

  if (message.type === "tradingview_alert") {
    handleTradingViewAlert(message.payload, sender)
      .then(sendResponse)
      .catch((error) => {
        const text = error && error.message ? error.message : String(error);
        writeLog("error", `Unhandled send error: ${text}`);
        sendResponse({ ok: false, error: text });
      });
    return true;
  }

  if (message.type === "content_status") {
    handleContentStatus(message, sender)
      .then(sendResponse)
      .catch((error) => {
        const text = error && error.message ? error.message : String(error);
        sendResponse({ ok: false, error: text });
      });
    return true;
  }

  if (message.type === "check_bridge") {
    handleBridgeHealth()
      .then(sendResponse)
      .catch((error) => {
        const text = error && error.message ? error.message : String(error);
        sendResponse({ ok: false, error: text, error_kind: classifyFetchError(error) });
      });
    return true;
  }

  if (message.type === "get_logs") {
    chrome.storage.local.get({ logs: [] }).then(({ logs }) => {
      sendResponse({ ok: true, logs });
    });
    return true;
  }

  if (message.type === "clear_logs") {
    chrome.storage.local.set({ logs: [] }).then(() => {
      sendResponse({ ok: true });
    });
    return true;
  }

  return false;
});

async function handleContentStatus(message, sender) {
  if (message.status === "observer_active") {
    await writeLog("info", `TradingView observer active${tabSuffix(sender)}`);
    return { ok: true };
  }
  await writeLog(
    "info",
    `TradingView content status: ${message.status || "unknown"}${formatStatusDetails(message.details)}${tabSuffix(sender)}`
  );
  return { ok: true };
}

function formatStatusDetails(details) {
  if (!details || typeof details !== "object") return "";
  const fields = [];
  for (const key of ["host", "path", "type", "count", "methods", "keys", "action", "symbol"]) {
    if (details[key] !== null && details[key] !== undefined && String(details[key]) !== "") {
      fields.push(`${key}=${String(details[key]).slice(0, 120)}`);
    }
  }
  return fields.length > 0 ? ` [${fields.join(" ")}]` : "";
}

function tabSuffix(sender) {
  const tab = sender && sender.tab ? sender.tab : null;
  if (!tab || !tab.url) return "";
  try {
    const url = new URL(tab.url);
    const symbol = url.searchParams.get("symbol");
    return symbol ? ` (${symbol})` : ` (${url.host})`;
  } catch (_) {
    return "";
  }
}

async function handleBridgeHealth() {
  const config = await chrome.storage.local.get(OptionXDefaults.DEFAULTS);
  if (!config.enabled) {
    await setBadge("idle");
    return { ok: true, disabled: true };
  }

  let healthEndpoint;
  try {
    healthEndpoint = healthEndpointFromSignalEndpoint(config.endpoint);
  } catch (error) {
    await setBadge("error");
    return {
      ok: false,
      error: error && error.message ? error.message : String(error),
      error_kind: "invalid_endpoint"
    };
  }

  try {
    const response = await getHealth(healthEndpoint);
    const responseText = await response.text();
    const body = parseResponse(responseText);
    const ok = response.ok && (!body || body.ok !== false);
    await setBadge(ok ? "ok" : "error");
    return {
      ok,
      status: response.status,
      endpoint: healthEndpoint,
      response: body
    };
  } catch (error) {
    await setBadge("error");
    return {
      ok: false,
      endpoint: healthEndpoint,
      error: error && error.message ? error.message : String(error),
      error_kind: classifyFetchError(error)
    };
  }
}

async function handleTradingViewAlert(payload, sender) {
  const config = await chrome.storage.local.get(OptionXDefaults.DEFAULTS);
  if (!config.enabled) {
    await writeLog("info", "Signal ignored because extension is disabled.");
    return { ok: true, accepted: false, disabled: true };
  }

  if (!payload || typeof payload !== "object") {
    await writeLog("error", "Rejected empty TradingView payload.");
    await setBadge("error");
    return { ok: false, error: "empty payload" };
  }

  const sourceGate = sourceCaptureGate(payload, config);
  if (!sourceGate.enabled) {
    return {
      ok: true,
      accepted: false,
      disabled_source: true,
      source_kind: sourceGate.source_kind
    };
  }

  const body = {
    ...payload,
    extension: (() => {
      const tabUrl = sender && sender.tab ? sender.tab.url : null;
      let host = null, symbolFromUrl = null, interval = null;
      if (tabUrl) {
        try {
          const u = new URL(tabUrl);
          host = u.host || null;
          symbolFromUrl = u.searchParams.get("symbol");
          interval = u.searchParams.get("interval");
        } catch (_) {}
      }
      const ext = { id: chrome.runtime.id, tab_id: sender && sender.tab ? sender.tab.id : null };
      if (host) ext.host = host;
      if (symbolFromUrl) ext.symbol_from_url = symbolFromUrl;
      if (interval) ext.interval = interval;
      if (config.include_tab_url === true && tabUrl) ext.url = tabUrl;
      return ext;
    })()
  };

  try {
    const response = await postSignal(config.endpoint, body, config.secret);

    const responseText = await response.text();
    if (!response.ok) {
      await writeLog(
        "error",
        `Bridge rejected ${shortSignal(payload)}: HTTP ${response.status}`
      );
      await setBadge("error");
      return {
        ok: false,
        accepted: false,
        status: response.status,
        response: responseText.slice(0, 500)
      };
    }

    await writeLog("info", `Sent ${shortSignal(payload)}`);
    await setBadge("ok");
    return {
      ok: true,
      accepted: true,
      status: response.status,
      response: parseResponse(responseText)
    };
  } catch (error) {
    const text = error && error.message ? error.message : String(error);
    const kind = classifyFetchError(error);
    await writeLog("error", `Bridge ${kind} for ${shortSignal(payload)}: ${text}`);
    await setBadge("error");
    return { ok: false, accepted: false, error: text, error_kind: kind };
  }
}

function sourceCaptureGate(payload, config) {
  const sourceKind = String(payload.source_kind || "").toLowerCase();
  if (sourceKind === "alert_toast_dom" && config.capture_alert_toasts === false) {
    return { enabled: false, source_kind: sourceKind };
  }
  if (
    (sourceKind === "private_pricealerts_ws" ||
      sourceKind === "tradingview_private_pricealerts_ws" ||
      sourceKind.includes("pricealerts")) &&
    config.capture_private_alerts === false
  ) {
    return { enabled: false, source_kind: sourceKind };
  }
  if (sourceKind === "private_chart_study_alert_messages" &&
      config.capture_chart_study_alerts === false) {
    return { enabled: false, source_kind: sourceKind };
  }
  return { enabled: true, source_kind: sourceKind };
}

function parseResponse(text) {
  if (!text) return null;
  try {
    return JSON.parse(text);
  } catch {
    return text.slice(0, 500);
  }
}

function shortSignal(payload) {
  const action = payload.action || "alert";
  const symbol = payload.symbol || "unknown";
  const message = payload.message || payload.description || "";
  return `${String(action).toUpperCase()} ${symbol} ${message}`.trim().slice(0, 140);
}

let logQueue = Promise.resolve();

async function writeLog(level, text) {
  const entry = {
    level,
    text,
    time: new Date().toISOString()
  };
  logQueue = logQueue.then(async () => {
    try {
      const { logs } = await chrome.storage.local.get({ logs: [] });
      const nextLogs = Array.isArray(logs) ? logs.slice(-MAX_LOGS + 1) : [];
      nextLogs.push(entry);
      await chrome.storage.local.set({ logs: nextLogs });
    } catch (err) {
      console.error("writeLog failed:", err);
    }
  });
  return logQueue;
}

async function setBadge(state) {
  if (state === "ok") {
    await chrome.action.setBadgeBackgroundColor({ color: "#138a5b" });
    await chrome.action.setBadgeText({ text: "OK" });
    return;
  }
  if (state === "error") {
    await chrome.action.setBadgeBackgroundColor({ color: "#c33434" });
    await chrome.action.setBadgeText({ text: "!" });
    return;
  }
  await chrome.action.setBadgeText({ text: "" });
}
