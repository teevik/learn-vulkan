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
        {
          devShells.default = pkgs.mkShell {
            packages = with pkgs; [
              pkg-config
              cmake
              ninja
              gnumake
              gcc
              llvmPackages_20.clang-tools

              vulkan-utility-libraries
              vulkan-validation-layers
              vulkan-loader
              vulkan-headers

              # simdjson
              # SDL2
              glfw
              glm
              imgui
              vulkan-memory-allocator

              xorg.libX11.dev
              xorg.libXft
              xorg.libXinerama

              vulkan-tools-lunarg
            ];

            VULKAN_SDK = "${pkgs.vulkan-headers}";

            LD_LIBRARY_PATH =
              with pkgs;
              lib.makeLibraryPath [
                stdenv.cc.cc

                # # vulkan-validation-layers
                # vulkan-headers
                # wayland
                # vulkan-loader
                # xorg.libX11
                # xorg.libXcursor
                # xorg.libXi
                # xorg.libXrandr
                # libxkbcommon

                vulkan-loader
                vulkan-tools-lunarg
              ];

            VK_LAYER_PATH = pkgs.lib.makeSearchPathOutput "lib" "share/vulkan/explicit_layer.d" [
              pkgs.vulkan-tools-lunarg # For VK_LAYER_LUNARG_api_dump
              pkgs.vulkan-validation-layers # For VK_LAYER_KHRONOS_validation
            ];

          };
        };
    };
}
