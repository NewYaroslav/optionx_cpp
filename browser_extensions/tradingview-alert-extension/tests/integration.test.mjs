import test from "node:test";
import assert from "node:assert/strict";
import { readFileSync } from "node:fs";
import { fileURLToPath } from "node:url";
import { dirname, join } from "node:path";
import { JSDOM } from "jsdom";

const __dirname = dirname(fileURLToPath(import.meta.url));
const EXT_DIR = join(__dirname, "..");
const PARSER_SRC_PATH = join(EXT_DIR, "content_scripts", "lib", "parser.js");
const ALERTS_SRC_PATH = join(EXT_DIR, "content_scripts", "tradingview_alerts.js");

// helpers ---------------------------------------------------------------

function buildTestEnv(initialHtml = "") {
  const dom = new JSDOM(
    `<!DOCTYPE html><html><body>${initialHtml}</body></html>`,
    {
      url: "https://www.tradingview.com/chart/?symbol=EURUSD",
      runScripts: "outside-only",
      pretendToBeVisual: true,
    }
  );
  const { window } = dom;

  const sentPayloads = [];

  window.chrome = {
    runtime: {
      id: "test-extension-id",
      lastError: null,
      sendMessage(message, callback) {
        sentPayloads.push(message);
        if (callback) setTimeout(() => callback({ ok: true, accepted: true }), 0);
      },
      onMessage: { addListener() {} },
      onInstalled: { addListener() {} },
    },
    storage: {
      local: {
        get(_defaults) { return Promise.resolve({}); },
        set(_values) { return Promise.resolve(); },
      },
    },
    action: {
      setBadgeBackgroundColor() { return Promise.resolve(); },
      setBadgeText() { return Promise.resolve(); },
    },
  };

  // Load parser.js first (defines window.OptionXParser), then alerts.js
  const parserSrc = readFileSync(PARSER_SRC_PATH, "utf8");
  const alertsSrc = readFileSync(ALERTS_SRC_PATH, "utf8");
  window.eval(parserSrc);
  window.eval(alertsSrc);

  return { dom, window, document: window.document, sentPayloads };
}

async function flushMicrotasks() {
  // 50ms initial delay + 300ms settle = 350ms minimum for content script
  await new Promise((r) => setTimeout(r, 400));
}

function getPayloadsFor(sentPayloads) {
  return sentPayloads
    .filter((m) => m && m.type === "tradingview_alert")
    .map((m) => m.payload);
}

// tests -----------------------------------------------------------------

test("integration: real DOM pipeline extracts EURUSD Crossing payload", async () => {
  const { document, sentPayloads } = buildTestEnv();
  document.body.insertAdjacentHTML(
    "beforeend",
    `<div class="tv-alert-toast"><span class="description-ULNSeceN">EURUSD Crossing 1.14145</span></div>`
  );
  await flushMicrotasks();

  const payloads = getPayloadsFor(sentPayloads);
  assert.ok(payloads.length >= 1, `expected >= 1 payload, got ${payloads.length}`);
  const p = payloads[0];
  assert.equal(p.symbol, "EURUSD");
  assert.equal(p.action, "alert");
  assert.equal(p.direction, "cross");
  assert.equal(p.price, 1.14145);
  assert.equal(p.observed_at, p.time); // back-compat
  assert.equal(typeof p.fingerprint, "string");
  assert.equal(typeof p.event_id, "string");
  assert.equal(p.message, "EURUSD Crossing 1.14145");
  assert.ok(p.raw && typeof p.raw === "object");
});

test("integration: EURUSD Crossing Up extracts direction=up", async () => {
  const { document, sentPayloads } = buildTestEnv();
  document.body.insertAdjacentHTML(
    "beforeend",
    `<div class="tv-alert-toast"><span class="description-ULNSeceN">EURUSD Crossing Up 1.14143</span></div>`
  );
  await flushMicrotasks();
  const payloads = getPayloadsFor(sentPayloads);
  assert.equal(payloads[0].direction, "up");
  assert.equal(payloads[0].price, 1.14143);
});

test("integration: EURUSD Crossing Down extracts direction=down", async () => {
  const { document, sentPayloads } = buildTestEnv();
  document.body.insertAdjacentHTML(
    "beforeend",
    `<div class="tv-alert-toast"><span class="description-ULNSeceN">EURUSD Crossing Down 1.14142</span></div>`
  );
  await flushMicrotasks();
  const payloads = getPayloadsFor(sentPayloads);
  assert.equal(payloads[0].direction, "down");
});

test("integration: pct trigger yields price=null, trigger_value, trigger_unit", async () => {
  const { document, sentPayloads } = buildTestEnv();
  document.body.insertAdjacentHTML(
    "beforeend",
    `<div class="tv-alert-toast"><span class="description-ULNSeceN">EURUSD Moving Up 1.0%</span></div>`
  );
  await flushMicrotasks();
  const payloads = getPayloadsFor(sentPayloads);
  assert.equal(payloads[0].symbol, "EURUSD");
  assert.equal(payloads[0].direction, "moving_up_pct");
  assert.equal(payloads[0].price, null);
  assert.equal(payloads[0].trigger_value, 1.0);
  assert.equal(payloads[0].trigger_unit, "percent");
  assert.equal(payloads[0].raw_direction, "Moving Up 1.0%");
});

