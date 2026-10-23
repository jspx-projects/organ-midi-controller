# Licensing

This project is **open source / open hardware with strong reciprocity**
(copyleft). Everyone can use, modify, manufacture and sell it, but if you
distribute a modified version, you must share your changes under the same
license and keep the notices.

That is the difference from a permissive license such as MIT or Apache-2.0:
those would also allow a company to take the code or design, modify it, and ship
a closed product without contributing anything back. Copyleft is the standard
answer when "reuse is welcome, free-riding is not" is the goal.

## Which license applies to what

| Part | License | SPDX identifier | File |
|---|---|---|---|
| Firmware and software (`firmware/`, including the test sketches) | GNU General Public License v3.0 or later | `GPL-3.0-or-later` | [`LICENSE`](../LICENSE) |
| Hardware design files (`hardware/schematics/`, PCB/board layouts) | CERN Open Hardware Licence v2 — Strongly Reciprocal | `CERN-OHL-S-2.0` | [`LICENSE-HARDWARE`](../LICENSE-HARDWARE) |
| Documentation and measurement captures (`docs/`, `README.md`) | Creative Commons Attribution-ShareAlike 4.0 International | `CC-BY-SA-4.0` | [`LICENSE-DOCS`](../LICENSE-DOCS) |

Photos, when added, will be licensed under CC-BY-SA-4.0 as well. The prototype
photos have been removed from the repository for privacy reasons.

## What a company may do

* **Use it** internally, including in commercial products.
* **Modify it** for its own use.
* **Manufacture and sell** hardware based on the design.
* **Charge money** for hardware, assembly, support or services.
* **Distribute** the original or a modified version, provided it complies with
  the license.

## What a company may not do

* Take the firmware or hardware design, modify it, and distribute the result as
  a **closed/proprietary** product.
* Distribute modified firmware without making the corresponding source
  available under GPL-3.0-or-later.
* Manufacture/distribute a modified hardware design without making the modified
  design files available under CERN-OHL-S-2.0.
* Remove copyright, license or attribution notices.
* Use the documentation or photos without attribution and share-alike.

## What copyleft does *not* do

A license cannot force a company to contribute if it only uses the project
internally and never distributes it. Copyleft triggers when the covered work (or
a modified version) is **conveyed/distributed**, or — for AGPL — when it is
offered as a network service. This project uses GPL, not AGPL, because the
controller is embedded firmware, not a network service.

It also cannot stop a company from building a product around the project; it
only requires that the covered firmware/design and modifications stay open.

## Third-party material

Some files are **not** covered by the project licenses:

* `docs/reference/gino-midi/*` — GINO-MIDI manuals, copyright GINO Electronics.
* `docs/reference/datasheets/*` — manufacturer datasheets and application notes.

These are included for reference under their original terms. See
[`reference/README.md`](reference/README.md).

## Why not MIT or Apache-2.0?

MIT and Apache-2.0 are excellent licenses if the goal is maximum adoption with
minimum obligations. They allow a company to ship a closed derivative without
contributing back. Your stated goal — "everybody can reuse it, but companies
cannot just steal it and never contribute" — is exactly what copyleft is for.

Apache-2.0 adds an explicit patent grant, but it is still permissive. GPL-3.0
also has a patent grant and adds the reciprocal requirement.

## Why three licenses instead of one?

Software, hardware designs and documentation are different kinds of work:

* GPL is written for software;
* CERN-OHL-S is written for hardware designs (schematics, PCB layouts,
  manufacturing files);
* CC BY-SA is written for documentation and creative works.

Using the appropriate license for each part is clearer than forcing one license
onto everything. The obligations are the same in spirit: share modifications,
keep notices, no closed derivatives.

## Commercial use

Commercial use is allowed. If you are a company and want to integrate the
project into a product, the practical requirements are:

1. Keep the copyright/license notices.
2. If you modify the firmware and ship it, publish the modified firmware source
   under GPL-3.0-or-later.
3. If you modify the hardware design and manufacture it, publish the modified
   design files under CERN-OHL-S-2.0.
4. Attribute the documentation/photos and share adaptations under CC BY-SA 4.0.

If you need a different arrangement (for example, a proprietary license),
contact the author.

---

*This is a plain-language summary, not legal advice. The full license texts are
the legally binding documents.*
