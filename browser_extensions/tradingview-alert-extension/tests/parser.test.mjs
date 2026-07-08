import test from "node:test";
import assert from "node:assert/strict";
import { createRequire } from "node:module";

const require = createRequire(import.meta.url);
const parser = require("../content_scripts/lib/parser.js");

const { normalizeAction, extractSymbol, extractPrice, extractDirection } = parser;

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
test("extractDirection: entering_channel", () => assert.equal(extractDirection("EURUSD Entering Channel"), "entering_channel"));
test("extractDirection: unknown -> null", () => assert.equal(extractDirection("RSI LONG zone on EURUSD"), null));