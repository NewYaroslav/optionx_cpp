(function (root, factory) {
  if (typeof module !== "undefined" && module.exports) {
    module.exports = factory();
  } else {
    root.OptionXParser = factory();
  }
})(typeof self !== "undefined" ? self : this, function () {
  "use strict";

  const ACTION_WORDS = { buy: "buy", sell: "sell", long: "buy", short: "sell", call: "buy", put: "sell" };

  const SYMBOL_BLACKLIST = new Set([
    "BUY", "SELL", "LONG", "SHORT", "CALL", "PUT",
    "ALERT", "PRICE", "CROSSING", "CROSSED", "CROSSES",
    "RSI", "MACD", "SMA", "EMA", "ADX", "ATR", "CCI",
    "PSAR", "STDEV", "TRIX", "WILLR", "MFI", "OBV", "TV"
  ]);

  const TRIGGER_PATTERNS = [
    { pattern: /\b(?:crossing|crossed|crosses)\s+up\b/i, value: "up" },
    { pattern: /\b(?:crossing|crossed|crosses)\s+down\b/i, value: "down" },
    { pattern: /\bmoving\s+up\s+[-+]?\d+(?:\.\d+)?\s*%/i, value: "moving_up_pct" },
    { pattern: /\bmoving\s+up\b/i, value: "moving_up" },
    { pattern: /\bmoving\s+down\s+[-+]?\d+(?:\.\d+)?\s*%/i, value: "moving_down_pct" },
    { pattern: /\bmoving\s+down\b/i, value: "moving_down" },
    { pattern: /\b(?:entering\s+channel)\b/i, value: "entering_channel" },
    { pattern: /\b(?:exiting\s+channel)\b/i, value: "exiting_channel" },
    { pattern: /\b(?:inside\s+channel|in\s+channel)\b/i, value: "inside_channel" },
    { pattern: /\b(?:outside\s+channel|out\s+of\s+channel)\b/i, value: "outside_channel" },
    { pattern: /\b(?:greater\s+than|above)\b/i, value: "above" },
    { pattern: /\b(?:less\s+than|below)\b/i, value: "below" },
    { pattern: /\b(?:crossing|crossed|crosses)\b/i, value: "cross" }
  ];

  const TRIGGER_PREFIX = "(?:Cross(?:ing|ed|es)\\b|Moving\\s+Up(?:\\s*%?)?|Moving\\s+Down(?:\\s*%?)?|Greater\\s+Than|Less\\s+Than|Entering\\s+Channel|Exiting\\s+Channel|Inside\\s+Channel|Outside\\s+Channel)";

  function normalizeActionWord(word) {
    if (!word) return "alert";
    const w = String(word).toLowerCase().trim();
    return ACTION_WORDS[w] || "alert";
  }

  function normalizeSymbol(s) {
    return String(s || "").trim().toUpperCase();
  }

  function normalizeAction(message, parsed) {
    if (parsed && typeof parsed === "object") {
      const rawAction = parsed.action || parsed.side || parsed.signal;
      if (typeof rawAction === "string") {
        const normalized = normalizeActionWord(rawAction);
        if (normalized === "buy" || normalized === "sell") return normalized;
      }
    }
    const commandMatch = String(message || "").match(/^\s*(BUY|SELL|LONG|SHORT|CALL|PUT)\b/i);
    if (commandMatch) {
      const normalized = normalizeActionWord(commandMatch[1]);
      if (normalized === "buy" || normalized === "sell") return normalized;
    }
    return "alert";
  }

  function extractRawAction(message, parsed) {
    if (parsed && typeof parsed === "object") {
      const rawAction = parsed.action || parsed.side || parsed.signal;
      if (typeof rawAction === "string" && rawAction.trim()) return rawAction.trim();
    }
    const commandMatch = String(message || "").match(/^\s*(BUY|SELL|LONG|SHORT|CALL|PUT)\b/i);
    return commandMatch ? commandMatch[1].toUpperCase() : null;
  }

  function extractRawDirection(message) {
    const text = String(message || "");
    for (const { pattern } of TRIGGER_PATTERNS) {
      const match = text.match(pattern);
      if (match) return match[0];
    }
    return null;
  }

  function parseJsonMessageSafe(text) {
    const trimmed = String(text || "").trimStart();
    if (!trimmed || trimmed[0] !== "{") return null;
    try {
      return JSON.parse(trimmed);
    } catch (_) {
      return null;
    }
  }

  function extractSymbol(title, message, parsed) {
    if (parsed && typeof parsed.symbol === "string" && parsed.symbol.trim()) {
      return normalizeSymbol(parsed.symbol);
    }
    const re = new RegExp("\\b([A-Z][A-Z0-9:._/-]{1,15})\\s+" + TRIGGER_PREFIX, "i");
    const triggerLike = String(message || "").match(re);
    if (triggerLike) {
      const candidate = normalizeSymbol(triggerLike[1]);
      if (!SYMBOL_BLACKLIST.has(candidate)) return candidate;
    }
    // "Alert on <X>" from title. Defense-in-depth anchor: \S+ stops at the
    // first whitespace, and ^\s* prevents mid-text matches like
    // "...prefix Alert on EURUSD..." from extracting a symbol. TradingView
    // tickers never contain whitespace, so \S+ is sufficient — we do not
    // try to split glued tickers (e.g. "EURUSDBUY EURUSD") because the
    // title/description DOM is separated by <br>/nodes, never concatenated.
    const fromTitle = String(title || "").match(/^\s*Alert\s+on\s+(\S+)/i);
    if (fromTitle) {
      const candidate = normalizeSymbol(fromTitle[1]);
      if (!SYMBOL_BLACKLIST.has(candidate)) return candidate;
      // fall through to token loop instead of returning blacklisted candidate
    }
    const tokens = String(message || "").match(/\b([A-Z][A-Z0-9:._/-]{1,15})\b/g) || [];
    for (const token of tokens) {
      if (!SYMBOL_BLACKLIST.has(token)) return normalizeSymbol(token);
    }
    return "";
  }

  function extractFirstNumber(message, parsed) {
    if (parsed && typeof parsed === "object") {
      if (typeof parsed.price === "number" && Number.isFinite(parsed.price)) return parsed.price;
      if (typeof parsed.price === "string") {
        const n = parseFloat(parsed.price);
        if (Number.isFinite(n)) return n;
      }
    }
    const numbers = String(message || "").match(/[-+]?\d+(?:\.\d+)?/g);
    if (!numbers) return null;
    const first = parseFloat(numbers[0]);
    return Number.isFinite(first) ? first : null;
  }

  function extractPrice(message, parsed) {
    return extractFirstNumber(message, parsed);
  }

  // Anchored extractor for pct-based triggers only. Looks for the number AFTER
  // "Moving Up" or "Moving Down", so digits inside the symbol (e.g. US100) are
  // never misassigned as the trigger value.
  function extractPctTriggerValue(message) {
    const match = String(message || "").match(
      /\bmoving\s+(?:up|down)\s+([-+]?\d+(?:\.\d+)?)\s*%/i
    );
    if (!match) return null;
    const value = parseFloat(match[1]);
    return Number.isFinite(value) ? value : null;
  }

  function resolveTriggerMetadata(message, parsed, direction) {
    const isPctTrigger = typeof direction === "string" && /_pct$/.test(direction);

    if (isPctTrigger) {
      // Anchored extraction: take the number right after "Moving Up/Down" so
      // digits inside the symbol (e.g. US100, NAS100, SPX500) are not
      // misassigned as the trigger value.
      return { price: null, trigger_value: extractPctTriggerValue(message), trigger_unit: "percent" };
    }
    return { price: extractFirstNumber(message, parsed), trigger_value: null, trigger_unit: null };
  }

  function extractDirection(message) {
    const text = String(message || "");
    for (const { pattern, value } of TRIGGER_PATTERNS) {
      if (pattern.test(text)) return value;
    }
    return null;
  }

  // 4-level fallback resolution for event_id:
  //   1. parsed.event_id      - canonical, use as-is
  //   2. parsed.fire_id       - prefix tv_fire:
  //   3. parsed.alert_id + time - prefix tv_alert:ALERT_ID:TIME (alert_id alone is NOT unique per fire)
  //   4. fingerprint fallback - prefix tv_toast:
  // alert_id is the TradingView alert CONFIGURATION id, which stays the same across
  // multiple fires of the same alert. Using alert_id alone as event_id would let the
  // bridge treat every subsequent fire as a duplicate.
  function makeEventId(parsed, fingerprint) {
    if (!parsed || typeof parsed !== "object") {
      return `tv_toast:${fingerprint}`;
    }
    if (typeof parsed.event_id === "string" && parsed.event_id.trim()) {
      return parsed.event_id.trim();
    }
    if (parsed.fire_id !== undefined && parsed.fire_id !== null) {
      return `tv_fire:${parsed.fire_id}`;
    }
    if (parsed.alert_id !== undefined && parsed.alert_id !== null) {
      const time =
        parsed.fire_time !== undefined && parsed.fire_time !== null
          ? parsed.fire_time
          : parsed.bar_time !== undefined && parsed.bar_time !== null
          ? parsed.bar_time
          : parsed.time !== undefined && parsed.time !== null
          ? parsed.time
          : parsed.timenow !== undefined && parsed.timenow !== null
          ? parsed.timenow
          : null;
      if (time !== null) return `tv_alert:${parsed.alert_id}:${time}`;
    }
    return `tv_toast:${fingerprint}`;
  }

  return {
    normalizeAction,
    normalizeSymbol,
    extractSymbol,
    extractPrice,
    extractFirstNumber,
    resolveTriggerMetadata,
    extractPctTriggerValue,
    extractDirection,
    extractRawAction,
    extractRawDirection,
    parseJsonMessageSafe,
    makeEventId,
    SYMBOL_BLACKLIST,
    TRIGGER_PATTERNS
  };
});
