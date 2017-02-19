# read-spirv

This is a small project I've set up to try and teach myself the [SPIR-V](https://www.khronos.org/registry/spir-v/specs/1.0/SPIRV.pdf) specification. The long term goal for this project is to build a lightweight, self-contained library in pure C++ that can consume a SPIR-V binary and emit a data structure describing all entry point interfaces and descriptor sets.

If you're looking for the ability to do interesting transformations on SPIR-V, you should probably check out [SPIRV-Cross](https://github.com/KhronosGroup/SPIRV-Cross) instead.
