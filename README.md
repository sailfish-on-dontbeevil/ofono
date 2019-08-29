# ofono
`ofono` fork with QMI modem support for the PinePhone.

This fork enable QMI modem support in the Sailfish OS fork of `ofono` (https://git.sailfishos.org/mer-core/ofono), by default it's disabled in the `.spec` file.
Also several patches are applied to make voicecalls possible with the QMI modem of the PinePhone.

## Patches
Cherry picked several patches from https://git.sysmocom.de/ofono/log/?h=voicecall to enable voicecall support for QMI modems in `ofono`.

- [[RFC] qmimodem: implement voice calls](https://git.sysmocom.de/ofono/commit/?h=voicecall&id=0bfb7039c9cb821aefb5e1cacc252172b8c80f1d)
- [common,atmodem: rename & move at_util_call_compare_by_id to common.c](https://git.sysmocom.de/ofono/commit/?h=voicecall&id=3bba30fd23705dc8817b2eb0f28c9be03b8f7892)
- [common,atmodem: rename & move at_util_call_compare_by_status to common.c](https://git.sysmocom.de/ofono/commit/?h=voicecall&id=4c71f0ca71c74987523c68764df28840ccd3882e)
- [add call-list helper to manage voice call lists](https://git.sysmocom.de/ofono/commit/?h=voicecall&id=2ae2366c262a15e4f3269afd9c80ddf06c1b9b46)
- [common: create GList helper ofono_call_compare](https://git.sysmocom.de/ofono/commit/?h=voicecall&id=c97a48fd4b94d4e6785dd713abe2a06da5e0d623)

## OBS build

This package is build on Mer OBS: https://build.merproject.org/package/show/nemo:devel:hw:pine:dontbeevil/ofono
