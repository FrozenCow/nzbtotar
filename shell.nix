let
  pkgs = import <nixpkgs> {};
  # The vrb library is not part of nixpkgs (yet?). We'll pull its definition from
  # pull-request https://github.com/NixOS/nixpkgs/pull/30688
  nixpkgsWithVrb = pkgs.fetchzip {
    url = https://github.com/NixOS/nixpkgs/archive/380bb673d46fbdcca42515d5a1a04c7385ff681f.tar.gz;
    sha256 = "1j8w5sjdk8vlr0m1m3bs3b2qfv26i80cwpsy6qj928yw9v59pxsw";
  };
  vrb = pkgs.callPackage "${nixpkgsWithVrb}/pkgs/development/libraries/vrb" {};
in
with pkgs;
stdenv.mkDerivation rec {
  name = "nzbtotar-${version}";
  version = "0.0.1";

  buildInputs = [ pkgconfig libxml2 expat vrb ];

  meta = with stdenv.lib; {
    description = "A lightweight volume control that sits in your systray";
    homepage = http://softwarebakery.com/maato/volumeicon.html;
    platforms = pkgs.lib.platforms.linux;
    maintainers = with maintainers; [ bobvanderlinden ];
    license = pkgs.lib.licenses.gpl3;
  };

}
