# WPM Support Policy

## Scope

WPM is maintained on a best-effort community basis. This policy applies to WPM
itself; package contents and install scripts published by third parties are the
responsibility of their respective maintainers.

## Supported release

Only the latest published WPM release is supported. Users should upgrade before
reporting an issue.

WPM targets supported Windows 10 and Windows 11 installations on x86, x64, and
ARM64 where the corresponding release asset is published.

## What maintainers may address

Maintainers may address reproducible defects, security vulnerabilities, and
release-packaging problems in the latest release. There is no guaranteed
response time, fix timeline, or backport commitment.

## Compatibility

WPM follows Semantic Versioning for its public releases. Within a 1.x release
line, breaking changes to documented command-line behavior, package metadata,
repository-index format, or package-signature format will be avoided where
practical. If such a change is necessary, it will be called out in the release
notes.

## Reporting an issue

Include the output of `wpm --version --verbose`, the command used, relevant
diagnostic output, and enough information to reproduce the issue. Never include
private signing keys or credentials.
