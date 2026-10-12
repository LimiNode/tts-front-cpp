# v0.2.2 quality archive

This directory is the immutable quality archive for the `v0.2.2` release. It
contains the reviewed 399-case EN/RU corpus and strict snapshot.

Snapshot outcomes:

- correct transformation: 345;
- justified preservation: 54;
- unnecessary refusal: 0;
- incorrect or partial transformation: 0.

The archive is executed by the strict CTest audit
`tts_front_quality_v022_archive` and its checksums are verified by
`tools/check_quality_archive.py`. Earlier release archives are left unchanged.
