#!/usr/bin/env python3
"""
Generate bcl_vss_paths.hpp from a merged VSS JSON export.

Usage:
    generate_header.py <merged.json> <output.hpp>

The script walks the nested JSON produced by:
    vspec export json --pretty --vspec bcl_lighting.vspec \
        --overlays bcl_extension.vspec --output merged.json

and emits one constexpr std::string_view constant per leaf signal.

Naming convention: dots removed, each component UpperCamelCase, prefixed with k.
Example:
  Vehicle.Body.Lights.DirectionIndicator.Left.IsSignaling
  -> kVehicleBodyLightsDirectionIndicatorLeftIsSignaling
"""

import json
import sys
import os
from datetime import datetime, timezone


def _to_constant_name(path: str) -> str:
    return "k" + path.replace(".", "")


def _collect_leaves(node: dict, path: str, out: list) -> None:
    if "children" in node:
        for child_name, child_node in node["children"].items():
            child_path = f"{path}.{child_name}" if path else child_name
            _collect_leaves(child_node, child_path, out)
    elif node.get("type") not in ("branch", None):
        out.append(path)


def main() -> int:
    if len(sys.argv) != 3:
        print(f"Usage: {sys.argv[0]} <merged.json> <output.hpp>", file=sys.stderr)
        return 1

    json_path = sys.argv[1]
    hpp_path = sys.argv[2]

    with open(json_path, encoding="utf-8") as f:
        data = json.load(f)

    if "Vehicle" not in data:
        print(f"ERROR: no 'Vehicle' root in {json_path}", file=sys.stderr)
        return 1

    leaves: list = []
    _collect_leaves(data["Vehicle"], "Vehicle", leaves)
    leaves.sort()

    os.makedirs(os.path.dirname(hpp_path), exist_ok=True)

    ts = datetime.now(timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ")
    lines = [
        "// AUTO-GENERATED — do not edit. Regenerate via CMake VSS target.",
        f"// Generated: {ts}",
        "// Source: vss/spec/bcl_lighting.vspec + vss/spec/bcl_extension.vspec",
        "//",
        "// SPDX-License-Identifier: MPL-2.0",
        "",
        "#pragma once",
        "#include <string_view>",
        "",
        "namespace body_control::lighting::vss::paths {",
        "",
    ]
    for path in leaves:
        name = _to_constant_name(path)
        lines.append(f'constexpr std::string_view {name} = "{path}";')

    lines += [
        "",
        "}  // namespace body_control::lighting::vss::paths",
        "",
    ]

    with open(hpp_path, "w", encoding="utf-8") as f:
        f.write("\n".join(lines))

    print(f"[vss] Generated {len(leaves)} signal path constants -> {hpp_path}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
