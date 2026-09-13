/* AsterOS browser demo powered by v86. */
(function () {
  "use strict";

  var screen = document.getElementById("screen_container");
  var status = document.getElementById("status");
  var pauseButton = document.getElementById("pause");
  var resetButton = document.getElementById("reset");
  var fullscreenButton = document.getElementById("fullscreen");
  var emulator;
  var running = true;

  function setStatus(text) {
    status.textContent = text;
  }

  function boot() {
    setStatus("Booting AsterOS…");
    screen.focus();

    try {
      emulator = new V86({
        wasm_path: "v86/v86.wasm",
        memory_size: 64 * 1024 * 1024,
        vga_memory_size: 8 * 1024 * 1024,
        screen_container: screen,
        bios: { url: "v86/seabios.bin" },
        vga_bios: { url: "v86/vgabios.bin" },
        cdrom: { url: "ai-os.iso" },
        autostart: true,
      });

      emulator.add_listener("emulator-ready", function () {
        setStatus("AsterOS is running");
      });
      emulator.add_listener("emulator-loaded", function () {
        setStatus("AsterOS is running");
      });
    } catch (error) {
      running = false;
      pauseButton.disabled = true;
      resetButton.disabled = true;
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
    emulator.restart();
    running = true;
    pauseButton.textContent = "Pause";
    setStatus("Rebooting AsterOS…");
    screen.focus();
  });

  fullscreenButton.addEventListener("click", function () {
    if (screen.requestFullscreen) {
      screen.requestFullscreen();
    } else if (screen.webkitRequestFullscreen) {
      screen.webkitRequestFullscreen();
    }
  });

  screen.addEventListener("click", function () {
    screen.focus();
  });

  boot();
})();
