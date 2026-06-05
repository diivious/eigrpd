# EIGRP Tools

These scripts support the project layout where `eigrpd/` is staged into an FRR
checkout and project-only material stays in the EIGRP repository.

## Scripts

```text
backup.sh     Create a clean zip of the project.
build.sh      Run standalone smoke, stage into FRR, configure, build, or check.
install.sh    Replace frr/eigrpd/ with eigrpd/ and copy test/frr/ to frr/tests/eigrpd/.
setup.sh      Debian development-machine setup helper.
unittest.sh   Run portable tests and FRR-native tests when present.
```

## Common commands

```sh
# Stage source into FRR. This removes the existing ~/devel/frr/eigrpd first.
tools/install.sh -frr-root ~/devel/frr

# Run standalone compile smoke
make -C build

# Configure and build FRR after staging EIGRP
tools/build.sh config -frr-root ~/devel/frr
tools/build.sh build  -frr-root ~/devel/frr

# Run portable tests
tools/unittest.sh -portable

# Make a clean delivery zip
tools/backup.sh -z eigrp.zip
```

## Local FRR service commands

```sh
sudo systemctl restart frr
sudo systemctl status frr
```

## Handy `vtysh` commands

```sh
vtysh -c 'show running-config'
vtysh -c 'configure terminal' -c 'service integrated-vtysh-config' -c 'end' -c 'write memory'
vtysh -c 'debug eigrp packets hello' -c 'debug eigrp packets update' -c 'debug eigrp transmit send' -c 'debug eigrp transmit recv'
vtysh -c 'configure terminal' -c 'router eigrp 4453' -c 'network 10.0.0.0/8' -c 'network 172.16.0.0/24' -c 'network 192.168.1.0/24'
```

## Test topology notes

```text
                         NAT to Host
                             |
                          (enp0s3)
                             |
RTR1(E1) ---- (enp0s8) FRR (enp0s9) ---- (E2)RTR2
                             |
                          (enp0s10)
```

FRR sample interfaces:

```text
enp0s3:  N/A
enp0s8:  10.0.0.250/8
enp0s9:  172.16.0.250/20
enp0s10: 192.168.1.250/24
```
