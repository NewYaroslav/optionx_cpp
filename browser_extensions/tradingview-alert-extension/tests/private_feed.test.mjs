import test from "node:test";
import assert from "node:assert/strict";
import { readFileSync } from "node:fs";
import { fileURLToPath } from "node:url";
import { dirname, join } from "node:path";
import { JSDOM } from "jsdom";

const __dirname = dirname(fileURLToPath(import.meta.url));
const EXT_DIR = join(__dirname, "..");
const PAGE_HOOK_SRC_PATH = join(EXT_DIR, "content_scripts", "private_feed_page_hook.js");
const CONTENT_SRC_PATH = join(EXT_DIR, "content_scripts", "tradingview_private_feed.js");

const PRIVATE_FEED_URL = "wss://pushstream.tradingview.com/message-pipe-ws/private_feed";
const CHART_SOCKET_URL = "wss://data.tradingview.com/socket.io/websocket?from=chart%2FX9YvLQhk%2F&type=chart&auth=sessionid";

function createDom() {
  return new JSDOM("<!doctype html><html><head></head><body></body></html>", {
    url: "https://www.tradingview.com/chart/X9YvLQhk/?symbol=FX%3AEURUSD",
    runScripts: "outside-only",
    pretendToBeVisual: true
  });
}

function installMockWebSocket(window) {
  class MockWebSocket {
    static instances = [];
    static CONNECTING = 0;
    static OPEN = 1;
    static CLOSING = 2;
    static CLOSED = 3;

    constructor(url, protocols) {
      this.url = url;
      this.protocols = protocols;
      this.listeners = new Map();
      MockWebSocket.instances.push(this);
    }

    addEventListener(type, listener) {
      const listeners = this.listeners.get(type) || [];
      listeners.push(listener);
      this.listeners.set(type, listeners);
    }

    emitMessage(data) {
      for (const listener of this.listeners.get("message") || []) {
        listener({ data });
      }
    }
  }

  window.WebSocket = MockWebSocket;
  return MockWebSocket;
}

function loadPageHook(window) {
  window.eval(readFileSync(PAGE_HOOK_SRC_PATH, "utf8"));
}

function tradingViewFrame(payload) {
  const text = JSON.stringify(payload);
  return `~m~${text.length}~m~${text}`;
}

function studyAlertFrame({ action, price, time, updateTime, barState = "RT_CONFIRMED" }) {
  const alertMessage = {
    barInfo: {
      barIndex: Math.floor(time / 60000),
      close: price,
      high: price + 1,
      low: price - 1,
      open: price - 0.5,
      time,
      updateTime,
      volume: 0
    },
    msg: JSON.stringify({
      source: "tradingview",
      signal_name: "noisy_rsi_test",
      action,
      symbol: "BTCUSD",
      tickerid: "CRYPTO:BTCUSD",
      price,
      time
    })
  };
  const ns = JSON.stringify({
    data: {
      alertMessages: [alertMessage],
      debug: [
        {
          idx: alertMessage.barInfo.barIndex,
          bs: barState,
          t: new Date(time).toISOString(),
          c: price
        }
      ],
      version: 2
    },
    isUpdate: true
  });
  return tradingViewFrame({
    m: "du",
    p: [
      "cs_test",
      {
        "8x94yO": {
          st: [],
          ns: { d: ns, indexes: "nochange" },
          t: "s1_st1"
        },
        "9S3h0E": {
          st: [],
          ns: { d: ns, indexes: "nochange" },
          t: "s1_st1"
        }
      }
    ]
  });
}

async function flush() {
  await new Promise((resolve) => setTimeout(resolve, 0));
}

test("private feed page hook forwards pricealerts alert_fired frames", async () => {
  const dom = createDom();
  const { window } = dom;
  const MockWebSocket = installMockWebSocket(window);
  const pageMessages = [];
  window.addEventListener("message", (event) => {
    if (event.data && event.data.type === "optionx_tradingview_private_feed" && event.data.payload) {
      pageMessages.push(event.data.payload);
    }
  });

  loadPageHook(window);
  const socket = new window.WebSocket(PRIVATE_FEED_URL);
  assert.equal(MockWebSocket.instances.length, 1);

  socket.emitMessage(JSON.stringify({
    id: 31,
    text: {
      channel: "pricealerts",
      content: {
        m: "alert_fired",
        id: "emrv-244490662",
        _rts: 1783478179763,
        p: {
          fire_id: 53256556946,
          alert_id: 5099741779,
          symbol: "={\"symbol\":\"FX:EURUSD\",\"adjustment\":\"splits\"}",
          message: "EURUSD Crossing 1.14072",
          fire_time: "2026-07-08T02:36:22Z"
        }
      }
    }
  }));
  await flush();

  assert.equal(pageMessages.length, 1);
  const payload = pageMessages[0];
  assert.equal(payload.source_kind, "private_pricealerts_ws");
  assert.equal(payload.method, "alert_fired");
  assert.equal(payload.event_id, "tv_price_alert:53256556946");
  assert.equal(payload.fire_id, "53256556946");
  assert.equal(payload.alert_id, "5099741779");
  assert.equal(payload.symbol, "FX:EURUSD");
  assert.equal(payload.message, "EURUSD Crossing 1.14072");
  assert.equal(payload.text.channel, "pricealerts");
  assert.equal(payload.text.content.m, "alert_fired");
});

