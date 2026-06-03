# EIGRP Tools

Copyright (C) 2026 Donnie V. Savage

## `unittest.sh`

Project test runner.

Examples:

```sh
./unittest.sh -list
./unittest.sh -portable
./unittest.sh -all
./unittest.sh -cli
./unittest.sh -neighbors
./unittest.sh -frr-root /path/to/frr -cli
```

When run from `frr/eigrpd/tools`, the script infers the FRR checkout root as the parent of `eigrpd`.

FRR topotest options copy EIGRP testcases into `frr/tests/...`, then run pytest from `frr/tests/topotests`.
