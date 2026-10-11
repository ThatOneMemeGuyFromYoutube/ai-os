"use strict";

const assert = require("node:assert/strict");
const fs = require("node:fs");
const path = require("node:path");
const test = require("node:test");
const vm = require("node:vm");

const root = path.resolve(__dirname, "..");
const demoSource = fs.readFileSync(path.join(root, "docs", "v86-demo.js"), "utf8");
const demoCss = fs.readFileSync(path.join(root, "docs", "v86-demo.css"), "utf8");

class ElementStub {
  constructor(id, parentNode = null, isCanvas = false) {
    this.id = id;
    this.parentNode = parentNode;
    this.listeners = Object.create(null);
    this.disabled = false;
    this.textContent = "";
    this.value = id === "network_device" ? "ne2k" : "";
    this.clientWidth = 480;
    this.style = {};
    this.width = isCanvas ? 720 : undefined;
    this.height = isCanvas ? 400 : undefined;
    this.canvas = isCanvas ? null : new ElementStub("canvas", this, true);
  }
  addEventListener(type, callback) {
    (this.listeners[type] ||= []).push(callback);
  }
  dispatch(type, event = {}) {
    const dispatchedEvent = { ...event, type, target: event.target || this };
    for (let current = this; current; current = current.parentNode) {
      for (const callback of current.listeners[type] || []) callback(dispatchedEvent);
    }
  }
  focus() {}
  querySelector(selector) { return selector === "canvas" ? this.canvas : null; }
}

function createDemo({ api = "modern" } = {}) {
  const elements = Object.fromEntries(
    ["screen_container", "status", "pause", "reset", "fullscreen", "mouse", "network_device"]
      .map((id) => [id, new ElementStub(id)]),
  );
  const documentListeners = Object.create(null);
  const documentStub = {
    pointerLockElement: null,
    body: new ElementStub("body"),
    getElementById: (id) => elements[id] || null,
    addEventListener(type, callback) {
      (documentListeners[type] ||= []).push(callback);
    },
    exitPointerLock() {
      documentStub.pointerLockElement = null;
      for (const callback of documentListeners.pointerlockchange || []) callback();
    },
  };
  const instances = [];
  class MockV86 {
    constructor(options) {
      this.options = options;
      this.listeners = Object.create(null);
      this.calls = [];
      instances.push(this);
    }
    add_listener(type, callback) {
      (this.listeners[type] ||= []).push(callback);
    }
    emit(type, payload) {
      for (const callback of this.listeners[type] || []) callback(payload);
    }
    async stop() { this.calls.push("stop"); this.emit("emulator-stopped"); }
    async run() { this.calls.push("run"); this.emit("emulator-started"); }
    async destroy() { this.calls.push("destroy"); }
    async lock_mouse() {
      this.calls.push("lock_mouse");
      documentStub.pointerLockElement = documentStub.body; // current v86 locks body
      for (const callback of documentListeners.pointerlockchange || []) callback();
    }
    mouse_set_enabled(enabled) { this.calls.push(["mouse_set_enabled", enabled]); }
    screen_go_fullscreen() { this.calls.push("fullscreen"); }
  }
  const windowStub = {};
  if (api === "modern") windowStub.V86 = MockV86;
  if (api === "legacy") windowStub.V86Starter = MockV86;
  const consoleStub = { error() {} };
  vm.runInNewContext(demoSource, {
    document: documentStub,
    window: windowStub,
    console: consoleStub,
  }, { filename: "docs/v86-demo.js" });
  return { elements, instances, documentStub };
}

const tick = () => new Promise((resolve) => setImmediate(resolve));

test("uses the current v86 screen API and configures the selected NIC", () => {
  const { elements, instances } = createDemo();
  assert.equal(instances.length, 1);
  const options = instances[0].options;
  assert.equal(options.screen.container, elements.screen_container);
  assert.equal(options.screen.use_graphical_text, false);
  assert.equal(Object.hasOwn(options, "screen_container"), false);
  assert.equal(options.net_device.type, "ne2k");
  assert.equal(options.autostart, true);
  assert.equal(options.cdrom.url, "ai-os.iso");
  assert.equal(elements.pause.disabled, true, "controls stay disabled until v86 is ready");
  instances[0].emit("emulator-ready");
  instances[0].emit("emulator-started");
  assert.equal(elements.pause.disabled, false);
  assert.match(elements.status.textContent, /NE2K device selected/);
  assert.match(elements.status.textContent, /network backend not configured/);
});

test("falls back to screen_container for legacy V86Starter bundles", () => {
  const { elements, instances } = createDemo({ api: "legacy" });
  const options = instances[0].options;
  assert.equal(options.screen_container, elements.screen_container);
  assert.equal(Object.hasOwn(options, "screen"), false);
});

