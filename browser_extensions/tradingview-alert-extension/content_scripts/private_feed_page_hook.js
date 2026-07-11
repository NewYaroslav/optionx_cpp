"use strict";

(() => {
  const HOOK_FLAG = "__optionxTradingViewPrivateFeedHook";
  const MESSAGE_TYPE = "optionx_tradingview_private_feed";
  const SOURCE = "tradingview_extension";
  const SOURCE_KIND = "private_pricealerts_ws";
  const PRIVATE_FEED_HOST = "pushstream.tradingview.com";
  const PRIVATE_FEED_PATH = "/message-pipe-ws/private_feed";

  if (window[HOOK_FLAG]) return;
  window[HOOK_FLAG] = true;

  const NativeWebSocket = window.WebSocket;
  if (typeof NativeWebSocket !== "function") return;

  function fnv1a(value) {
    let hash = 0x811c9dc5;
    for (let index = 0; index < value.length; index += 1) {
      hash ^= value.charCodeAt(index);
      hash = Math.imul(hash, 0x01000193);
    }
    return (hash >>> 0).toString(16).padStart(8, "0");
  }

  function isPrivateFeedUrl(url) {
    try {
      const parsed = new URL(String(url), window.location.href);
      return parsed.hostname === PRIVATE_FEED_HOST &&
        parsed.pathname === PRIVATE_FEED_PATH;
    } catch (_) {
      return false;
    }
  }

  function parseJsonFrame(data) {
    if (typeof data !== "string") return null;
    const text = data.trim();
    if (!text || text[0] !== "{") return null;
    try {
      const parsed = JSON.parse(text);
      return parsed && typeof parsed === "object" ? parsed : null;
    } catch (_) {
      return null;
    }
  }

  function scalarToString(value) {
    if (value === null || value === undefined) return "";
    if (typeof value === "string") return value;
    if (typeof value === "number" || typeof value === "boolean") return String(value);
    return "";
  }

  function firstString(object, keys) {
    if (!object || typeof object !== "object") return "";
    for (const key of keys) {
      const value = scalarToString(object[key]).trim();
      if (value) return value;
    }
    return "";
  }

  function normalizeSymbol(symbol) {
    const text = scalarToString(symbol).trim();
    if (!text.startsWith("=")) return text;
    try {
      const parsed = JSON.parse(text.slice(1));
      return firstString(parsed, ["symbol", "tickerid", "ticker"]) || text;
    } catch (_) {
      return text;
    }
  }

  function buildPayload(frame) {
    const text = frame && frame.text;
    const content = text && text.content;
    const data = content && content.p;
    if (!text || text.channel !== "pricealerts") return null;
    if (!content || content.m !== "alert_fired") return null;
    if (!data || typeof data !== "object") return null;

    const fireId = firstString(data, ["fire_id"]);
    if (!fireId) return null;

    const alertId = firstString(data, ["alert_id", "id"]);
    const message = firstString(data, ["message", "description", "title"]);
    const symbol = normalizeSymbol(firstString(data, ["symbol", "tickerid", "ticker", "main_symbol"]));
    const fireTime = firstString(data, ["fire_time", "fired_at", "time", "timestamp"]);
    const eventId = `tv_price_alert:${fireId}`;
    const fingerprint = fnv1a(`${SOURCE_KIND}|${fireId}|${alertId}|${symbol}|${message}`);
    const observedAt = new Date().toISOString();

    return {
      version: 1,
      source: SOURCE,
      source_kind: SOURCE_KIND,
      method: "alert_fired",
      event_id: eventId,
      dedupe_key: eventId,
      fingerprint,
      fire_id: fireId,
      alert_id: alertId,
      action: "alert",
      symbol,
      message,
      observed_at: observedAt,
      time: fireTime || observedAt,
      text,
      raw: {
        frame_id: frame.id || null,
        channel: text.channel,
        content_id: content.id || "",
        rts: content._rts || null
      }
    };
  }

  function postPayload(payload) {
    window.postMessage({ type: MESSAGE_TYPE, payload }, window.location.origin || "*");
  }

  function attachSocket(socket) {
    socket.addEventListener("message", (event) => {
      const frame = parseJsonFrame(event.data);
      const payload = frame ? buildPayload(frame) : null;
      if (payload) postPayload(payload);
    });
  }

  function OptionXWebSocket(url, protocols) {
    const socket = protocols === undefined
      ? new NativeWebSocket(url)
      : new NativeWebSocket(url, protocols);
    if (isPrivateFeedUrl(url)) {
      attachSocket(socket);
    }
    return socket;
  }

  Object.setPrototypeOf(OptionXWebSocket, NativeWebSocket);
  OptionXWebSocket.prototype = NativeWebSocket.prototype;
  for (const key of ["CONNECTING", "OPEN", "CLOSING", "CLOSED"]) {
    if (Object.prototype.hasOwnProperty.call(NativeWebSocket, key)) {
      Object.defineProperty(OptionXWebSocket, key, {
        value: NativeWebSocket[key],
        enumerable: true,
        configurable: true
      });
    }
  }

  window.WebSocket = OptionXWebSocket;
})();
