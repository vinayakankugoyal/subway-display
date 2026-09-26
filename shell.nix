# shell.nix
{ pkgs ? import <nixpkgs> {} }:

pkgs.mkShell {
  packages = with pkgs; [
    arduino-cli
    python3          # esptool, used by the ESP32 core for uploads
  ];

  shellHook = ''
    export FQBN=esp32:esp32:esp32
    export PORT=$(ls /dev/cu.usbserial-* 2>/dev/null | head -n1)

    alias build='arduino-cli compile --fqbn $FQBN .'
    alias flash='arduino-cli upload  --fqbn $FQBN -p $PORT .'
    alias mon='arduino-cli monitor -p $PORT -c baudrate=115200'

    echo "arduino-cli ready — port: ''${PORT:-not found}"
  '';
}