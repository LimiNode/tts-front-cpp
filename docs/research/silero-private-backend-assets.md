# Private Silero backend: asset boundary

This step starts the production backend on the library side without making
ONNX Runtime a dependency of the core public API. Runtime asset verification is
now available as an internal utility:

```cpp
tts_front::detail::sha256_file_hex(path, chunk_size)
```

It hashes assets incrementally, so a large graph is not materialized as one
`std::string`. The utility has SHA-256 known-answer coverage (`"abc"`) and
rejects invalid chunk sizes.

The native research executable continues to own its complete parity harness.
The next private-backend step will build a bundle loader around this utility,
then add optional ONNX Runtime session ownership and sentence orchestration.
Neither that backend nor ONNX Runtime is exposed by `tts_front.hpp` or enabled
by the default `tts_front` build in this change.
