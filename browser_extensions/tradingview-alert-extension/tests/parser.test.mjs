import test from "node:test";
import assert from "node:assert/strict";
import { createRequire } from "node:module";

const require = createRequire(import.meta.url);
const parser = require("../content_scripts/lib/parser.js");
const defaults = require("../content_scripts/lib/defaults.js");

const { normalizeAction, extractSymbol, extractPrice, extractDirection, extractRawAction, extractRawDirection, parseJsonMessageSafe, makeEventId } = parser;

test("normalizeAction: BUY EURUSD -> buy", () => assert.equal(normalizeAction("BUY EURUSD", null), "buy"));
test("normalizeAction: parsed action=buy", () => assert.equal(normalizeAction("", { action: "buy" }), "buy"));
test("normalizeAction: parsed side=sell", () => assert.equal(normalizeAction("", { side: "sell" }), "sell"));
test("normalizeAction: LONG command -> buy", () => assert.equal(normalizeAction("LONG BTCUSDT", null), "buy"));
test("normalizeAction: SHORT command -> sell", () => assert.equal(normalizeAction("SHORT BTCUSDT", null), "sell"));
test("normalizeAction: RSI LONG zone -> alert", () => assert.equal(normalizeAction("RSI LONG zone on EURUSD", null), "alert"));
test("normalizeAction: empty -> alert", () => assert.equal(normalizeAction("", null), "alert"));

test("extractSymbol: EURUSD Crossing 1.14145 -> EURUSD", () => assert.equal(extractSymbol("", "EURUSD Crossing 1.14145", null), "EURUSD"));
test("extractSymbol: EURUSD Crossing Up", () => assert.equal(extractSymbol("", "EURUSD Crossing Up 1.14143", null), "EURUSD"));
test("extractSymbol: Alert on BINANCE:BTCUSDT from title", () => assert.equal(extractSymbol("Alert on BINANCE:BTCUSDT", "", null), "BINANCE:BTCUSDT"));
test("extractSymbol: blacklist filters PRICE", () => assert.equal(extractSymbol("", "PRICE crosses level", null), ""));
test("extractSymbol: parsed.symbol priority", () => assert.equal(extractSymbol("", "anything", { symbol: "XAUUSD" }), "XAUUSD"));

test("extractPrice: first number not last", () => assert.equal(extractPrice("BUY EURUSD at 1.14 target 1.20", null), 1.14));
test("extractPrice: parsed JSON price", () => assert.equal(extractPrice("anything", { price: 1.14 }), 1.14));
test("extractPrice: no number -> null", () => assert.equal(extractPrice("BUY EURUSD", null), null));

test("extractDirection: cross", () => assert.equal(extractDirection("EURUSD Crossing 1.14145"), "cross"));
test("extractDirection: up", () => assert.equal(extractDirection("EURUSD Crossing Up 1.14143"), "up"));
test("extractDirection: down", () => assert.equal(extractDirection("EURUSD Crossing Down 1.14142"), "down"));
test("extractDirection: above (Greater Than)", () => assert.equal(extractDirection("EURUSD Greater Than 1.15"), "above"));
test("extractDirection: below (Less Than)", () => assert.equal(extractDirection("EURUSD Less Than 1.10"), "below"));
test("extractDirection: moving_up_pct", () => assert.equal(extractDirection("EURUSD Moving Up 1.0%"), "moving_up_pct"));
test("extractDirection: moving_up (no percent)", () => {
  assert.equal(extractDirection("EURUSD Moving Up"), "moving_up");
});

test("extractDirection: moving_up_pct (with number and percent)", () => {
  assert.equal(extractDirection("EURUSD Moving Up 1.0%"), "moving_up_pct");
});

test("extractDirection: moving_down (no percent)", () => {
  assert.equal(extractDirection("EURUSD Moving Down"), "moving_down");
});

test("extractDirection: moving_down_pct (with number and percent)", () => {
  assert.equal(extractDirection("EURUSD Moving Down 2.5%"), "moving_down_pct");
});
test("extractDirection: entering_channel", () => assert.equal(extractDirection("EURUSD Entering Channel"), "entering_channel"));
test("extractDirection: unknown -> null", () => assert.equal(extractDirection("RSI LONG zone on EURUSD"), null));

