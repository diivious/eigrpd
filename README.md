EIGRP
=====

EIGRP is a portable EIGRP implementation project based on RFC 7868.

The current production host is FRR. The repository is organized so the daemon
source can be staged into an FRR checkout while project specs, tools, standalone
compile smoke tests, and portable tests stay outside the FRR daemon directory.

Project layout
--------------

```text
eigrp/
  specs/          Project design and protocol implementation notes
  tools/          Local build, install, setup, backup, and test helpers
  eigrpd/         Daemon source copied to frr/eigrpd/
  build/          Standalone compile-smoke harness
  test/
    common/       Shared fixtures and packet samples
    portable/     Python/source-level tests that do not need FRR
    frr/          FRR-native test payload copied to frr/tests/eigrpd/
    bsd/          Future BSD-hosted tests
```

FRR staging
-----------

Clone FRR and this project as siblings:

```sh
git clone https://github.com/frrouting/frr.git frr
git clone git@github.com:diivious/eigrp.git eigrp
```

Stage EIGRP into FRR:

```sh
cd eigrp
tools/install.sh -frr-root ../frr
```

This copies:

```text
eigrpd/    -> ../frr/eigrpd/
test/frr/  -> ../frr/tests/eigrpd/
```

Build FRR normally, or use the helper:

```sh
tools/build.sh config -frr-root ../frr
tools/build.sh build  -frr-root ../frr
```

Standalone compile smoke
------------------------

The standalone smoke harness catches syntax/prototype drift in selected daemon
files without requiring a full FRR build:

```sh
make -C build
```

or:

```sh
make smoke
```

This is not a replacement for the FRR build/link gate.

Tests
-----

Run portable tests:

```sh
tools/unittest.sh -portable
```

Run packet-only portable tests:

```sh
tools/unittest.sh -packet
```

Install FRR-native tests into an FRR checkout:

```sh
tools/unittest.sh -install -frr-root ../frr
```

Run FRR-native tests when `test/frr/` has a real test payload:

```sh
tools/unittest.sh -frr -frr-root ../frr
```

Debugging
---------

Stop the service-managed EIGRP daemon before launching a debug copy:

```sh
sudo systemctl stop frr
sudo systemctl start frr
sudo kill -9 "$(pidof eigrpd)" 2>/dev/null || true
```

Then, from the FRR checkout:

```sh
sudo gdb eigrpd/.libs/eigrpd
```

Useful `vtysh` smoke commands:

```sh
vtysh -c 'configure terminal' -c 'router eigrp 4453' -c 'network 0.0.0.0/0'
vtysh -c 'show running-config'
vtysh -c 'show ip eigrp topology'
vtysh -c 'show ip eigrp neighbors'
```

Contributing
------------

Small, reviewable commits are preferred. Existing author/copyright history must
be preserved when refactoring existing FRR-derived files.

Security
--------

To report security issues, email:

```text
diivious [at] hotmail.com
```
