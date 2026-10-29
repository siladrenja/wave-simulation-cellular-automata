{
  description = "GPU cellular-automata water-wave simulation (C++ / OpenGL)";

  # Match the host release.  Mixing a 25.05 GLFW/GLVND stack with the 26.05
  # Mesa driver in /run/opengl-driver prevents EGL and GLX vendor loading.
  inputs.nixpkgs.url = "github:NixOS/nixpkgs/nixos-26.05";

  outputs = { self, nixpkgs }:
    let
      systems = [ "x86_64-linux" "aarch64-linux" ];
      forAllSystems = nixpkgs.lib.genAttrs systems;
      windowsPackage = system:
        let
          pkgs = import nixpkgs {
            inherit system;
            # GLEW builds for MinGW, although its Nixpkgs platform metadata
            # does not currently list Windows.
            config.allowUnsupportedSystem = true;
          };
          win = pkgs.pkgsCross.mingwW64;
          glfwWindows = win.glfw.overrideAttrs (old: {
            cmakeFlags = (old.cmakeFlags or [ ]) ++ [
              "-DBUILD_SHARED_LIBS=OFF"
              "-DGLFW_BUILD_EXAMPLES=OFF"
              "-DGLFW_BUILD_TESTS=OFF"
              "-DGLFW_BUILD_DOCS=OFF"
            ];
          });
          glewWindows = (win.glew.override { enableEGL = false; }).overrideAttrs (_: {
            # The stock derivation assumes every non-Darwin target is Unix and
            # consequently pulls X11/GLVND into MinGW builds. GLEW only needs
            # the Windows OpenGL import library for this target.
            buildInputs = [ ];
            propagatedBuildInputs = [ ];
            cmakeFlags = [ "-DBUILD_SHARED_LIBS=OFF" ];
          });
        in win.stdenv.mkDerivation {
          pname = "wave-ca-windows";
          version = "1.0.0";
          src = pkgs.lib.cleanSourceWith {
            src = self;
            filter = path: type:
              let name = baseNameOf path;
              in name != "build" && name != "result" && name != ".git";
          };

          nativeBuildInputs = with pkgs; [ cmake ninja ];
          buildInputs = [ glfwWindows glewWindows win.glm ];

          cmakeFlags = [
            "-DCMAKE_BUILD_TYPE=Release"
            "-DBUILD_SHARED_LIBS=OFF"
            "-DGLEW_USE_STATIC_LIBS=ON"
          ];

          postInstall = ''
            ${pkgs.zip}/bin/zip -j "$out/wave-ca-windows-x86_64.zip" "$out/wave_ca.exe" "$out/LICENSE"
            cd "$out"
            ${pkgs.zip}/bin/zip -r "wave-ca-windows-x86_64.zip" shaders
          '';

          dontFixup = true;
        };
    in {
      packages = forAllSystems (system: {
        windows = windowsPackage system;
      });

      devShells = forAllSystems (system:
        let pkgs = import nixpkgs { inherit system; };
        in {
          default = pkgs.mkShell {
            packages = with pkgs; [
              cmake
              ninja
              pkg-config
              gcc
              glfw
              glew
              glm
              libGL
              libGLU
              libx11
              libxcursor
              libxi
              libxinerama
              libxrandr
              libxext
              libxrender
              libxcb
              libxfixes
              libxau
              libxdmcp
              wayland
              libxkbcommon
              mesa-demos
            ];

            # NixOS exposes the hardware-specific Mesa drivers here. Preserve
            # any session paths (PipeWire on this machine adds one).
            shellHook = ''
              export LD_LIBRARY_PATH="/run/opengl-driver/lib''${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
              export __EGL_VENDOR_LIBRARY_DIRS="/run/opengl-driver/share/glvnd/egl_vendor.d"
            '';
          };
        });
    };
}