test("private feed page hook ignores lifecycle and non-private sockets", async () => {
  const dom = createDom();
  const { window } = dom;
  installMockWebSocket(window);
  const pageMessages = [];
  window.addEventListener("message", (event) => {
    if (event.data && event.data.type === "optionx_tradingview_private_feed" && event.data.payload) {
      pageMessages.push(event.data.payload);
    }
  });

  loadPageHook(window);
  const privateSocket = new window.WebSocket(PRIVATE_FEED_URL);
  const otherSocket = new window.WebSocket("wss://data.tradingview.com/socket.io/websocket");

  privateSocket.emitMessage(JSON.stringify({
    text: {
      channel: "pricealerts",
      content: {
        m: "alerts_updated",
        p: [{ alert_id: 5099741779, active: false }]
      }
    }
  }));
  otherSocket.emitMessage(JSON.stringify({
    text: {
      channel: "pricealerts",
      content: {
        m: "alert_fired",
        p: { fire_id: 1, message: "EURUSD Crossing 1.14" }
      }
    }
  }));
  await flush();

  assert.equal(pageMessages.length, 0);
});

test("chart socket study alertMessages are forwarded once across duplicate study ids", async () => {
  const dom = createDom();
  const { window } = dom;
  installMockWebSocket(window);
  const pageMessages = [];
  window.addEventListener("message", (event) => {
    if (event.data && event.data.type === "optionx_tradingview_private_feed" && event.data.payload) {
      pageMessages.push(event.data.payload);
    }
  });

  loadPageHook(window);
  const socket = new window.WebSocket(CHART_SOCKET_URL);

  socket.emitMessage(studyAlertFrame({
    action: "sell",
    price: 64131.92,
    time: 1783763820000,
    updateTime: 1783763881395
  }));
  await flush();

  assert.equal(pageMessages.length, 1);
  const payload = pageMessages[0];
  assert.equal(payload.source_kind, "private_chart_study_alert_messages");
  assert.equal(payload.method, "du.alertMessages");
  assert.match(payload.event_id, /^tv_study_alert:/);
  assert.equal(payload.chart_session, "cs_test");
  assert.equal(payload.study_id, "8x94yO");
  assert.equal(payload.signal_name, "noisy_rsi_test");
  assert.equal(payload.action, "sell");
  assert.equal(payload.symbol, "BTCUSD");
  assert.equal(payload.tickerid, "CRYPTO:BTCUSD");
  assert.equal(payload.price, 64131.92);
  assert.equal(payload.time, 1783763820000);
  assert.equal(payload.bar_state, "RT_CONFIRMED");
  assert.equal(payload.bar_state_source, "debug_idx");
  assert.deepEqual(Array.from(payload.bar_states), ["RT_CONFIRMED"]);
  assert.equal(payload.raw.parsed_message.action, "sell");
  assert.equal(payload.raw.debug[0].bs, "RT_CONFIRMED");

  socket.emitMessage(studyAlertFrame({
    action: "sell",
    price: 64131.92,
    time: 1783763820000,
    updateTime: 1783763881395
  }));
  await flush();
  assert.equal(pageMessages.length, 1);
});

test("private feed content script relays page-hook payloads to service worker", async () => {
  const dom = createDom();
  const { window } = dom;
  const sentPayloads = [];
  const statusMessages = [];

  window.chrome = {
    runtime: {
      lastError: null,
      getURL(path) {
        return `chrome-extension://test/${path}`;
      },
      sendMessage(message, callback) {
        if (message && message.type === "tradingview_alert") sentPayloads.push(message.payload);
        if (message && message.type === "content_status") statusMessages.push(message);
        if (callback) callback({ ok: true });
      }
    }
  };

  window.eval(readFileSync(CONTENT_SRC_PATH, "utf8"));
  const payload = {
    source_kind: "private_pricealerts_ws",
    event_id: "tv_price_alert:1",
    text: {
      channel: "pricealerts",
      content: { m: "alert_fired", p: { fire_id: 1 } }
    }
  };
  window.dispatchEvent(new window.MessageEvent("message", {
    source: window,
    data: {
      type: "optionx_tradingview_private_feed",
      payload
    }
  }));
  await flush();

  assert.ok(statusMessages.some((message) => message.status === "private_feed_hook_injected"));
  assert.equal(sentPayloads.length, 1);
  assert.deepEqual(sentPayloads[0], payload);
});
