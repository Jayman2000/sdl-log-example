{ pkgs ? import <nixpkgs> { } }:

pkgs.mkShell {
  name = "shell-for-building-sdl-log-example";
  packages = with pkgs; [
    SDL2
    cmake
    pkg-config
    systemd.dev
  ];
  hardeningDisable = [ "all" ];
}
