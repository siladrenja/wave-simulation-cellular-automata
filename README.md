# Wave CA — GPU water-wave simulation

C++20 + OpenGL 3.3 cellular-automata wave simulation. The wave equation runs
entirely on the GPU using a fragment shader and ping-pong framebuffer textures.

## NixOS build

```sh
nix develop
cmake -S . -B build -G Ninja
cmake --build build
./build/wave_ca
```

## Windows release (built on NixOS)

Cross-compile and copy the ready-to-distribute ZIP into `build/`:

```sh
nix build .#windows
cp result/wave-ca-windows-x86_64.zip build/
```

Extract the ZIP on a 64-bit Windows system and run `wave_ca.exe`. The archive
contains the shaders and statically linked MinGW, GLFW, and GLEW dependencies,
so it does not require Nix or a compiler on the Windows machine.

Controls:

- **Left drag (3D):** add ripples using water-plane ray picking
- **Right drag (3D):** orbit camera
- **Mouse wheel:** zoom
- **Space:** toggle 3D and 2D editor
- **Left drag (2D):** paint solid obstacles
- **Escape:** quit

## Structure

```text
wave-ca/
├── flake.nix
├── CMakeLists.txt
├── README.md
├── shaders/
│   └── GLSL simulation and rendering shaders
└── src/
    └── main.cpp
```

The simulation uses a stable four-neighbor wave stencil. Obstacle cells are held
at zero and neighboring cells use reflective (zero normal derivative) boundary
samples. The 2D view shows positive displacement in blue, negative displacement
in red, and obstacles in yellow.
