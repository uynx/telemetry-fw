{ pkgs, inputs, ... }:
{
  packages = [ pkgs.gcc-arm-embedded pkgs.qemu pkgs.gnumake pkgs.gcc pkgs.python3 ];
  env.FREERTOS = "${inputs.freertos}";
}
