/* AsterOS browser demo powered by v86. */
(function () {
  "use strict";

  var screen = document.getElementById("screen_container");
  var status = document.getElementById("status");
  var pauseButton = document.getElementById("pause");
  var resetButton = document.getElementById("reset");
  var fullscreenButton = document.getElementById("fullscreen");
  var mouseButton = document.getElementById("mouse");
  var networkDevice = document.getElementById("network_device");
  var emulator = null;
  var emulatorReady = false;
  var running = false;
  var transitioning = false;
  var failed = false;
  var mouseLocked = false;
  var activeNetworkType = null;

  function setStatus(message) {
    status.textContent = message;
  }

  function selectedNetworkType() {
    return networkDevice && networkDevice.value !== "none" ? networkDevice.value : null;
  }

  function networkLabel(type) {
    return type === "virtio" ? "VirtIO" : "NE2K";
  }

  function runningMessage() {
    return activeNetworkType
      ? "AsterOS is running · " + networkLabel(activeNetworkType) + " device selected (network backend not configured)"
      : "AsterOS is running · no network device selected";
  }

  function updateControls() {
    var canOperate = emulatorReady && !transitioning && !failed;
    pauseButton.disabled = !canOperate;
    pauseButton.textContent = running ? "Pause" : "Resume";
    resetButton.disabled = !emulator || transitioning;
    fullscreenButton.disabled = !emulatorReady || transitioning;
    if (mouseButton) mouseButton.disabled = !canOperate;
    if (networkDevice) networkDevice.disabled = !emulatorReady || transitioning;
  }

  function setMouseState(locked) {
    mouseLocked = locked;
    if (mouseButton) mouseButton.textContent = locked ? "Release mouse" : "Capture mouse";
    if (!emulatorReady || transitioning || failed) return;
    if (locked) setStatus("Mouse captured · press Esc to release");
    else setStatus(running ? runningMessage() + " · mouse released" : "AsterOS paused");
  }

  function isPointerLocked() {
    // v86's lock_mouse() currently requests pointer lock on document.body,
    // not on the screen container. Accept the actual API target.
    return Boolean(document.pointerLockElement);
  }

  async function captureMouse() {
    if (!emulator || !emulatorReady || failed || transitioning) return;
    if (isPointerLocked()) {
      setMouseState(true);
      return;
    }
    try {
      screen.focus();
      // Match v86's own Lock mouse button: make sure emulator input has not
      // been disabled before requesting pointer lock.
      if (typeof emulator.mouse_set_enabled === "function") {
        emulator.mouse_set_enabled(true);
      }
      // lock_mouse() is asynchronous in current v86. Await it so rejected
      // browser pointer-lock requests are visible instead of becoming unhandled.
      await emulator.lock_mouse();
      if (!isPointerLocked()) {
        setStatus("Requesting mouse capture · press Esc to release");
      }
    } catch (error) {
      setStatus("Unable to capture mouse: " + error.message);
      console.error(error);
    }
  }

  function boot() {
    var networkType = selectedNetworkType();
    var modernApi = typeof window.V86 === "function";
    var V86Constructor = modernApi ? window.V86 : window.V86Starter;
    var options;

    emulatorReady = false;
    running = false;
    transitioning = false;
    failed = false;
    activeNetworkType = networkType;
    if (mouseLocked && document.exitPointerLock) document.exitPointerLock();
    mouseLocked = false;
    updateControls();
    setStatus(networkType
      ? "Booting AsterOS with " + networkLabel(networkType) + " device…"
      : "Booting AsterOS…");

    if (typeof V86Constructor !== "function") {
      failed = true;
      setStatus("Unable to start v86: browser runtime did not expose V86 or V86Starter");
      updateControls();
      return;
    }

    options = {
      wasm_path: "v86/v86.wasm",
      memory_size: 64 * 1024 * 1024,
      vga_memory_size: 8 * 1024 * 1024,
      screen: { container: screen, use_graphical_text: false },
      bios: { url: "v86/seabios.bin" },
      vga_bios: { url: "v86/vgabios.bin" },
      cdrom: { url: "ai-os.iso" },
      autostart: true
    };
    if (networkType) options.net_device = { type: networkType };

    /* Older v86 bundles expose V86Starter and only accept screen_container. */
    if (!modernApi && typeof window.V86Starter === "function") {
      delete options.screen;
      options.screen_container = screen;
    }

    try {
      emulator = new V86Constructor(options);
      emulator.add_listener("emulator-ready", function () {
        emulatorReady = true;
        failed = false;
        updateControls();
        screen.focus();
        setStatus(running ? runningMessage() : "AsterOS emulator ready");
      });
      emulator.add_listener("emulator-started", function () {
        running = true;
        updateControls();
        if (emulatorReady && !transitioning && !failed) setStatus(runningMessage());
      });
      emulator.add_listener("emulator-stopped", function () {
        running = false;
        updateControls();
        if (emulatorReady && !transitioning && !failed) setStatus("AsterOS paused");
      });
      emulator.add_listener("download-error", function (event) {
        failed = true;
        updateControls();
        var fileName = event && event.file_name ? event.file_name : "a required emulator asset";
        setStatus("Unable to load " + fileName + ". Check the connection and Reset to retry.");
        console.error("v86 download failed", event);
      });
    } catch (error) {
      emulator = null;
      emulatorReady = false;
      running = false;
      failed = true;
      updateControls();
      setStatus("Unable to start v86: " + error.message);
      console.error(error);
    }
  }

  async function toggleRunState() {
    if (!emulator || !emulatorReady || transitioning || failed) return;
    var wasRunning = running;
    transitioning = true;
    updateControls();
    setStatus(wasRunning ? "Pausing AsterOS…" : "Resuming AsterOS…");
    try {
      if (wasRunning) await emulator.stop();
      else await emulator.run();
      running = !wasRunning;
      setStatus(running ? runningMessage() : "AsterOS paused");
    } catch (error) {
      failed = true;
      setStatus("Unable to " + (wasRunning ? "pause" : "resume") + " AsterOS: " + error.message);
      console.error(error);
    } finally {
      transitioning = false;
      updateControls();
    }
  }

  async function resetEmulator() {
    if (!emulator || transitioning) return;
    var previousEmulator = emulator;
    transitioning = true;
    updateControls();
    if (document.pointerLockElement && document.exitPointerLock) document.exitPointerLock();
    setStatus("Rebooting AsterOS…");
    try {
      await previousEmulator.stop();
      await previousEmulator.destroy();
      if (emulator !== previousEmulator) return;
      emulator = null;
      emulatorReady = false;
      running = false;
      failed = false;
      transitioning = false;
      mouseLocked = false;
      if (mouseButton) mouseButton.textContent = "Capture mouse";
      boot();
    } catch (error) {
      transitioning = false;
      failed = true;
      updateControls();
      setStatus("Unable to reset v86: " + error.message);
      console.error(error);
    }
  }

  pauseButton.addEventListener("click", function () { toggleRunState(); });
  resetButton.addEventListener("click", function () { resetEmulator(); });

  if (mouseButton) {
    mouseButton.addEventListener("click", function () {
      if (!emulator || !emulatorReady || transitioning) return;
      if (mouseLocked || isPointerLocked()) {
        if (document.exitPointerLock) document.exitPointerLock();
      } else {
        captureMouse();
      }
    });
  }

  screen.addEventListener("pointerdown", function (event) {
    // Capture on the press gesture, before the browser synthesizes click.
    // Keep this event's target on v86's framebuffer canvas so v86's
    // window-level mouse handlers route the press into the guest.
    if (event.button !== 0) return;
    screen.focus();
    captureMouse();
  });

  document.addEventListener("pointerlockchange", function () {
    setMouseState(isPointerLocked());
  });

  screen.addEventListener("contextmenu", function (event) { event.preventDefault(); });

  fullscreenButton.addEventListener("click", function () {
    if (!emulator || !emulatorReady) return;
    try {
      emulator.screen_go_fullscreen();
    } catch (error) {
      setStatus("Unable to enter fullscreen: " + error.message);
      console.error(error);
    }
  });

  updateControls();
  boot();
})();
