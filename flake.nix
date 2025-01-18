{
  inputs = {
    nixpkgs.url = "github:nixos/nixpkgs?ref=nixos-unstable";
  };

  outputs = { self, nixpkgs }:
    let
      pkgs = import nixpkgs { system = "x86_64-linux"; };
      platformioInputs = with pkgs; [
        platformio
        gcc-arm-embedded
        dfu-util
        cmake
        gnumake
      ];
    in
    {
      packages.x86_64-linux.rtl_433_f007th_prometheus = pkgs.buildGoModule {
        pname = "rtl_433_f007th_prometheus";
        version = "local";
        src = ./rtl_433_f007th_prometheus;
        vendorHash = "sha256-/6+qz4r3pNwLi1uGPaykecfC10IIpeq3O9XZTBtmaoI=";
      };

      packages.x86_64-linux.upload-stm32 = pkgs.writeShellApplication
        {
          name = "upload-bluepill";
          runtimeInputs = platformioInputs;
          text = ''
            pio run -e bluepill_f103c8_128k -t upload "$@"
          '';
        };

      devShells.x86_64-linux.default = pkgs.mkShell {
        buildInputs = platformioInputs ++ (with pkgs;
          [
            go
            gopls
          ]);
      };
    };
}
