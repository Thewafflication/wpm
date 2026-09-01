# WPM 2.0 CP-01B Operand-Shape Change Impact

**Content type:** Controlled change-impact analysis

**Status:** Accepted

**Change:** Validate simple command operand shapes before initialization

**Owner and date:** WPM maintainers, 2026-09-01

This increment validates minimum and maximum positional operands for init,
build, verify, install, remove, keygen, and update after recognized options are
excluded. Invalid shapes identify the problem and command, render one shared
usage line, point to command help, and return before data or log initialization.
Execution behavior and all persistent formats are unchanged.

TC-0029 covers every implemented boundary and the no-state condition. Nested
repo/key/trust/config action grammar and upgrade combination semantics remain
in their existing command validators and are pending broader TC-0014 closure.
