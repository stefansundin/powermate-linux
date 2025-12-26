{
  description = "NixOS package for powermate";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";
    powermate = {
      type = "git";
      url = "https://github.com/stefansundin/powermate-linux.git";
      submodules = true;
      flake = false;
    };
  };

  outputs = { self, nixpkgs, powermate }:
    let
      supportedSystems = [ "x86_64-linux" "aarch64-linux" ];
      forAllSystems = nixpkgs.lib.genAttrs supportedSystems;
    in
    {
      packages = forAllSystems (system:
        let pkgs = import nixpkgs { inherit system; }; in
        {
          default = pkgs.stdenv.mkDerivation {
            pname = "powermate";
            version = "8";
            src = powermate;

            nativeBuildInputs = [ pkgs.pkg-config ];
            buildInputs = [ pkgs.libpulseaudio ];

            makeFlags = [ "DESTDIR=$(out) PREFIX='' " ];
          };
        });
    };
}