test("extractSymbol: fromTitle blacklist filter PRICE", () => {
  assert.equal(extractSymbol("Alert on PRICE", "", null), "");
});

test("extractSymbol: fromTitle blacklist filter RSI", () => {
  assert.equal(extractSymbol("Alert on RSI", "", null), "");
});

test("extractSymbol: mid-string trigger 'AAPL crosses 200' -> AAPL", () => {
  assert.equal(extractSymbol("", "long story here AAPL crosses 200", null), "AAPL");
});

test("extractSymbol: 'TV BUY EURUSD' -> EURUSD (BUY blacklisted, EURUSD not)", () => {
  assert.equal(extractSymbol("", "TV BUY EURUSD", null), "EURUSD");
});

test("extractDirection: no longer matches >=", () => {
  assert.equal(extractDirection("AAPL >= 200"), null);
});

test("extractDirection: no longer matches <=", () => {
  assert.equal(extractDirection("AAPL <= 100"), null);
});

test("extractDirection: Greater Than still works", () => {
  assert.equal(extractDirection("EURUSD Greater Than 1.15"), "above");
});

test("extractRawAction: from parsed JSON", () => {
  assert.equal(extractRawAction("", { action: "GOOGLY" }), "GOOGLY");
});

test("extractRawAction: from command pattern", () => {
  assert.equal(extractRawAction("BUY EURUSD", null), "BUY");
});

test("extractRawAction: empty when no source", () => {
  assert.equal(extractRawAction("RSI LONG zone on EURUSD", null), null);
});

test("extractRawDirection: matches 'Crossing Up'", () => {
  assert.equal(extractRawDirection("EURUSD Crossing Up 1.14143"), "Crossing Up");
});

test("extractRawDirection: matches 'Greater Than'", () => {
  assert.equal(extractRawDirection("EURUSD Greater Than 1.15"), "Greater Than");
});

test("extractRawDirection: null when no trigger", () => {
  assert.equal(extractRawDirection("RSI LONG zone on EURUSD"), null);
});

test("parseJsonMessageSafe: handles leading whitespace", () => {
  assert.deepEqual(parseJsonMessageSafe('  {"a":1}'), { a: 1 });
});

test("parseJsonMessageSafe: rejects non-JSON", () => {
  assert.equal(parseJsonMessageSafe("not json"), null);
});

test("makeEventId: null parsed -> tv_toast fallback", () => {
  assert.equal(makeEventId(null, "fp1"), "tv_toast:fp1");
});

test("makeEventId: canonical parsed.event_id used as-is", () => {
  assert.equal(makeEventId({ event_id: "abc-123" }, "fp1"), "abc-123");
});

test("makeEventId: parsed.fire_id prefixed with tv_fire", () => {
  assert.equal(makeEventId({ fire_id: 42 }, "fp1"), "tv_fire:42");
});

test("makeEventId: parsed.alert_id + time -> tv_alert", () => {
  assert.equal(
    makeEventId({ alert_id: "a1", time: "2026-07-08T12:00:00Z" }, "fp1"),
    "tv_alert:a1:2026-07-08T12:00:00Z"
  );
});

test("makeEventId: alert_id alone (no time) -> fallback tv_toast", () => {
  assert.equal(makeEventId({ alert_id: "a1" }, "fp1"), "tv_toast:fp1");
});

test("makeEventId: fire_time wins over time when both present", () => {
  assert.equal(
    makeEventId({ alert_id: "a1", fire_time: 1234567890, time: "should-not-win" }, "fp1"),
    "tv_alert:a1:1234567890"
  );
});

test("makeEventId: timenow as last resort for alert_id", () => {
  assert.equal(
    makeEventId({ alert_id: "a1", timenow: 1234567890 }, "fp1"),
    "tv_alert:a1:1234567890"
  );
});

test("DEFAULTS.enabled is false by default (safety)", () => {
  assert.equal(defaults.DEFAULTS.enabled, false);
});

test("DEFAULTS.endpoint points to local bridge", () => {
  assert.equal(defaults.DEFAULTS.endpoint, "http://127.0.0.1:6560/api/v1/tradingview/signal");
});

test("DEFAULTS.include_tab_url is false by default (privacy)", () => {
  assert.equal(defaults.DEFAULTS.include_tab_url, false);
});

test("DEFAULTS.secret is empty", () => {
  assert.equal(defaults.DEFAULTS.secret, "");
});
