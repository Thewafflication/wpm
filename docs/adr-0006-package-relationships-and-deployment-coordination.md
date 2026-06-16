# ADR-0006: Package Relationships and Deployment Coordination

Status: Proposed

## Context

Traditional package managers focus on dependency graph resolution and package
ownership.

WPM focuses on deployment coordination.

Packages are deployment units rather than filesystem ownership units.

## Core Principle

WPM SHALL coordinate package deployments.

WPM SHALL NOT attempt to implement a complex SAT-based dependency solver.

## Relationship Types

### Depends

Indicates a required package.

Example:

depends=dotnet-runtime>=8.0

The dependency must be installed before deployment continues.

### Recommends

Indicates a suggested package.

Example:

recommends=sql-management-studio

Installation may proceed without recommended packages.

### Conflicts

Indicates incompatible packages.

Example:

conflicts=legacy-runtime

Conflicting packages SHALL block installation.

### Bundle

Used by meta-packages.

Example:

bundle=postgresql
bundle=nginx
bundle=php

Bundles define deployment groups.

## Meta Packages

Meta-packages coordinate installation of multiple packages.

A meta-package may contain little or no deployable content.

Example:

web-stack
├── bundle=postgresql
├── bundle=nginx
└── bundle=php

Installing the meta-package causes WPM to deploy all bundled packages.

## Deployment Context

When a deployment begins, WPM SHALL create a deployment context.

Example:

C:\ProgramData\WPM\contexts\<deployment-id>\

Environment variable:

WPM_CONTEXT

SHALL contain the context path.

## Context Data

Context data MAY be exchanged between packages.

Example:

postgresql install.cmd writes:

{
  "database_host": "localhost",
  "database_port": 5432
}

nginx install.cmd reads the same information.

This enables package cooperation without hardcoded package coupling.

## Deployment Order

WPM SHALL:

1. Verify packages
2. Resolve package relationships
3. Build deployment plan
4. Create deployment context
5. Execute deployment steps
6. Record results

## Multiple Versions

Multiple versions MAY coexist.

Relationship resolution SHALL select the highest compatible version unless
explicitly overridden.

## Failure Handling

Deployment failures SHALL stop the deployment plan.

Future versions MAY support rollback and recovery workflows.

## Future Enhancements

Future versions MAY support:

- Deployment profiles
- Feature selection
- Virtual packages
- Capability providers
- Rollback orchestration
- Deployment templates

## Consequences

Benefits:

- Simple dependency model
- Enterprise deployment support
- Package cooperation
- Flexible software packaging
- Reduced dependency complexity

Tradeoffs:

- Less automation than advanced dependency solvers
- More package maintainer responsibility

WPM prioritizes coordinated deployment over dependency graph optimization.
