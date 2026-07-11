import test from "node:test";
import assert from "node:assert/strict";
import { readFileSync } from "node:fs";
import { fileURLToPath } from "node:url";
import { dirname, join } from "node:path";
import { JSDOM } from "jsdom";

const __dirname = dirname(fileURLToPath(import.meta.url));
const EXT_DIR = join(__dirname, "..");
const DEFAULTS_SRC_PATH = join(EXT_DIR, "content_scripts", "lib", "defaults.js");
const POPUP_SRC_PATH = join(EXT_DIR, "popup", "popup.js");

function createPopupHtml() {
  return `<!doctype html>
<html>
<body>
  <input id="enabled" type="checkbox">
  <input id="capture-alert-toasts" type="checkbox">
  <input id="capture-private-alerts" type="checkbox">
  <input id="include-tab-url" type="checkbox">
  <input id="endpoint" type="url">
  <input id="secret" type="password">
  <button id="save" type="button">Save</button>
  <button id="test" type="button">Send test</button>
  <button id="clear" type="button">Clear log</button>
  <span id="status"></span>
  <div id="logs"></div>
</body>
</html>`;
}

async function flushMicrotasks() {
  await new Promise((resolve) => setTimeout(resolve, 0));
}

function buildPopupEnv() {
  const dom = new JSDOM(createPopupHtml(), {
    url: "chrome-extension://test/popup/popup.html",
    runScripts: "outside-only"
  });
  const { window } = dom;
  const listeners = new Set();
  const store = {
    enabled: true,
    capture_alert_toasts: true,
    capture_private_alerts: true,
    endpoint: "http://127.0.0.1:6560/api/v1/tradingview/signal",
    secret: "",
    include_tab_url: false,
    logs: []
  };

  window.setInterval = () => 1;
  window.clearInterval = () => {};
  window.chrome = {
    runtime: {
      lastError: null,
      sendMessage(message, callback) {
        if (message && message.type === "check_bridge") {
          callback({ ok: true });
        } else if (message && message.type === "clear_logs") {
          store.logs = [];
          callback({ ok: true });
        } else {
          callback({ ok: true, accepted: true });
        }
      }
    },
    storage: {
      local: {
        get(defaults) {
          return Promise.resolve({ ...(defaults || {}), ...store });
        },
        set(values) {
          Object.assign(store, values || {});
          return Promise.resolve();
        }
      },
      onChanged: {
        addListener(listener) {
          listeners.add(listener);
        },
        removeListener(listener) {
          listeners.delete(listener);
        }
      }
    }
  };

  window.eval(readFileSync(DEFAULTS_SRC_PATH, "utf8"));
  window.eval(readFileSync(POPUP_SRC_PATH, "utf8"));

  return {
    dom,
    window,
    store,
    emitStorageChange(changes) {
      for (const listener of listeners) listener(changes, "local");
    }
  };
}

test("popup refreshes recent events when logs change in storage", async () => {
  const { window, store, emitStorageChange } = buildPopupEnv();

  window.document.dispatchEvent(new window.Event("DOMContentLoaded"));
  await flushMicrotasks();
  await flushMicrotasks();

  assert.match(window.document.getElementById("logs").textContent, /No events yet/);

  store.logs = [{
    level: "info",
    text: "Sent BUY EURUSD",
    time: "2026-07-11T00:00:00.000Z"
  }];
  emitStorageChange({ logs: { oldValue: [], newValue: store.logs } });
  await flushMicrotasks();

  assert.match(window.document.getElementById("logs").textContent, /Sent BUY EURUSD/);
});
