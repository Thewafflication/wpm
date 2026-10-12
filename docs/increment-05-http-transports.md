# I-05 HTTP transport: compatible framing delivery

Status: partial delivery in 1.2.23. Native SMB and the broader managed-network
matrix remain open; I-05 is not declared complete.

HTTP uses the existing WinInet APIs, with repository-scoped opt-in and unchanged
origin/redirect/signature rules. Content-Length is now parsed as an unsigned
64-bit decimal value rather than a 32-bit numeric query. Consistent repeated
values are accepted; conflicting, nonnumeric, and overflowing values are
rejected by WinInet or WPM. Known zero lengths are distinguished from absent
lengths. Unknown-length and chunked responses retain their existing support.

Progress preserves the known 64-bit total, and complete received bytes must
match it before temporary publication. The original Windows error is captured
before closing handles or finishing progress so cleanup cannot obscure it.

`tests/http-length-integration.py` uses a real loopback HTTP/1.1 server with
normal, omitted, chunked, repeated, short, greater-than-4-GiB declared, malformed,
overflowing, conflicting, and zero length responses. It sends small bodies;
large framing is tested without allocating or downloading multi-GiB data.
Fixtures verify exact cached bytes and modification-time preservation,
candidate cleanup, 64-bit verbose totals, and a successful later retry.

This is an additive 1.x transport correction. Physical/managed SMB evidence,
interruptions across the platform matrix, and remaining I-05 obligations stay
open. Breaking changes remain reserved for 2.x.
