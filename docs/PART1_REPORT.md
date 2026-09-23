# AveMotion Part 1 — implementation report

Date: 2026-07-25  
Local bootstrap commit: run `git rev-parse HEAD`  
Tag: `stage0-bootstrap-part1`

## Delivered scope

Part 1 establishes a reproducible reference laboratory around the rlottie lineage
without modifying upstream implementation code prematurely.

Completed:

- standalone CMake 3.25+ project;
- pinned upstream manifest for `desktop-app/rlottie@cbfdcdd2830b4beba53e64e72c11cb0328a720a9`;
- comparison pin for `TelegramMessenger/rlottie@67f103bc8b625f2a4a9e94f1d8c7bd84c5a08d1d`;
- vendor/fetch script with exact commit verification and cleanup on failure;
- optional CMake integration of the pinned rlottie checkout;
- first AveMotion-owned facade: `avemotion::reference::ReferenceRuntime`;
- metadata probe executable;
- offline characterization/bootstrap test;
- optional rlottie JSON smoke test;
- install/export package and external consumer smoke test;
- Clang and GCC presets;
- Windows/MSVC presets prepared for external verification;
- GitHub Actions bootstrap and upstream-reference jobs;
- provenance, license/security, patching and next-stage documents;
- clean local Git repository and tag.

## Verified in this environment

Environment:

- Linux 6.12.13 x86_64 GNU/Linux
- cmake version 3.31.6
- Ninja 1.12.1
- clang version 17.0.0 (https://github.com/swiftlang/llvm-project.git 10999b6d034fe318f3d56c83bddb6572593a8bb0)
- g++ (Debian 14.2.0-19) 14.2.0

Successful configurations:

1. `linux-clang-debug`
   - configure: passed
   - build: passed
   - CTest: 1/1 passed
   - probe: passed
2. `linux-gcc-release`
   - configure: passed
   - build: passed
   - CTest: 1/1 passed
   - probe: passed
3. install/export package
   - install: passed
   - external `find_package(AveMotionBootstrap CONFIG REQUIRED)`: passed
   - external consumer compile/link/run: passed
4. Python tooling
   - syntax compilation: passed

Full command output is stored in `docs/build-logs/`.

## Upstream checkout status

The execution sandbox blocks outbound network access and DNS from the build
container. The pinned checkout therefore could not be cloned locally, and the
optional `avemotion.rlottie.smoke` test was not executed here.

This is explicitly recorded rather than hidden:

- failure log: `docs/build-logs/fetch-upstream-sandbox.log`;
- the fetch script removes incomplete checkouts;
- the reference CMake preset fails loudly if the checkout is missing or has the
  wrong commit;
- GitHub Actions is prepared to fetch and test the pinned checkout on a runner
  with network access.

The upstream source is intentionally not copied into this archive. This keeps its
provenance and Git history intact. Run:

`python3 scripts/fetch_rlottie.py`

then:

`cmake --preset linux-clang-reference`
`cmake --build --preset linux-clang-reference`
`ctest --preset linux-clang-reference`

## Architecture boundary introduced

`avemotion_reference` is the only target allowed to expose the old renderer to
new AveMotion characterization code. UI, future runtime and future Direct2D code
must not include rlottie headers directly.

Current direction:

`pinned rlottie -> reference facade -> characterization tests -> isolated replacement`

No upstream files were renamed in Part 1. Renaming before golden tests would make
regressions difficult to localize and would erase useful correspondence with the
original code.

## Known limitations

- no Direct2D code yet;
- no new canonical asset/evaluator yet;
- no upstream source bundled;
- no upstream build result from this sandbox;
- Windows presets are prepared but not executed here;
- AveMotion-owned code does not yet have a selected distribution license;
- the selected historical upstream must not be treated as safe for untrusted
  animation input without a later security audit and validation layer.

## Part 2 recommendation

After fetching the pinned source:

1. build upstream unchanged on Linux and Windows;
2. create a small controlled Lottie corpus;
3. render fixed frames and record metadata plus pixel hashes;
4. instrument the path `load -> update -> render tree -> CPU rasterizer`;
5. expose evaluated render-tree data behind an AveMotion-owned adapter;
6. only then begin file/class renaming or extraction of the CPU renderer.

This preserves the professional sequence:

`characterize -> isolate -> replace -> compare -> retire old path`.
