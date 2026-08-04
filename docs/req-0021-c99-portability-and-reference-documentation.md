# REQ-0021: C99 Portability and Reference Documentation

**Content type:** Project requirements

**Status:** Proposed

**Source:** WPM 2.0 roadmap, Milestone 8

## Scope

Applies to project-owned C source and headers, portable-core boundaries,
Windows platform helpers, compilation checks, public interface documentation,
and generated API/reference documentation.

## Requirement

**REQ-0021.001**
The WPM portable core shall conform to ISO C99 and shall not depend on C11-only
language features or library interfaces. Platform or toolchain compatibility
adapters may provide required behavior outside the core when their boundary
and assumptions are documented and tested.

**REQ-0021.002**
Direct Windows API use shall be isolated behind narrow platform helpers for
filesystem, path, process, console, network/transport, environment,
authorization, and other operating-system behavior. Portable modules shall not
depend on Windows types or headers except through an approved unavoidable
boundary recorded by inspection.

**REQ-0021.003**
CI shall compile the portable core in a controlled C99 configuration in
addition to the supported Windows release builds. Newly introduced warnings
shall be treated as errors, and the C99 gate shall fail on a C11-only construct,
an undeclared platform dependency, or a missing required portable-core source.

**REQ-0021.004**
Every project-owned public module, function, structure, constant, enumeration,
and data format shall have Doxygen-compatible `/** ... */` documentation that
identifies purpose, parameters, return and error behavior, ownership and
lifetime, mutability, thread or process assumptions where relevant, and
security or trust-boundary effects.

**REQ-0021.005**
Project-owned module headers and security-sensitive or non-obvious algorithms
shall document invariants, resource bounds, validation order, failure behavior,
and relationships to controlled requirements or DFS threats. Local
implementation comments shall remain concise and shall not duplicate an
authoritative public contract.

**REQ-0021.006**
The documentation build shall generate API/reference documentation from the
controlled source without warnings when the documentation toolchain is
available. CI shall publish the generated reference as an artifact and shall
fail the applicable documentation gate for broken references, malformed
documentation, or missing required public-entity coverage.

**REQ-0021.007**
Portability refactoring shall preserve observable CLI behavior, package and
repository formats, signature/trust validation, secure path containment,
supported architectures, and declared supported-Windows behavior. A platform
helper shall not weaken input bounds, error propagation, cleanup, secret
handling, or diagnostic redaction.

## Rationale

A C99 portable core reduces compiler and platform coupling while narrow
Windows adapters make unavoidable operating-system behavior reviewable.
Generated interface documentation captures ownership and security contracts
that are otherwise easy to violate during the broad 2.0 work.

## Verification

**Method:** Compilation, static inspection, automated coverage check, generated
documentation, and regression test

**References:** Planned TC-0021; `docs/traceability-2.0.md`

Planned verification uses a strict C99 compiler configuration, a platform
dependency boundary check, supported Windows builds/tests on x86, x64, and
ARM64, Doxygen warning-as-error generation, negative documentation fixtures,
and inspection of public and security-sensitive contracts.

## Relationships

- **Derived from:** `docs/roadmap-2.0.md` Milestone 8 and the selected WSP C
  source-style profile.
- **Depends on:** REQ-0001 through REQ-0020 implementation interfaces,
  WSP-CSTYLE-0001 through WSP-CSTYLE-0005, WSP-TEST-0012, and WSP-DOC-0008.
- **Conflicts with:** None. Windows remains the supported runtime; portability
  describes the core and does not create a non-Windows product support claim.

## Change Impact

This requirement can touch every project-owned C module, build configuration,
CI workflow, header contract, and reference-documentation artifact. Refactoring
risk includes ABI/API drift, legacy compiler differences, path and process
behavior, warning churn, and accidental weakening of security checks. Changes
should be sliced after shared 2.0 interfaces stabilize and verified against
the full existing regression matrix.

## Tailoring

An unavoidable Windows dependency in otherwise portable logic requires an
approved, documented boundary and compensating test; it does not permit silent
use of C11-only features in the portable core.

## Implementation Record

Planned allocation is a portable-core target plus narrow Windows platform
modules, strict C99 CI configuration, documentation coverage validator, and
Doxygen artifact build. TC-0021 will provide compilation, boundary, generation,
and regression evidence. No portability or documentation-completion claim is
made by this proposed baseline.
