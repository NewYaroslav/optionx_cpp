"use strict";

(() => {
  const SOURCE = "tradingview_extension";
  const SOURCE_KIND = "alert_toast_dom";
  const DUPLICATE_WINDOW_MS = 5000;
  const MAX_PARENT_DEPTH = 8;

  const recentSignals = new Map();
  const handledRoots = new WeakSet();
  let activeObserver = null;
  let extensionContextInvalidated = false;

  function normalizeText(value) {
    return String(value || "").replace(/\s+/g, " ").trim();
  }

  function nodeText(node) {
    return normalizeText(node && node.textContent ? node.textContent : "");
  }

  function isExtensionContextInvalidated(error) {
    const message = error && error.message ? error.message : String(error || "");
    return /extension context invalidated/i.test(message);
  }

  function deactivateInvalidatedContext(error) {
    if (!isExtensionContextInvalidated(error)) return false;
    extensionContextInvalidated = true;
    if (activeObserver) {
      activeObserver.disconnect();
      activeObserver = null;
    }
    console.debug("OptionX bridge: content script context invalidated; reload the TradingView tab.");
    return true;
  }

  function visibleInnerLines(node) {
    const value = node && node.innerText ? node.innerText : nodeText(node);
    return String(value || "")
      .split(/\n+/)
      .map(normalizeText)
      .filter(Boolean);
  }

  function hasAlertTitle(node) {
    return /\bAlert\s+on\b/i.test(nodeText(node));
  }

  function isLikelyToastContainer(node) {
    if (!node || node.nodeType !== Node.ELEMENT_NODE) return false;
    const className = String(node.className || "");
    return /\balert\b/i.test(String(node.getAttribute("role") || "")) ||
      Boolean(node.getAttribute("aria-live")) ||
      /toast|notification|notice|popup|alert/i.test(className);
  }

  function messageLooksLikeAlert(message) {
    const parsed = parseJsonMessage(message);
    if (parsed && typeof parsed === "object") return true;
    const symbol = OptionXParser.extractSymbol("", message, parsed);
    const direction = OptionXParser.extractDirection(message);
    const action = OptionXParser.normalizeAction(message, parsed);
    return Boolean(symbol && (direction || action === "buy" || action === "sell"));
  }

  function findDescription(root) {
    const selectors = [
      'span[class*="description"]',
      'div[class*="description"]',
      '[data-name*="description"]'
    ];

    for (const selector of selectors) {
      for (const element of root.querySelectorAll(selector)) {
        const value = nodeText(element);
        if (value && !/^Alert\s+on\b/i.test(value)) return value;
      }
    }

    const lines = visibleInnerLines(root);
    const titleIndex = lines.findIndex((line) => /^Alert\s+on\b/i.test(line));
    if (titleIndex >= 0) {
      for (let index = titleIndex + 1; index < lines.length; index += 1) {
        const line = lines[index];
        if (!/^Edit alert$/i.test(line) && !/^\d{1,2}:\d{2}(:\d{2})?$/.test(line)) {
          return line;
        }
      }
    }

    return "";
  }

  function findTitle(root) {
    const lines = visibleInnerLines(root);
    return lines.find((line) => /^Alert\s+on\b/i.test(line)) || "";
  }

  function looksLikeAlertToast(root) {
    if (!root || root.nodeType !== Node.ELEMENT_NODE) return false;
    const description = findDescription(root);
    if (!description) return false;
    if (hasAlertTitle(root)) return true;
    return isLikelyToastContainer(root) && messageLooksLikeAlert(description);
  }

  function climbToAlertToast(element) {
    let current = element;
    for (let depth = 0; current && depth < MAX_PARENT_DEPTH; depth += 1) {
      if (current === document.body || current === document.documentElement) return null;
      if (looksLikeAlertToast(current)) return current;
      current = current.parentElement;
    }
    return null;
  }

  function collectAlertRoots(node) {
    const roots = new Set();
    if (!node || node.nodeType !== Node.ELEMENT_NODE) return roots;

    const element = node;
    if (
      element !== document.body &&
      element !== document.documentElement &&
      looksLikeAlertToast(element)
    ) {
      roots.add(element);
    }

    const candidates = element.querySelectorAll(
      '[class*="description"], [class*="toast"], [role="alert"], [aria-live]'
    );
    for (const candidate of candidates) {
      const root = climbToAlertToast(candidate);
      if (root) roots.add(root);
    }

    return roots;
  }

  function parseJsonMessage(message) {
    const trimmed = String(message || "").trimStart();
    if (!trimmed || trimmed[0] !== "{") return null;
    try {
      const parsed = JSON.parse(trimmed);
      return parsed && typeof parsed === "object" ? parsed : null;
    } catch (_) {
      return null;
    }
  }

  function fnv1a(value) {
    let hash = 0x811c9dc5;
    for (let index = 0; index < value.length; index += 1) {
      hash ^= value.charCodeAt(index);
      hash = Math.imul(hash, 0x01000193);
    }
    return (hash >>> 0).toString(16).padStart(8, "0");
  }

  function shouldSuppressDuplicate(key) {
    const now = Date.now();
    for (const [candidate, timestamp] of recentSignals) {
      if (now - timestamp > DUPLICATE_WINDOW_MS) recentSignals.delete(candidate);
    }

    const previous = recentSignals.get(key);
    if (previous && now - previous < DUPLICATE_WINDOW_MS) return true;

    recentSignals.set(key, now);
    return false;
  }

  function buildPayload(root) {
    const title = findTitle(root);
    const message = findDescription(root);
    const parsed = parseJsonMessage(message);
    const symbol = OptionXParser.extractSymbol(title, message, parsed);
    const action = OptionXParser.normalizeAction(message, parsed);
    const direction = OptionXParser.extractDirection(message);
    const fingerprintInput = `${SOURCE_KIND}|${symbol}|${title}|${message}`;
    const fingerprint = fnv1a(fingerprintInput);
    const observedAt = new Date().toISOString();
    const eventId = OptionXParser.makeEventId(parsed, fingerprint);
    const { price, trigger_value, trigger_unit } =
      OptionXParser.resolveTriggerMetadata(message, parsed, direction);

    return {
      version: 1,
      source: SOURCE,
      source_kind: SOURCE_KIND,
      event_id: eventId,
      fingerprint,
      symbol,
      action,
      raw_action: OptionXParser.extractRawAction(message, parsed),
      direction,
      raw_direction: OptionXParser.extractRawDirection(message),
      price,
      trigger_value,
      trigger_unit,
      observed_at: observedAt,
      time: observedAt,
      title,
      message,
      raw: {
        title,
        description: message,
        parsed_message: parsed
      }
    };
  }

  function sendPayload(payload) {
    if (extensionContextInvalidated) return;
    try {
      chrome.runtime.sendMessage({ type: "tradingview_alert", payload }, () => {
        if (chrome.runtime.lastError) {
          console.debug("OptionX TradingView bridge send failed:", chrome.runtime.lastError.message);
        }
      });
    } catch (error) {
      if (!deactivateInvalidatedContext(error)) throw error;
    }
  }

  function sendStatus(status, details = {}) {
    if (extensionContextInvalidated) return;
    try {
      chrome.runtime.sendMessage({ type: "content_status", status, details }, () => {
        if (chrome.runtime.lastError) {
          console.debug("OptionX TradingView bridge status failed:", chrome.runtime.lastError.message);
        }
      });
    } catch (error) {
      if (!deactivateInvalidatedContext(error)) throw error;
    }
  }

  function countAlertDescriptions(root) {
    const candidates = root.querySelectorAll('[class*="description"]');
    let count = 0;
    for (const cand of candidates) {
      const text = (cand.textContent || "").trim();
      if (text && text.length > 3 && !/edit alert/i.test(text)) count++;
    }
    return count;
  }

  function processRoot(root) {
    if (extensionContextInvalidated) return;
    if (!root || handledRoots.has(root)) return;
    if (countAlertDescriptions(root) > 1) {
      console.debug("OptionX bridge: skipping root with multiple descriptions to avoid merged payload");
      handledRoots.add(root);
      return;
    }
    try {
      const payload = buildPayload(root);
      if (!payload.message) return;
      if (shouldSuppressDuplicate(payload.fingerprint)) return;
      handledRoots.add(root);
      sendPayload(payload);
    } catch (error) {
      if (!deactivateInvalidatedContext(error)) throw error;
    }
  }

  function inspectNode(node) {
    const roots = collectAlertRoots(node);
    roots.forEach(processRoot);
  }

  function inspectNodeSoon(node) {
    setTimeout(() => inspectNode(node), 50);
    setTimeout(() => inspectNode(node), 300);
  }

  function startObserver() {
    if (!document.body) {
      setTimeout(startObserver, 250);
      return;
    }

    const observer = new MutationObserver((mutations) => {
      if (extensionContextInvalidated) return;
      for (const mutation of mutations) {
        if (mutation.addedNodes && mutation.addedNodes.length) {
          for (const node of mutation.addedNodes) {
            inspectNodeSoon(node);
          }
        }
        if (mutation.type === "characterData" && mutation.target.parentElement) {
          inspectNodeSoon(mutation.target.parentElement);
        } else if (mutation.type === "attributes" && mutation.target) {
          inspectNodeSoon(mutation.target);
        }
      }
    });

    observer.observe(document.body, {
      childList: true,
      subtree: true,
      characterData: true,
      attributes: true,
      attributeFilter: ["class"]
    });
    activeObserver = observer;

    inspectNodeSoon(document.body);
    sendStatus("observer_active", { url: location.href });
    console.info("OptionX TradingView Alert Bridge observer active.");
  }

  startObserver();
})();
