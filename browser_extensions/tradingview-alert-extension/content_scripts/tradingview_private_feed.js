"use strict";

(() => {
  const PAGE_HOOK = "content_scripts/private_feed_page_hook.js";
  const MESSAGE_TYPE = "optionx_tradingview_private_feed";
  const PRICE_ALERT_SOURCE_KIND = "private_pricealerts_ws";
  const CHART_STUDY_SOURCE_KIND = "private_chart_study_alert_messages";
  let extensionContextInvalidated = false;

  function isObject(value) {
    return value !== null && typeof value === "object" && !Array.isArray(value);
  }

  function scalarToString(value) {
    if (value === null || value === undefined) return "";
    return String(value);
  }

  function hasPrefix(value, prefix) {
    return scalarToString(value).startsWith(prefix);
  }

  function isValidPriceAlertPayload(payload) {
    if (payload.source_kind !== PRICE_ALERT_SOURCE_KIND) return false;
    if (payload.source !== "tradingview_extension") return false;
    if (payload.method !== "alert_fired") return false;
    if (payload.action !== "alert") return false;

    const text = payload.text;
    const content = text && text.content;
    const data = content && content.p;
    if (!isObject(text) || text.channel !== "pricealerts") return false;
    if (!isObject(content) || content.m !== "alert_fired") return false;
    if (!isObject(data)) return false;

    const fireId = scalarToString(data.fire_id || payload.fire_id);
    if (!fireId) return false;
    if (payload.fire_id && scalarToString(payload.fire_id) !== fireId) return false;

    const expectedEventId = `tv_price_alert:${fireId}`;
    if (payload.event_id !== expectedEventId) return false;
    if (payload.dedupe_key && payload.dedupe_key !== expectedEventId) return false;
    return true;
  }

  function isValidChartStudyPayload(payload) {
    if (payload.source_kind !== CHART_STUDY_SOURCE_KIND) return false;
    if (payload.source !== "tradingview_extension") return false;
    if (payload.method !== "du.alertMessages") return false;
    if (!payload.event_id || payload.dedupe_key !== payload.event_id) return false;
    if (!payload.chart_session || !payload.study_id) return false;
    if (!payload.symbol && !payload.tickerid) return false;
    if (!payload.action) return false;

    const raw = payload.raw;
    if (!isObject(raw)) return false;
    if (!isObject(raw.barInfo)) return false;
    if (!raw.alert_message_text) return false;
    if (!isObject(raw.parsed_message)) return false;
    if (!hasPrefix(payload.event_id, "tv_study_alert:") &&
        payload.event_id !== scalarToString(raw.parsed_message.event_id) &&
        payload.event_id !== scalarToString(raw.parsed_message.dedupe_key)) {
      return false;
    }
    if (scalarToString(raw.parsed_message.action || raw.parsed_message.side || raw.parsed_message.direction) !==
        scalarToString(payload.action)) {
      return false;
    }
    return true;
  }

  function isValidHookPayload(payload) {
    if (!isObject(payload)) return false;
    if (payload.source_kind === PRICE_ALERT_SOURCE_KIND) {
      return isValidPriceAlertPayload(payload);
    }
    if (payload.source_kind === CHART_STUDY_SOURCE_KIND) {
      return isValidChartStudyPayload(payload);
    }
    return false;
  }

  function isExtensionContextInvalidated(error) {
    const message = error && error.message ? error.message : String(error || "");
    return /extension context invalidated/i.test(message);
  }

  function deactivateInvalidatedContext(error) {
    if (!isExtensionContextInvalidated(error)) return false;
    extensionContextInvalidated = true;
    window.removeEventListener("message", handlePageMessage);
    console.debug("OptionX bridge: private feed context invalidated; reload the TradingView tab.");
    return true;
  }

  function sendStatus(status, details = {}) {
    if (extensionContextInvalidated) return;
    try {
      chrome.runtime.sendMessage({ type: "content_status", status, details }, () => {
        if (chrome.runtime.lastError) {
          console.debug("OptionX TradingView private feed status failed:", chrome.runtime.lastError.message);
        }
      });
    } catch (error) {
      if (!deactivateInvalidatedContext(error)) throw error;
    }
  }

  function sendPayload(payload) {
    if (extensionContextInvalidated) return;
    try {
      chrome.runtime.sendMessage({ type: "tradingview_alert", payload }, () => {
        if (chrome.runtime.lastError) {
          console.debug("OptionX TradingView private feed send failed:", chrome.runtime.lastError.message);
        }
      });
    } catch (error) {
      if (!deactivateInvalidatedContext(error)) throw error;
    }
  }

  function handlePageMessage(event) {
    if (event.source !== window) return;
    const data = event.data;
    if (!data || typeof data !== "object" || data.type !== MESSAGE_TYPE) return;
    if (data.status) {
      sendStatus(data.status, data.details || {});
      return;
    }
    if (!isValidHookPayload(data.payload)) {
      sendStatus("private_feed_payload_rejected", {
        source_kind: data.payload && data.payload.source_kind ? data.payload.source_kind : ""
      });
      return;
    }
    sendPayload(data.payload);
  }

  function injectPageHook() {
    const root = document.documentElement || document.head || document.body;
    if (!root) {
      setTimeout(injectPageHook, 25);
      return;
    }

    const script = document.createElement("script");
    script.src = chrome.runtime.getURL(PAGE_HOOK);
    script.async = false;
    script.onload = () => script.remove();
    script.onerror = () => {
      script.remove();
      sendStatus("private_feed_hook_error", { url: location.href });
    };
    root.appendChild(script);
    sendStatus("private_feed_hook_injected", { url: location.href });
  }

  window.addEventListener("message", handlePageMessage);
  injectPageHook();
})();