test("surfaces v86 asset download failures and allows a reset retry", () => {
  const { elements, instances } = createDemo();
  instances[0].emit("download-error", { file_name: "v86/v86.wasm" });
  assert.match(elements.status.textContent, /Unable to load v86\/v86\.wasm/);
  assert.equal(elements.pause.disabled, true);
  assert.equal(elements.reset.disabled, false);
});

test("pause and resume wait for asynchronous v86 state changes", async () => {
  const { elements, instances } = createDemo();
  const emulator = instances[0];
  emulator.emit("emulator-ready");
  emulator.emit("emulator-started");
  elements.pause.dispatch("click");
  await tick();
  assert.deepEqual(emulator.calls, ["stop"]);
  assert.equal(elements.pause.textContent, "Resume");
  assert.equal(elements.status.textContent, "AsterOS paused");
  elements.pause.dispatch("click");
  await tick();
  assert.deepEqual(emulator.calls, ["stop", "run"]);
  assert.equal(elements.pause.textContent, "Pause");
});

test("reset recreates v86 with the newly selected network device", async () => {
  const { elements, instances } = createDemo();
  instances[0].emit("emulator-ready");
  elements.network_device.value = "virtio";
  elements.reset.dispatch("click");
  await tick();
  await tick();
  assert.equal(instances.length, 2);
  assert.deepEqual(instances[0].calls, ["stop", "destroy"]);
  assert.equal(instances[1].options.net_device.type, "virtio");
  assert.equal(instances[1].options.screen.container, elements.screen_container);
});

test("canvas presentation stays within the demo viewport without distorting aspect ratio", () => {
  assert.match(demoCss, /\.screen\s*>\s*canvas\s*\{[^}]*max-width:\s*100%/s);
  assert.match(demoCss, /\.screen\s*>\s*canvas\s*\{[^}]*height:\s*auto\s*!important/s);
  assert.match(demoCss, /\.screen:fullscreen\s*\{[^}]*width:\s*100vw/s);
  assert.match(demoCss, /image-rendering:\s*pixelated/);
  assert.match(demoCss, /\.screen\s*>\s*canvas\s*\{[^}]*pointer-events:\s*auto/s,
    "the VM framebuffer canvas must be the actual pointer target");
  assert.match(demoCss, /\.screen:focus-visible\s*\{[^}]*box-shadow:\s*inset 0 0 0 2px #89a8ff/s,
    "keep the browser focus highlight as a useful signal");
});


test("pointerdown on the VM canvas starts capture before click synthesis", async () => {
  const { elements, instances, documentStub } = createDemo();
  const emulator = instances[0];
  emulator.emit("emulator-ready");

  const targets = [];
  elements.screen_container.addEventListener("pointerdown", (event) => targets.push(event.target));
  elements.screen_container.canvas.dispatch("pointerdown", { button: 0 });
  await tick();

  assert.equal(targets[0], elements.screen_container.canvas,
    "the original canvas target must bubble to the emulator container");
  assert.deepEqual(emulator.calls, [["mouse_set_enabled", true], "lock_mouse"]);
  assert.equal(documentStub.pointerLockElement, documentStub.body);
  assert.equal(elements.mouse.textContent, "Release mouse");

  // v86 locks document.body, not #screen_container; UI must still recognize it.
  elements.mouse.dispatch("click");
  assert.equal(documentStub.pointerLockElement, null);
  assert.equal(elements.mouse.textContent, "Capture mouse");
});

test("the VM canvas receives pointer events and right-click does not steal the pointer", async () => {
  const { elements, instances, documentStub } = createDemo();
  instances[0].emit("emulator-ready");
  const targets = [];
  elements.screen_container.addEventListener("pointerdown", (event) => targets.push(event.target));

  elements.screen_container.canvas.dispatch("pointerdown", { button: 2 });

  assert.equal(targets[0], elements.screen_container.canvas);
  assert.equal(documentStub.pointerLockElement, null);
  assert.deepEqual(instances[0].calls, []);
});

test("toolbar Capture mouse enables guest input before acquiring pointer lock", async () => {
  const { elements, instances, documentStub } = createDemo();
  const emulator = instances[0];
  emulator.emit("emulator-ready");

  elements.mouse.dispatch("click");
  await tick();
  assert.deepEqual(emulator.calls, [["mouse_set_enabled", true], "lock_mouse"]);
  assert.equal(documentStub.pointerLockElement, documentStub.body);
  assert.equal(elements.mouse.textContent, "Release mouse");
});
