"use strict";

(() => {
  const HOOK_FLAG = "__optionxTradingViewPrivateFeedHook";
  const MESSAGE_TYPE = "optionx_tradingview_private_feed";
  const SOURCE = "tradingview_extension";
  const PRICE_ALERT_SOURCE_KIND = "private_pricealerts_ws";
  const CHART_STUDY_SOURCE_KIND = "private_chart_study_alert_messages";
  const PRIVATE_FEED_HOST = "pushstream.tradingview.com";
  const PRIVATE_FEED_PATH = "/message-pipe-ws/private_feed";
  const CHART_SOCKET_HOST = "data.tradingview.com";
  const CHART_SOCKET_PATH = "/socket.io/websocket";
  const MAX_STUDY_KEYS = 2048;

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

  function socketKind(url) {
    try {
      const parsed = new URL(String(url), window.location.href);
      if (parsed.hostname === PRIVATE_FEED_HOST &&
          parsed.pathname === PRIVATE_FEED_PATH) {
        return "private_feed";
      }
      if (parsed.hostname === CHART_SOCKET_HOST &&
          parsed.pathname === CHART_SOCKET_PATH) {
        return "chart_socket";
      }
      return "";
    } catch (_) {
      return "";
    }
  }

  function parseJsonObject(text) {
    try {
      const parsed = JSON.parse(text);
      return parsed && typeof parsed === "object" ? parsed : null;
    } catch (_) {
      return null;
    }
  }

  function parseTradingViewFrames(data) {
    if (typeof data !== "string") return null;
    const text = data.trim();
    if (!text) return [];
    if (text[0] === "{") {
      const parsed = parseJsonObject(text);
      return parsed ? [parsed] : [];
    }

    const frames = [];
    let index = 0;
    while (index < text.length) {
      const marker = text.indexOf("~m~", index);
      if (marker < 0) break;
      const lengthStart = marker + 3;
      const lengthEnd = text.indexOf("~m~", lengthStart);
      if (lengthEnd < 0) break;
      const length = Number(text.slice(lengthStart, lengthEnd));
      const payloadStart = lengthEnd + 3;
      if (!Number.isFinite(length) || length < 0) {
        index = payloadStart;
        continue;
      }
      const payload = text.slice(payloadStart, payloadStart + length);
      if (payload.length < length) break;
      if (payload && payload[0] === "{") {
        const parsed = parseJsonObject(payload);
        if (parsed) frames.push(parsed);
      }
      index = payloadStart + Math.max(length, 1);
    }
    return frames;
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

  function buildPriceAlertPayload(frame) {
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
    const fingerprint = fnv1a(`${PRICE_ALERT_SOURCE_KIND}|${fireId}|${alertId}|${symbol}|${message}`);
    const observedAt = new Date().toISOString();

    return {
      version: 1,
      source: SOURCE,
      source_kind: PRICE_ALERT_SOURCE_KIND,
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

  function numberOrNull(value) {
    return typeof value === "number" && Number.isFinite(value) ? value : null;
  }

  function boundedRemember(state, key) {
    if (state.studyKeys.has(key)) return false;
    state.studyKeys.add(key);
    state.studyKeyOrder.push(key);
    while (state.studyKeyOrder.length > MAX_STUDY_KEYS) {
      const oldKey = state.studyKeyOrder.shift();
      state.studyKeys.delete(oldKey);
    }
    return true;
  }

  function studyAlertKey(parsedMessage, messageText, barInfo) {
    const explicitEventId = firstString(parsedMessage, ["event_id", "dedupe_key"]);
    if (explicitEventId) return explicitEventId;
    return [
      firstString(parsedMessage, ["tickerid", "symbol", "ticker"]),
      firstString(parsedMessage, ["signal_name", "strategy", "name"]),
      firstString(parsedMessage, ["action", "side", "direction"]),
      scalarToString(parsedMessage && parsedMessage.time),
      scalarToString(parsedMessage && parsedMessage.price),
      scalarToString(barInfo && barInfo.time),
      scalarToString(barInfo && barInfo.updateTime),
      messageText
    ].join("|");
  }

  function buildStudyAlertPayloads(frame, state) {
    if (!frame || frame.m !== "du" || !Array.isArray(frame.p)) return [];
    const chartSession = scalarToString(frame.p[0]);
    const updates = frame.p[1];
    if (!updates || typeof updates !== "object") return [];

    const payloads = [];
    let sawStudyAlert = false;
    const seedOnly = !state.studySeeded;

    for (const [studyId, update] of Object.entries(updates)) {
      const nsData = update && update.ns && scalarToString(update.ns.d);
      if (!nsData) continue;

      const parsedNs = parseJsonObject(nsData);
      const alertMessages =
        parsedNs &&
        parsedNs.data &&
        Array.isArray(parsedNs.data.alertMessages)
          ? parsedNs.data.alertMessages
          : [];
      if (alertMessages.length === 0) continue;

      for (const alertMessage of alertMessages) {
        const messageText = scalarToString(alertMessage && alertMessage.msg).trim();
        if (!messageText) continue;
        const parsedMessage = parseJsonObject(messageText) || {};
        const barInfo =
          alertMessage && alertMessage.barInfo && typeof alertMessage.barInfo === "object"
            ? alertMessage.barInfo
            : {};
        const key = studyAlertKey(parsedMessage, messageText, barInfo);
        const isNew = boundedRemember(state, key);
        sawStudyAlert = true;
        if (seedOnly || !isNew) continue;

        const eventId = firstString(parsedMessage, ["event_id", "dedupe_key"]) ||
          `tv_study_alert:${fnv1a(key)}`;
        const fingerprint = fnv1a(`${CHART_STUDY_SOURCE_KIND}|${key}`);
        const symbol = firstString(parsedMessage, ["symbol", "tickerid", "ticker"]);
        const tickerid = firstString(parsedMessage, ["tickerid"]);
        const signalName = firstString(parsedMessage, ["signal_name", "strategy", "name"]);
        const action = firstString(parsedMessage, ["action", "side", "direction"]);
        const observedAt = new Date().toISOString();
        const comment = firstString(parsedMessage, ["message", "comment", "description"]) ||
          [signalName, action, symbol].filter(Boolean).join(" ");

        payloads.push({
          version: 1,
          source: SOURCE,
          source_kind: CHART_STUDY_SOURCE_KIND,
          method: "du.alertMessages",
          event_id: eventId,
          dedupe_key: eventId,
          fingerprint,
          chart_session: chartSession,
          study_id: studyId,
          signal_name: signalName,
          action,
          symbol,
          tickerid,
          price: numberOrNull(parsedMessage.price) ?? numberOrNull(barInfo.close),
          time: parsedMessage.time ?? barInfo.time ?? observedAt,
          bar_time: barInfo.time ?? null,
          update_time: barInfo.updateTime ?? null,
          message: comment,
          observed_at: observedAt,
          raw: {
            chart_session: chartSession,
            study_id: studyId,
            series_type: update && update.t ? update.t : "",
            barInfo,
            alert_message_text: messageText,
            parsed_message: parsedMessage
          }
        });
      }
    }

    if (sawStudyAlert) {
      state.studySeeded = true;
    }
    return payloads;
  }

  function postPayload(payload) {
    window.postMessage({ type: MESSAGE_TYPE, payload }, window.location.origin || "*");
  }

  function attachSocket(socket, kind) {
    const state = {
      studySeeded: kind !== "chart_socket",
      studyKeys: new Set(),
      studyKeyOrder: []
    };
    socket.addEventListener("message", (event) => {
      const frames = parseTradingViewFrames(event.data);
      for (const frame of frames) {
        if (kind === "private_feed") {
          const payload = buildPriceAlertPayload(frame);
          if (payload) postPayload(payload);
          continue;
        }
        if (kind === "chart_socket") {
          for (const payload of buildStudyAlertPayloads(frame, state)) {
            postPayload(payload);
          }
        }
      }
    });
  }

  function OptionXWebSocket(url, protocols) {
    const socket = protocols === undefined
      ? new NativeWebSocket(url)
      : new NativeWebSocket(url, protocols);
    const kind = socketKind(url);
    if (kind) {
      attachSocket(socket, kind);
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
