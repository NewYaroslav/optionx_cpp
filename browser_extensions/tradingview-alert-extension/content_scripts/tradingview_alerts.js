"use strict";

(() => {
  const SOURCE = "tradingview_extension";
  const SOURCE_KIND = "alert_toast_dom";
  const DUPLICATE_WINDOW_MS = 5000;
  const MAX_PARENT_DEPTH = 8;

  const recentSignals = new Map();
  const handledRoots = new WeakSet();

  function normalizeText(value) {
    return String(value || "").replace(/\s+/g, " ").trim();
  }

  function nodeText(node) {
    return normalizeText(node && node.textContent ? node.textContent : "");
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
    if (!hasAlertTitle(root)) return false;
    return Boolean(findDescription(root));
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
    if (!message || message[0] !== "{") return null;
    try {
      const parsed = JSON.parse(message);
      return parsed && typeof parsed === "object" ? parsed : null;
    } catch {
      return null;
    }
  }

  function normalizeAction(message, parsed) {
    const rawAction = parsed && (parsed.action || parsed.side || parsed.signal);
    if (rawAction) return normalizeActionWord(rawAction);

    const match = String(message || "").match(/\b(BUY|SELL|LONG|SHORT|CALL|PUT)\b/i);
    if (!match) return "alert";
    return normalizeActionWord(match[1]);
  }

  function normalizeActionWord(value) {
    const normalized = String(value || "").trim().toLowerCase();
    if (normalized === "long" || normalized === "call") return "buy";
    if (normalized === "short" || normalized === "put") return "sell";
    if (normalized === "buy" || normalized === "sell") return normalized;
    return normalized || "alert";
  }

  function extractSymbol(title, message, parsed) {
    if (parsed && parsed.symbol) return normalizeSymbol(parsed.symbol);

    const fromTitle = String(title || "").match(/\bAlert\s+on\s+([A-Za-z0-9:._/-]+)/i);
    if (fromTitle) return normalizeSymbol(fromTitle[1]);

    const fromMessage = String(message || "").match(/\b([A-Z]{2,12}[A-Z0-9:_/-]*)\b/);
    return fromMessage ? normalizeSymbol(fromMessage[1]) : "";
  }

  function normalizeSymbol(value) {
    return String(value || "").trim().replace(/[^A-Za-z0-9:._/-]/g, "").toUpperCase();
  }

  function extractPrice(message, parsed) {
    if (parsed && parsed.price !== undefined) return toNumberOrString(parsed.price);
    const numbers = String(message || "").match(/[-+]?\d+(?:\.\d+)?/g);
    if (!numbers || numbers.length === 0) return null;
    return Number(numbers[numbers.length - 1]);
  }

  function toNumberOrString(value) {
    const numberValue = Number(value);
    return Number.isFinite(numberValue) ? numberValue : value;
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
    const symbol = extractSymbol(title, message, parsed);
    const action = normalizeAction(message, parsed);
    const dedupeKey = `${SOURCE_KIND}|${symbol}|${title}|${message}`;
    const hash = fnv1a(dedupeKey);
    const now = new Date();

    return {
      version: 1,
      source: SOURCE,
      source_kind: SOURCE_KIND,
      event_id: `tv_toast:${hash}:${Math.floor(now.getTime() / 1000)}`,
      dedupe_key: dedupeKey,
      symbol,
      action,
      price: extractPrice(message, parsed),
      time: now.toISOString(),
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
    chrome.runtime.sendMessage({ type: "tradingview_alert", payload }, () => {
      if (chrome.runtime.lastError) {
        console.debug("OptionX TradingView bridge send failed:", chrome.runtime.lastError.message);
      }
    });
  }

  function processRoot(root) {
    if (!root || handledRoots.has(root)) return;
    const payload = buildPayload(root);
    if (!payload.message) return;
    if (shouldSuppressDuplicate(payload.dedupe_key)) return;
    handledRoots.add(root);
    sendPayload(payload);
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
      for (const mutation of mutations) {
        for (const node of mutation.addedNodes) {
          inspectNodeSoon(node);
        }
      }
    });

    observer.observe(document.body, {
      childList: true,
      subtree: true
    });

    inspectNodeSoon(document.body);
    console.info("OptionX TradingView Alert Bridge observer active.");
  }

  startObserver();
})();
