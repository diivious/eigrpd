# EIGRP Standalone Compile Smoke

This directory contains a narrow compile-only harness for selected `eigrpd/`
files that are most likely to break during CLI, VTY, auth, and debug command
changes.

It uses local FRR stub headers under `build/include/` and compiles selected
source files with `-fsyntax-only`. It does not link, does not run FRR clippy,
and is not a replacement for copying `eigrpd/` into `frr/eigrpd/` and running
the full FRR build.

Run from the repository root:

```sh
make -C build
```

or:

```sh
make smoke
```

Optional CLI-focused alias:

```sh
make -C build cli
```

To see the files covered by the smoke harness:

```sh
make -C build list
```

There is also an experimental full-source syntax pass:

```sh
make -C build all-sources
```

`all-sources` is expected to fail until the local FRR stub headers are expanded
enough to cover the full daemon. The default `smoke` target stays intentionally
narrow so it remains useful during ordinary cleanup.
