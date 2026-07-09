import test from "node:test";
import assert from "node:assert/strict";
import { readFileSync } from "node:fs";
import { fileURLToPath } from "node:url";
import { dirname, join } from "node:path";
import { JSDOM } from "jsdom";
import { createRequire } from "node:module";

const require = createRequire(import.meta.url);
const parser = require("../content_scripts/lib/parser.js");

const __dirname = dirname(fileURLToPath(import.meta.url));
const FIXTURE_PATH = join(__dirname, "fixtures", "toast-sample.html");

function buildDomFromFixture() {
  const html = readFileSync(FIXTURE_PATH, "utf-8");
  const dom = new JSDOM(`<!DOCTYPE html><html><body>${html}</body></html>`, {
    runScripts: "outside-only",
    pretendToBeVisual: true
  });
  return dom;
}

test("integration: extractSymbol from real DOM fixture (EURUSD Crossing)", () => {
  const dom = buildDomFromFixture();
  const descriptionEls = dom.window.document.querySelectorAll('[class*="description"]');
  assert.ok(descriptionEls.length >= 3, "fixture should have at least 3 description spans");

  const first = descriptionEls[0];
  const message = first.textContent.trim();
  assert.equal(message, "EURUSD Crossing 1.14145");

  const symbol = parser.extractSymbol("", message, null);
  assert.equal(symbol, "EURUSD");

  const direction = parser.extractDirection(message);
  assert.equal(direction, "cross");

  const price = parser.extractPrice(message, null);
  assert.equal(price, 1.14145);
});

test("integration: extractSymbol from real DOM fixture (EURUSD Crossing Up)", () => {
  const dom = buildDomFromFixture();
  const els = dom.window.document.querySelectorAll('[class*="description"]');
  const upEl = Array.from(els).find((e) => /Crossing Up/.test(e.textContent));
  assert.ok(upEl, "fixture should contain 'Crossing Up' span");

  const message = upEl.textContent.trim();
  assert.equal(message, "EURUSD Crossing Up 1.14143");

  const symbol = parser.extractSymbol("", message, null);
  assert.equal(symbol, "EURUSD");

  const direction = parser.extractDirection(message);
  assert.equal(direction, "up");

  const price = parser.extractPrice(message, null);
  assert.equal(price, 1.14143);
});

test("integration: extractSymbol from real DOM fixture (EURUSD Crossing Down)", () => {
  const dom = buildDomFromFixture();
  const els = dom.window.document.querySelectorAll('[class*="description"]');
  const downEl = Array.from(els).find((e) => /Crossing Down/.test(e.textContent));
  assert.ok(downEl);

  const message = downEl.textContent.trim();
  assert.equal(message, "EURUSD Crossing Down 1.14142");

  const symbol = parser.extractSymbol("", message, null);
  assert.equal(symbol, "EURUSD");

  const direction = parser.extractDirection(message);
  assert.equal(direction, "down");
});

test("integration: dynamic DOM insertion via MutationObserver", async () => {
  const dom = buildDomFromFixture();
  const { window } = dom;
  const document = window.document;

  const seenDirections = [];
  const observer = new window.MutationObserver((mutations) => {
    for (const mutation of mutations) {
      for (const node of mutation.addedNodes) {
        if (node.nodeType !== 1) continue;
        const desc = node.querySelector && node.querySelector('[class*="description"]');
        if (!desc) continue;
        const text = desc.textContent.trim();
        seenDirections.push(parser.extractDirection(text));
      }
    }
  });

  observer.observe(document.body, { childList: true, subtree: true });

  const toast = document.createElement("div");
  toast.className = "toast-item";
  toast.innerHTML = `<span class="description-dynamic">BTCUSDT Greater Than 50000</span>`;
  document.body.appendChild(toast);

  await new Promise((resolve) => setTimeout(resolve, 0));
  observer.disconnect();
  assert.deepEqual(seenDirections, ["above"]);
});
