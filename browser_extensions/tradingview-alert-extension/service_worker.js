"use strict";

const DEFAULTS = {
  enabled: false,
  endpoint: "http://127.0.0.1:6560/api/v1/tradingview/signal",
  secret: "",
  include_tab_url: false
};

const MAX_LOGS = 50;
const FETCH_TIMEOUT_MS = 3000;

async function postSignal(endpoint, body, secret) {
  const controller = new AbortController();
  const timeoutId = setTimeout(() => controller.abort(), FETCH_TIMEOUT_MS);
  try {
    return await fetch(endpoint, {
      method: "POST",
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

chrome.runtime.onInstalled.addListener(async (details) => {
  if (details.reason === "install") {
    await chrome.storage.local.set(DEFAULTS);
  } else {
    const current = await chrome.storage.local.get(DEFAULTS);
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

async function handleTradingViewAlert(payload, sender) {
  const config = await chrome.storage.local.get(DEFAULTS);
  if (!config.enabled) {
    await writeLog("info", "Signal ignored because extension is disabled.");
    return { ok: true, accepted: false, disabled: true };
  }

  if (!payload || typeof payload !== "object") {
    await writeLog("error", "Rejected empty TradingView payload.");
    await setBadge("error");
    return { ok: false, error: "empty payload" };
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
    if (error && error.name === "AbortError") {
      await writeLog("error", `Bridge timeout for ${shortSignal(payload)} after ${FETCH_TIMEOUT_MS}ms`);
    } else {
      await writeLog("error", `Bridge offline for ${shortSignal(payload)}: ${text}`);
    }
    await setBadge("error");
    return { ok: false, accepted: false, error: text };
  }
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
