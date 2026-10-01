"""Validate owned JSON/YAML syntax and reject duplicate mapping keys.

Run from the repository root. Configuration files are read without changes;
parse errors terminate the process and name the offending file.
"""

import json
from pathlib import Path

import yaml


def unique_pairs(pairs):
    """Return a mapping; raise ValueError when a key occurs twice."""
    result = {}
    for key, value in pairs:
        if key in result:
            raise ValueError(f"Duplicate configuration key: {key}")
        result[key] = value
    return result


class UniqueLoader(yaml.SafeLoader):
    """Safe YAML loader that validates mapping-key uniqueness."""


def construct_mapping(loader, node):
    """Construct YAML mappings while checking the original key inventory."""
    return unique_pairs(
        (loader.construct_object(key), loader.construct_object(value))
        for key, value in node.value
    )


UniqueLoader.add_constructor(
    yaml.resolver.BaseResolver.DEFAULT_MAPPING_TAG, construct_mapping
)


def main():
    """Validate the controlled configuration inventory; fail on any error."""
    paths = [Path("CMakePresets.json"), Path(".pre-commit-config.yaml")]
    paths.extend(Path(".github").rglob("*.yml"))
    paths.extend(Path("documentation").rglob("*.json"))
    for path in paths:
        try:
            text = path.read_text(encoding="utf-8-sig")
            if path.suffix == ".json":
                json.loads(text, object_pairs_hook=unique_pairs)
            else:
                yaml.load(text, Loader=UniqueLoader)
        except (ValueError, yaml.YAMLError) as error:
            raise ValueError(f"{path}: {error}") from error
    print(f"Configuration syntax passed for {len(paths)} files.")


if __name__ == "__main__":
    main()
