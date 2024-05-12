# SDL Logging Example

## How to run the code that’s in this repo

1. Get a copy of this repo.
2. Change directory into your local copy of this repo:
    ```
    cd <repo>
    ```
3. Make sure that you have the following prerequisites installed:
    - [CMake](https://cmake.org)
    - [SDL](https://libsdl.org)2

    If you have [the Nix package manager](https://nix.dev) installed, then you
    can run `nix-shell` to start a shell with all of those prerequisites
    installed.
4. Generate the project’s build system:
    ```
    cmake -B build
    ```
5. Build the project:
    ```
    cmake --build build
    ```
6. Run the executable:
    ```
    build/sdl-log-example
    ```

## Copying

Everything in this repo is dedicated to the public domain using [🅭🄍 1.0](https://creativecommons.org/publicdomain/zero/1.0/).
