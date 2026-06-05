# SPDX-License-Identifier: ISC
#
# Copyright (C) 2026 Donnie V. Savage
#
# Source-level guards for EIGRP named-mode CLI direction.

from pathlib import Path


ROOT = Path(__file__).resolve().parents[4]
CLI = ROOT / "eigrpd" / "eigrp_cli.c"
SPEC = ROOT / "specs" / "cli-spec.md"


def read(path: Path) -> str:
    return path.read_text()


def function_body(source: str, name: str) -> str:
    start = source.index(f"{name}(void)")
    end = source.index("\n}\n", start) + 3
    return source[start:end]


def test_named_router_command_is_installed_instead_of_classic_router_command():
    cli = read(CLI)
    init = function_body(cli, "eigrp_cli_init")

    assert "install_element(CONFIG_NODE, &router_eigrp_named_cmd);" in init
    assert "install_element(CONFIG_NODE, &no_router_eigrp_named_cmd);" in init
    assert "install_element(CONFIG_NODE, &router_eigrp_cmd);" not in init
    assert "install_element(CONFIG_NODE, &no_router_eigrp_cmd);" not in init
    assert '"router eigrp WORD"' in cli
    assert '"address-family ipv4 [unicast] [vrf NAME] autonomous-system (1-65535)"' in cli


def test_classic_interface_commands_are_not_installed():
    init = function_body(read(CLI), "eigrp_cli_init")

    classic_installs = {
        "eigrp_if_ip_hellointerval_cmd",
        "eigrp_if_ip_holdinterval_cmd",
        "eigrp_ip_summary_address_cmd",
        "eigrp_authentication_mode_cmd",
        "eigrp_authentication_keychain_cmd",
    }

    for command in classic_installs:
        assert command not in init


def test_named_af_interface_commands_are_installed():
    init = function_body(read(CLI), "eigrp_cli_init")

    named_commands = {
        "eigrp_exit_af_interface_cmd",
        "eigrp_af_interface_hello_interval_cmd",
        "eigrp_af_interface_hold_time_cmd",
        "eigrp_af_interface_bandwidth_percent_cmd",
        "eigrp_af_interface_summary_address_cmd",
        "eigrp_af_interface_authentication_mode_cmd",
        "eigrp_af_interface_keychain_cmd",
        "eigrp_no_shutdown_cmd",
    }

    for command in named_commands:
        assert f"install_element(EIGRP_NODE, &{command});" in init

    assert "install_element(INTERFACE_NODE" not in init


def test_not_configured_stubs_are_explicit():
    cli = read(CLI)

    assert "eigrp_cli_not_configured" in cli
    assert "address-family ipv6" in cli
    assert "bandwidth-percent" in cli
    assert "summary-address" in cli
    assert "split-horizon" in cli
    assert "distance eigrp" in cli
    assert "offset-list" in cli
    assert "summary-metric" in cli


def test_cli_spec_documents_named_mode_cli_direction():
    spec = read(SPEC)

    assert "Named-Mode CLI Direction" in spec
    assert "router eigrp <name>" in spec
    assert "address-family ipv4 unicast" in spec
    assert "Classic configuration commands must not be installed" in spec
