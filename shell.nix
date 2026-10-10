# Dev shell for both halves of the project.
#
#   nix-shell          # drops you in with every toolchain below on PATH
#   build / flash      # compile and upload the ESP32 client
#   mon                # serial monitor
#   serve              # build and run the arrivals server
#
# Caveat worth knowing: Nix provides the *toolchains* here, not every
# dependency. The ESP32 core (~7.8GB of Espressif binaries) and the Arduino
# libraries are fetched by arduino-cli, and the server's packages by npm.
# Neither is in nixpkgs -- there is no xtensa-esp32 compiler there at all.
# Run `setup` once on a new machine to pull both.
{ pkgs ? import <nixpkgs> {} }:

pkgs.mkShell {
  packages = with pkgs; [
    arduino-cli
    nodejs_22        # server: build + run
    python3          # esptool, used by the ESP32 core for uploads
  ];

  shellHook = ''
    export FQBN=esp32:esp32:esp32
    export PORT=$(ls /dev/cu.usbserial-* 2>/dev/null | head -n1)

    # Keep Arduino libraries beside the cores in ~/Library/Arduino15 rather than
    # the default ~/Documents/Arduino, which some terminals cannot read under
    # macOS TCC -- which shows up as a baffling "Adafruit_GFX.h: No such file".
    export ARDUINO_DIRECTORIES_USER="$HOME/Library/Arduino15"

    alias build='arduino-cli compile --fqbn $FQBN client'
    alias flash='arduino-cli upload  --fqbn $FQBN -p $PORT client'
    alias mon='arduino-cli monitor -p $PORT -c baudrate=115200'
    alias serve='(cd server && npm run build && npm start)'

    setup() {
      arduino-cli core update-index &&
      arduino-cli core install esp32:esp32@3.3.11 &&
      arduino-cli lib install "Adafruit GFX Library@1.12.6" "ArduinoJson@7.4.3" &&
      (cd server && npm ci)
    }

    echo "arduino-cli $(arduino-cli version | awk '{print $3}') · node $(node --version) · port: ''${PORT:-not found}"
  '';
}
