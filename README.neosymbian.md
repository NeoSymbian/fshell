<!-- SPDX-License-Identifier: Apache-2.0 -->
# NeoSymbian debug-access adaptations

This fork starts at SymbianSource commit `30d5fcf516eed5ae7a5379a6a469c75f93a1345b`.
The NeoSymbian build system selects a small text-shell configuration. It retains
the native console and file-server APIs; it does not replace `econs.dll`.

## Native interfaces

```text
hash --algorithm SHA256 --input FILE --hex
ymodem receive --output DEST --sha256 HEX [--overwrite]
ymodem send FILE
```

YMODEM modes are positional arguments. The receiver decodes a strict UTF-8
basename from bounded 128-byte or 1 KiB block-zero metadata. Data is binary;
the sender uses 1 KiB blocks and terminates the batch with an empty 128-byte
header. The native `RFile` size limit is 2,147,483,647 bytes.

Single-file reception writes a sibling temporary file, flushes it, re-reads it
through F32 to verify SHA-256, and closes it. The receiver validates the empty
batch terminator before renaming the temporary file into place. Replacement
requires `--overwrite`; protocol, validation and storage errors return a
nonzero command result. An EOT ACK alone does not establish successful
finalization: callers must also check the shell's command completion status.
Loss of that final status leaves the caller uncertain whether finalization
completed. No automatic transfer resume or power-loss atomicity is promised.

The direct serial console accepts optional `waitdcd=1` and `sizedetect=0|1` title fields:

```text
fshell --console vt100busdevcons --console-title pdd=euart1,ldd=ecomm,port=1,rate=115200,waitdcd=1,sizedetect=0
```

It subscribes to carrier changes before checking the current state, waits before
terminal initialization, and then obeys/fails DCD. The USB PDD must map host DTR
to DCD. Disconnect ends the shell; the user-side supervisor starts a fresh
session. The upstream default (`waitdcd=0`) remains unchanged.

The dedicated machine console uses `sizedetect=0` so terminal initialization
cannot issue DSR cursor-position requests or consume the first command as an
invalid terminal reply. This title setting disables detection even if a writable
`vt100.ini` enables `console_size_detect`. Omission defaults to `sizedetect=1`,
which preserves the normal upstream INI-controlled behavior for other users.

SHA-256 requires the native software `hash.dll`, fshell crypto/SHA2 support,
and `hash.lib` in the shell's import set. External command names can use the
build emitter's short ROM aliases (`HASH.EXE`, `BASE64.EXE`); native command
resolution falls back from the `fshell_` prefix to the bare command name.

## Focused host checks

```sh
c++ -std=c++03 -Wall -Wextra -Werror -pedantic tests/host/ymodem_meta.cpp -o /tmp/ymodem-meta
/tmp/ymodem-meta
c++ -std=c++03 -Wall -Wextra -Werror -pedantic -fsanitize=address,undefined -fno-omit-frame-pointer tests/host/ymodem_meta.cpp -o /tmp/ymodem-meta-san
/tmp/ymodem-meta-san
```

These checks exercise metadata bounds, malformed UTF-8, path syntax, Unicode
names and size overflow. They do not validate the Symbian file-server,
serial-driver, transfer state machine or carrier lifecycle. Those require the
pinned target build and QEMU/phone acceptance in the workspace debug-access
plan.
