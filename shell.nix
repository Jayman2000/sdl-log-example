{ pkgs ? import <nixpkgs> { } }:

pkgs.mkShell {
  name = "shell-for-building-sdl-log-example";
  packages = with pkgs; [
    cmake
    SDL2
  ];
}
