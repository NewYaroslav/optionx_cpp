"use strict";

(() => {
  const PAGE_HOOK = "content_scripts/private_feed_page_hook.js";
  const MESSAGE_TYPE = "optionx_tradingview_private_feed";
  let extensionContextInvalidated = false;

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
    if (!data.payload || typeof data.payload !== "object") return;
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
