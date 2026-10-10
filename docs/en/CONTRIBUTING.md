# Contributing to Bubba Machine

Thanks for wanting to help. Bubba Machine is free firmware for the M-VAVE FM-1 and every bit of help counts: testing in the emulator, reporting bugs, improving the manual, designing sounds or writing code.

You can write in **English or Spanish**.

## Ways to help

- **Test and report.** Use the web emulator and open an *issue* with what happened, what you expected and how to reproduce it. Screenshots help a lot.
- **Interface ideas.** Open an *idea* issue. The current direction is in [`DESIGN_3.0.md`](DESIGN_3.0.md).
- **Documentation.** The manual lives in [`docs/en/MANUAL.md`](MANUAL.md) (and the Spanish one in `docs/MANUAL.md`); the website is generated from the `.md` files.
- **Code.** Read [`DEVELOPMENT.md`](DEVELOPMENT.md) to build and test.
- **Hardware.** If you have an FM-1 and experience with the JieLi SDK, the port in [`hal/fm1/`](../../hal/fm1/README_PORT.md) is what needs help the most.

## Code rules

1. **All code is our own.** Do not copy, translate or paraphrase code from other FM-1 firmwares (including Felucca). Hardware facts (pins, timings, documented registers) can be used, citing the source in `docs/SPEC_V3.md`.
2. **Third-party code only with a GPL-3.0-compatible license**, and listed in the [credits](CREDITS.md).
3. **Sounds:** only synthesized in `tools/`, made by you, or under a free compatible license (CC0, CC-BY or GPL). No samples from records or commercial packs. To test with third-party audio use `assets/loops/loops.local.json`, which is never pushed.
4. **Dependency-free C99** in `core/`: no `malloc`, no floating point in the audio path and nothing that depends on the hardware. Everything hardware-related goes in `hal/`.
5. **Every new file** starts with the license identifier:
   ```c
   /* SPDX-License-Identifier: GPL-3.0-or-later
    * Copyright (C) 2026 Your Name */
   ```

## Before opening a pull request

Pull requests go to the **`dev`** branch. From there changes move to `qa` to be tested and then to `main`, which is what is released (see [branches](DEVELOPMENT.md#branches)).

- [ ] `py -3 tools/build.py test` passes with no failures.
- [ ] If you changed the interface, the screenshots (`assets/screens/`) and the manual are updated.
- [ ] If you changed the pattern or flash format, it is explained in the PR and in the [changelog](CHANGELOG.md).
- [ ] If you added anything third-party, it is in the [credits](CREDITS.md).

## License of contributions

By submitting a contribution you agree that it is published under the project's license, **GPL-3.0-or-later**. We recommend signing your commits with `git commit -s` ([Developer Certificate of Origin](https://developercertificate.org/)) to record that you have the right to contribute that code.

## Conduct

This project follows a [code of conduct](CODE_OF_CONDUCT.md).
