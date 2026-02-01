{
  description = "Flake for kew - a command-line music player for Linux";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";
    kew-src = {
      url = "github:ravachol/kew";
      flake = false;
    };
  };

  outputs = { self, nixpkgs, kew-src }:
    let
      supportedSystems = [ "x86_64-linux" "aarch64-linux" ];
      forAllSystems = nixpkgs.lib.genAttrs supportedSystems;
    in
    {
      packages = forAllSystems (system:
        let
          pkgs = nixpkgs.legacyPackages.${system};
        in
        with nixpkgs.lib;
        {
          default = pkgs.stdenv.mkDerivation (finalAttrs: 
            let
              uppercaseFirst = x: 
                (toUpper (substring 0 1 x)) + (substring 1 ((strings.stringLength x) - 1) x);
            in
            {
              pname = "kew";
              version = kew-src.shortRev or "dev";
              src = kew-src;
              
              postPatch = ''
                substituteInPlace Makefile \
                  --replace-fail '$(shell uname -s)' '${uppercaseFirst pkgs.stdenv.hostPlatform.parsed.kernel.name}' \
                  --replace-fail '$(shell uname -m)' '${pkgs.stdenv.hostPlatform.parsed.cpu.name}'
              '';
              
              nativeBuildInputs = with pkgs; [
                pkg-config
              ] ++ optionals pkgs.stdenv.hostPlatform.isLinux [
                autoPatchelfHook
              ];
              
              buildInputs = with pkgs; [
                fftwFloat.dev
                chafa
                curl.dev
                gdk-pixbuf
                glib.dev
                libopus
                opusfile
                libvorbis
                taglib
                faad2
                libogg
              ];
              
              runtimeDependencies = with pkgs; [
                libpulseaudio
                alsa-lib
              ];
              
              enableParallelBuilding = true;
              
              installFlags = [
                "MAN_DIR=${placeholder "out"}/share/man"
                "PREFIX=${placeholder "out"}"
              ];
              
              nativeInstallCheckInputs = [ pkgs.versionCheckHook ];
              versionCheckProgramArg = "--version";
              doInstallCheck = true;
              
              meta = {
                description = ''
                  Command-line music player for Linux
                '';
                homepage = "https://github.com/ravachol/kew";
                platforms = platforms.unix;
                license = licenses.gpl2Only;
                mainProgram = "kew";
              };
            });
        }
      );

      overlays.default = final: prev: {
        kew = self.packages.${prev.system}.default;
      };
    };
}
