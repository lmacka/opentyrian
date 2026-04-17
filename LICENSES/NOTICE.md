# Licensing Notice

This Android build of OpenTyrian ships three separate bodies of work under
three separate licenses. All are covered here.

## 1. OpenTyrian source code — GPL-2.0-or-later

The C source code in `src/` is OpenTyrian, a community port of Tyrian 2.1,
originally released under the GPL by the OpenTyrian Development Team.

- Full text: [`opentyrian-GPL-2.0.txt`](opentyrian-GPL-2.0.txt)
- Upstream: https://github.com/opentyrian/opentyrian

The Android-specific additions in this fork (`src/android_input.*`,
`android-project/`) are released under the same GPL-2.0-or-later terms.

## 2. SDL2 and SDL2_net — zlib

SDL2 (vendored at `android-project/app/jni/SDL`) and SDL2_net (vendored at
`android-project/app/jni/SDL_net`) are distributed under the zlib license by
the libsdl-org project.

- SDL2 full text: [`SDL2-zlib.txt`](SDL2-zlib.txt)
- SDL2_net full text: [`SDL2_net-zlib.txt`](SDL2_net-zlib.txt)
- Upstream: https://github.com/libsdl-org/SDL, https://github.com/libsdl-org/SDL_net

## 3. Tyrian 2.1 game data — freeware (post-commercial release)

The Tyrian 2.1 data files (`*.lvl`, `*.shp`, `*.dat`, etc.) are bundled in
the APK under `assets/tyrian/`. Their redistribution basis is **not** the
1995 Epic MegaGames EULA that ships inside `tyrian21.zip`. That original
commercial license is preserved as a historical artifact at
[`tyrian-2.1-data-ORIGINAL-1995-EULA.txt`](tyrian-2.1-data-ORIGINAL-1995-EULA.txt)
and is noted here because the data files themselves reference it.

The current redistribution basis is the post-commercial freeware release of
the Tyrian 2.1 data by the rights holder. This is documented by the upstream
OpenTyrian project in its own README, which is the package this fork relies
on:

> Tyrian 2.1 data files which have been released as freeware:
>   https://camanis.net/tyrian/tyrian21.zip
> — https://github.com/opentyrian/opentyrian README

This fork is a downstream redistributor operating on the same basis as
upstream. If you are the rights holder and wish the data removed, open an
issue at https://github.com/lmacka/opentyrian and the APK will be rebuilt
without bundled assets, requiring users to sideload the zip themselves.

## Build artifacts

The signed release APK at `OpenTyrian-release.apk` is a composite of all
three above. Distributing the APK therefore requires honouring all three
licenses: GPL-2.0-or-later for the OpenTyrian portion, zlib for the SDL
portion, and the freeware terms for the Tyrian data.
