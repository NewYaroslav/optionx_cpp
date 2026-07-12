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
  const statusCounts = new Map();

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

  function socketDetails(url) {
    try {
      const parsed = new URL(String(url), window.location.href);
      return {
        host: parsed.hostname,
        path: parsed.pathname,
        type: parsed.searchParams.get("type") || "",
        tradingview: parsed.hostname.endsWith(".tradingview.com") ||
          parsed.hostname === "tradingview.com"
      };
    } catch (_) {
      return {
        host: "",
        path: "",
        type: "",
        tradingview: false
      };
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
    if (typeof data !== "string") return [];
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

  function studyAlertKey(parsedMessage, messageText, barInfo, debugInfo) {
    const stateKey = scalarToString(debugInfo && debugInfo.state).trim();
    const explicitEventId = firstString(parsedMessage, ["event_id", "dedupe_key"]);
    const baseKey = explicitEventId ||
      [
        firstString(parsedMessage, ["tickerid", "symbol", "ticker"]),
        firstString(parsedMessage, ["signal_name", "strategy", "name"]),
        firstString(parsedMessage, ["action", "side", "direction"]),
        scalarToString(parsedMessage && parsedMessage.time),
        scalarToString(parsedMessage && parsedMessage.price),
        scalarToString(barInfo && barInfo.time),
        scalarToString(barInfo && barInfo.updateTime),
        messageText
      ].join("|");
    return [baseKey, stateKey].join("|");
  }

  function studyDebugInfo(parsedNs, barInfo) {
    const debug =
      parsedNs &&
      parsedNs.data &&
      Array.isArray(parsedNs.data.debug)
        ? parsedNs.data.debug
        : [];
    const states = [];
    for (const item of debug) {
      const state = firstString(item, ["bs", "bar_state", "state"]);
      if (state && !states.includes(state)) states.push(state);
    }

    const barIndex = Number(barInfo && barInfo.barIndex);
    const barTime = Number(barInfo && barInfo.time);
    let exact = null;
    let exactSource = "";
    if (Number.isFinite(barIndex)) {
      for (const item of debug) {
        if (Number(item && item.idx) === barIndex) {
          exact = item;
          exactSource = "debug_idx";
        }
      }
    }
    if (!exact && Number.isFinite(barTime)) {
      for (const item of debug) {
        const itemTime = Date.parse(scalarToString(item && item.t));
        if (Number.isFinite(itemTime) && itemTime === barTime) {
          exact = item;
          exactSource = "debug_time";
        }
      }
    }
    const exactState = firstString(exact, ["bs", "bar_state", "state"]);
    return {
      state: exactState,
      source: exactState ? exactSource : "",
      states,
      debug
    };
  }

  function buildStudyAlertPayloads(frame, state) {
    if (!frame || frame.m !== "du" || !Array.isArray(frame.p)) return [];
    const chartSession = scalarToString(frame.p[0]);
    const updates = frame.p[1];
    if (!updates || typeof updates !== "object") return [];

    const payloads = [];

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
        const debugInfo = studyDebugInfo(parsedNs, barInfo);
        const key = studyAlertKey(parsedMessage, messageText, barInfo, debugInfo);
        const isNew = boundedRemember(state, key);
        if (!isNew) continue;

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
          bar_state: debugInfo.state,
          bar_state_source: debugInfo.source,
          bar_states: debugInfo.states,
          message: comment,
          observed_at: observedAt,
          raw: {
            chart_session: chartSession,
            study_id: studyId,
            series_type: update && update.t ? update.t : "",
            barInfo,
            alert_message_text: messageText,
            parsed_message: parsedMessage,
            debug: debugInfo.debug,
            bar_state: debugInfo.state,
            bar_state_source: debugInfo.source,
            bar_states: debugInfo.states
          }
        });
      }
    }
    return payloads;
  }

  function postPayload(payload) {
    window.postMessage({ type: MESSAGE_TYPE, payload }, window.location.origin || "*");
  }

  function postStatus(status, details = {}) {
    window.postMessage({ type: MESSAGE_TYPE, status, details }, window.location.origin || "*");
  }

  function postStatusLimited(status, details = {}, limit = 5) {
    const count = statusCounts.get(status) || 0;
    if (count >= limit) return;
    statusCounts.set(status, count + 1);
    postStatus(status, details);
  }

  function frameMethodSummary(frames) {
    const counts = {};
    for (const frame of frames) {
      const method = scalarToString(frame && frame.m) || "<none>";
      counts[method] = (counts[method] || 0) + 1;
    }
    return Object.entries(counts)
      .map(([method, count]) => `${method}:${count}`)
      .join(",");
  }

  function countStudyAlertMessages(frame) {
    if (!frame || frame.m !== "du" || !Array.isArray(frame.p)) return 0;
    const updates = frame.p[1];
    if (!updates || typeof updates !== "object") return 0;
    let total = 0;
    for (const update of Object.values(updates)) {
      const nsData = update && update.ns && scalarToString(update.ns.d);
      if (!nsData) continue;
      const parsedNs = parseJsonObject(nsData);
      const alertMessages =
        parsedNs &&
        parsedNs.data &&
        Array.isArray(parsedNs.data.alertMessages)
          ? parsedNs.data.alertMessages
          : [];
      total += alertMessages.length;
    }
    return total;
  }

  setTimeout(() => {
    postStatusLimited("page_hook_installed", { url: location.href }, 3);
  }, 0);

  function attachSocket(socket, kind, url) {
    const state = {
      studyKeys: new Set(),
      studyKeyOrder: []
    };
    postStatus(
      kind === "chart_socket" ? "chart_socket_hook_attached" : "private_feed_hook_attached",
      socketDetails(url));
    socket.addEventListener("message", (event) => {
      const frames = parseTradingViewFrames(event.data);
      if (kind === "chart_socket" && frames.length > 0) {
        postStatusLimited(
          "chart_socket_frames_seen",
          { count: frames.length, methods: frameMethodSummary(frames) },
          5);
      }
      for (const frame of frames) {
        if (kind === "private_feed") {
          const payload = buildPriceAlertPayload(frame);
          if (payload) postPayload(payload);
          continue;
        }
        if (kind === "chart_socket") {
          if (frame && frame.m === "du") {
            postStatusLimited(
              "chart_socket_du_seen",
              { keys: Object.keys(frame.p && frame.p[1] || {}).slice(0, 8).join(",") },
              5);
          }
          const alertMessageCount = countStudyAlertMessages(frame);
          if (alertMessageCount > 0) {
            postStatusLimited(
              "chart_socket_study_alerts_seen",
              { count: alertMessageCount },
              10);
          }
          const payloads = buildStudyAlertPayloads(frame, state);
          if (payloads.length > 0) {
            postStatusLimited(
              "chart_socket_study_alerts_forwarded",
              {
                count: payloads.length,
                action: payloads[0].action || "",
                symbol: payloads[0].symbol || ""
              },
              10);
          }
          for (const payload of payloads) {
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
    const details = socketDetails(url);
    if (details.tradingview) {
      postStatusLimited(kind ? "tradingview_ws_matched" : "tradingview_ws_ignored", details, 10);
    }
    if (kind) {
      attachSocket(socket, kind, url);
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
