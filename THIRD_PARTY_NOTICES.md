# Third-Party Notices

WPM includes the following third-party software.

## Mbed TLS

- Project: [Mbed TLS](https://github.com/Mbed-TLS/mbedtls)
- Version: 3.6.7, pinned commit `068ff080b369adfac81509f9b57b2afabaf82dc5`
- Copyright: The Mbed TLS Contributors
- License: Apache-2.0 OR GPL-2.0-or-later; WPM uses the GPL option as part of
  this GPL-3.0-or-later work. The upstream license is retained in
  `third_party/mbedtls/LICENSE`; WPM's GPL text is in `LICENSE.txt`.
- Bundled source: `third_party/mbedtls` (Git submodule); no upstream modifications.
- Purpose: app-local TLS 1.2 and X.509 certificate verification.

## Mozilla CA certificate data

- Source: [curl's Mozilla CA extraction](https://curl.se/docs/caextract.html)
- Snapshot: Mozilla certificate data dated 2026-08-13, downloaded 2026-09-24.
- Source form: [WPM's retained PEM and provenance](https://github.com/Thewafflication/wpm/tree/master/third_party/certificates)
- License: MPL-2.0. This Source Code Form is subject to the terms of the Mozilla
  Public License, v. 2.0. If a copy of the MPL was not distributed with this file,
  You can obtain one at https://mozilla.org/MPL/2.0/.
- Purpose: embedded HTTPS trust anchors, independent of Windows' certificate store.

No OneCoreAPI, Wine, or ReactOS source is included in this transport.

## WCRT (optional build dependency)

- Project: Waughtal C Run Time (WCRT)
- License: GPL-3.0-or-later
- Usage: TinyCC/WCRT builds link the installed static library and console
  startup object; WCRT source is not bundled in this repository
- License text: `LICENSE.txt` and the `LICENSE.txt` included with the installed
  WCRT package

---

## minizip-ng

- Project: [minizip-ng](https://github.com/zlib-ng/minizip-ng)
- License: zlib License
- Bundled source: `third_party/minizip-ng`

This software is provided 'as-is', without any express or implied
warranty. In no event will the authors be held liable for any damages
arising from the use of this software.

Permission is granted to anyone to use this software for any purpose,
including commercial applications, and to alter it and redistribute it
freely, subject to the following restrictions:

1. The origin of this software must not be misrepresented; you must not
   claim that you wrote the original software. If you use this software
   in a product, an acknowledgement in the product documentation would be
   appreciated but is not required.
2. Altered source versions must be plainly marked as such, and must not be
   misrepresented as being the original software.
3. This notice may not be removed or altered from any source distribution.

---

## zlib-ng

- Project: [zlib-ng](https://github.com/zlib-ng/zlib-ng)
- License: zlib License
- Bundled source: `third_party/zlib-ng`

Copyright 1995-2024 Jean-loup Gailly and Mark Adler

This software is provided 'as-is', without any express or implied
warranty. In no event will the authors be held liable for any damages
arising from the use of this software.

Permission is granted to anyone to use this software for any purpose,
including commercial applications, and to alter it and redistribute it
freely, subject to the following restrictions:

1. The origin of this software must not be misrepresented; you must not
   claim that you wrote the original software. If you use this software
   in a product, an acknowledgment in the product documentation would be
   appreciated but is not required.
2. Altered source versions must be plainly marked as such, and must not be
   misrepresented as being the original software.
3. This notice may not be removed or altered from any source distribution.

---

## libsodium

- Project: [libsodium](https://github.com/jedisct1/libsodium)
- License: ISC
- Bundled source: `third_party/libsodium`

Copyright (c) 2013-2024  
Frank Denis <j at pureftpd dot org>

Permission to use, copy, modify, and/or distribute this software for any  
purpose with or without fee is hereby granted, provided that the above  
copyright notice and this permission notice appear in all copies.

THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES  
WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF  
MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR  
ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES  
WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN  
ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF  
OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