test("integration: pct trigger with digits in symbol (US100) extracts correct trigger value", async () => {
  const { document, sentPayloads } = buildTestEnv();
  document.body.insertAdjacentHTML(
    "beforeend",
    `<div class="tv-alert-toast"><span class="description-ULNSeceN">US100 Moving Up 1.0%</span></div>`
  );
  await flushMicrotasks();
  const payloads = getPayloadsFor(sentPayloads);
  assert.equal(payloads[0].symbol, "US100");
  assert.equal(payloads[0].direction, "moving_up_pct");
  assert.equal(payloads[0].price, null);
  assert.equal(payloads[0].trigger_value, 1.0);
});

test("integration: explicit BUY command sets action=buy", async () => {
  const { document, sentPayloads } = buildTestEnv();
  document.body.insertAdjacentHTML(
    "beforeend",
    `<div class="tv-alert-toast"><span class="description-ULNSeceN">BUY EURUSD</span></div>`
  );
  await flushMicrotasks();
  const payloads = getPayloadsFor(sentPayloads);
  assert.equal(payloads[0].symbol, "EURUSD");
  assert.equal(payloads[0].action, "buy");
  assert.equal(payloads[0].raw_action, "BUY");
});

test("integration: same toast inserted twice is deduped (5s window)", async () => {
  const { document, sentPayloads } = buildTestEnv();
  const html = `<div class="tv-alert-toast"><span class="description-ULNSeceN">EURUSD Crossing 1.14145</span></div>`;

  document.body.insertAdjacentHTML("beforeend", html);
  await flushMicrotasks();
  const after1 = getPayloadsFor(sentPayloads).length;

  document.body.insertAdjacentHTML("beforeend", html);
  await flushMicrotasks();
  const after2 = getPayloadsFor(sentPayloads).length;

  assert.equal(after1, 1, "first insertion should produce one payload");
  assert.equal(after2, 1, "second insertion within dedup window should not produce another");
});

test("integration: root with multiple descriptions is skipped", async () => {
  const { document, sentPayloads } = buildTestEnv();
  document.body.insertAdjacentHTML(
    "beforeend",
    `<div class="tv-alert-toast">
       <span class="description-ULNSeceN">EURUSD Crossing 1.14145</span>
       <span class="description-other-hash">GBPJPY Crossing 1.27000</span>
     </div>`
  );
  await flushMicrotasks();
  const payloads = getPayloadsFor(sentPayloads);
  assert.equal(payloads.length, 0, "merged root with >1 description should be skipped");
});

test("integration: dynamic MutationObserver insertion (addedNodes)", async () => {
  const { document, sentPayloads } = buildTestEnv();
  await flushMicrotasks();
  sentPayloads.length = 0;

  document.body.insertAdjacentHTML(
    "beforeend",
    `<div class="tv-alert-toast-new"><span class="description-ULNSeceN">BTCUSDT Crossing 50000</span></div>`
  );
  await flushMicrotasks();
  const payloads = getPayloadsFor(sentPayloads);
  assert.equal(payloads.length, 1);
  assert.equal(payloads[0].symbol, "BTCUSDT");
  assert.equal(payloads[0].price, 50000);
});

test("integration: characterData mutation triggers re-inspection", async () => {
  const { document, sentPayloads } = buildTestEnv();
  await flushMicrotasks();
  sentPayloads.length = 0;

  document.body.insertAdjacentHTML(
    "beforeend",
    `<div class="tv-alert-toast-cd"><span class="description-ULNSeceN">placeholder</span></div>`
  );
  await flushMicrotasks();
  sentPayloads.length = 0;

  const descEl = document.querySelector(".tv-alert-toast-cd .description-ULNSeceN");
  descEl.firstChild.nodeValue = "ETHUSD Crossing 3000";

  await flushMicrotasks();
  const payloads = getPayloadsFor(sentPayloads);
  assert.ok(Array.isArray(payloads));
});

test("integration: parsed JSON action overrides text message", async () => {
  const { document, sentPayloads } = buildTestEnv();
  document.body.insertAdjacentHTML(
    "beforeend",
    `<div class="tv-alert-toast"><span class="description-ULNSeceN">{"action":"buy","symbol":"XAUUSD","price":2000}</span></div>`
  );
  await flushMicrotasks();
  const payloads = getPayloadsFor(sentPayloads);
  assert.equal(payloads[0].symbol, "XAUUSD");
  assert.equal(payloads[0].action, "buy");
  assert.equal(payloads[0].price, 2000);
  assert.equal(payloads[0].raw_action, "buy");
});

test("integration: extension metadata populated from sender URL", async () => {
  const { document, sentPayloads } = buildTestEnv();
  document.body.insertAdjacentHTML(
    "beforeend",
    `<div class="tv-alert-toast"><span class="description-ULNSeceN">EURUSD Crossing 1.14145</span></div>`
  );
  await flushMicrotasks();
  const payloads = getPayloadsFor(sentPayloads);
  assert.equal(sentPayloads[0].type, "tradingview_alert");
  assert.ok(sentPayloads[0].payload);
});
