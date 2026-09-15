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
  var emulator;
  var running = true;
  var mouseLocked = false;

  function setStatus(text) { status.textContent = text; }
  function selectedNetworkType() {
    return networkDevice && networkDevice.value !== "none" ? networkDevice.value : null;
  }
  function networkLabel(type) {
    return type === "virtio" ? "VirtIO" : "NE2K";
  }
  function setMouseState(locked) {
    mouseLocked = locked;
    if (mouseButton) mouseButton.textContent = locked ? "Release mouse" : "Capture mouse";
  }
  function captureMouse() {
    if (!emulator || mouseLocked) return;
    emulator.lock_mouse();
    screen.focus();
    setStatus("Mouse captured · press Esc to release");
  }

  function boot() {
    var networkType = selectedNetworkType();
    setStatus(networkType ? "Booting AsterOS with " + networkLabel(networkType) + " network device…" : "Booting AsterOS…");
    try {
      var options = {
        wasm_path: "v86/v86.wasm",
        memory_size: 64 * 1024 * 1024,
        vga_memory_size: 8 * 1024 * 1024,
        screen_container: screen,
        bios: { url: "v86/seabios.bin" },
        vga_bios: { url: "v86/vgabios.bin" },
        cdrom: { url: "ai-os.iso" },
        autostart: true
      };
      if (networkType) options.net_device = { type: networkType };

      emulator = new V86(options);

      emulator.add_listener("emulator-ready", function () {
        setStatus(networkType
          ? "AsterOS is running · " + networkLabel(networkType) + " network device selected"
          : "AsterOS is running · no network device selected");
      });
      emulator.add_listener("emulator-loaded", function () {
        setStatus(networkType
          ? "AsterOS is running · " + networkLabel(networkType) + " network device selected"
          : "AsterOS is running · no network device selected");
      });
    } catch (error) {
      running = false;
      pauseButton.disabled = true;
      resetButton.disabled = true;
      if (mouseButton) mouseButton.disabled = true;
      if (networkDevice) networkDevice.disabled = true;
      fullscreenButton.disabled = true;
      setStatus("Unable to start v86: " + error.message);
      console.error(error);
    }
  }

  pauseButton.addEventListener("click", function () {
    if (!emulator) return;
    if (running) {
      emulator.stop();
      running = false;
      pauseButton.textContent = "Resume";
      setStatus("AsterOS paused");
    } else {
      emulator.run();
      running = true;
      pauseButton.textContent = "Pause";
      setStatus("AsterOS is running");
      screen.focus();
    }
  });

  resetButton.addEventListener("click", function () {
    if (!emulator) return;
    if (document.pointerLockElement && document.exitPointerLock) document.exitPointerLock();
    resetButton.disabled = true;
    pauseButton.disabled = true;
    if (networkDevice) networkDevice.disabled = true;
    setStatus("Rebooting AsterOS…");
    emulator.stop().then(function () {
      return emulator.destroy();
    }).then(function () {
      emulator = null;
      running = true;
      setMouseState(false);
      pauseButton.textContent = "Pause";
      resetButton.disabled = false;
      if (networkDevice) networkDevice.disabled = false;
      screen.focus();
      boot();
    }).catch(function (error) {
      resetButton.disabled = false;
      pauseButton.disabled = false;
      if (networkDevice) networkDevice.disabled = false;
      setStatus("Unable to reset v86: " + error.message);
      console.error(error);
    });
  });

  if (mouseButton) {
    mouseButton.addEventListener("click", function () {
      if (!emulator) return;
      if (mouseLocked) {
        if (document.exitPointerLock) document.exitPointerLock();
      } else {
        captureMouse();
      }
    });
  }

  screen.addEventListener("click", function () {
    screen.focus();
    captureMouse();
  });

  document.addEventListener("pointerlockchange", function () {
    var locked = document.pointerLockElement === screen;
    setMouseState(locked);
    if (!locked && emulator) setStatus(running ? "AsterOS is running · mouse released" : "AsterOS paused");
  });

  screen.addEventListener("contextmenu", function (event) { event.preventDefault(); });

  fullscreenButton.addEventListener("click", function () {
    if (!emulator) return;
    emulator.screen_go_fullscreen();
  });

  boot();
})();
