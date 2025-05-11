{
  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";
    flake-parts.url = "github:hercules-ci/flake-parts";
  };

  outputs =
    inputs@{ flake-parts, ... }:
    flake-parts.lib.mkFlake { inherit inputs; } {
      systems = [
        "x86_64-linux"
        "aarch64-linux"
        "x86_64-darwin"
        "aarch64-darwin"
      ];

      perSystem =
        { pkgs, ... }:
        let
          mkShell = pkgs.mkShell.override { stdenv = pkgs.clangStdenv; };
        in
        {
          devShells.default = mkShell {
            packages = with pkgs; [
              pkg-config
              cmake
              ninja
              gnumake
              gcc
              llvmPackages_20.clang-tools

              vulkan-utility-libraries
              vulkan-validation-layers
              vulkan-extension-layer
              vulkan-loader
              vulkan-headers

              glfw
              glm
              (imgui.override {
                IMGUI_BUILD_GLFW_BINDING = true;
                IMGUI_BUILD_VULKAN_BINDING = true;
                IMGUI_BUILD_OPENGL3_BINDING = false;
              })
              vulkan-memory-allocator

              wayland-scanner
              wayland
              libxkbcommon
              xorg.libX11
              xorg.libXcursor
              xorg.libXi
              xorg.libXrandr
              xorg.libXft
              xorg.libXinerama

              vulkan-tools-lunarg
              renderdoc
              glslang
            ];

            VULKAN_SDK = "${pkgs.vulkan-headers}";

            LD_LIBRARY_PATH =
              with pkgs;
              lib.makeLibraryPath [
                vulkan-loader
                vulkan-tools-lunarg
                xorg.libX11
                wayland
              ];

            VK_LAYER_PATH =
              with pkgs;
              (lib.makeSearchPathOutput "lib" "share/vulkan/explicit_layer.d" [
                vulkan-tools-lunarg # For VK_LAYER_LUNARG_api_dump
                vulkan-validation-layers # For VK_LAYER_KHRONOS_validation
                vulkan-extension-layer # for VK_LAYER_KHRONOS_shader_object
              ])
              + ":"
              + (lib.makeSearchPathOutput "lib" "share/vulkan/implicit_layer.d" [
                renderdoc
              ]);
          };
        };
    };
}
