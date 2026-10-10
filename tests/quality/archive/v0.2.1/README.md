# v0.2.1 quality archive

This directory is the immutable quality archive for the `v0.2.1` release.
It contains the 373-case EN/RU corpus and its strict snapshot captured for
the release implementation.

Snapshot outcomes:

- correct transformation: 324;
- justified preservation: 34;
- unnecessary refusal: 2;
- incorrect or partial transformation: 13.

The `v0.2.0` archive remains unchanged and is still the historical release
baseline. The `v0.2.1` archive is executed by the strict CTest audit and its
checksums are verified by `tools/check_quality_archive.py`.
