# Native Windows validation received 2026-07-26

Host run:

```text
Visual Studio 2022 Community / MSVC
Windows 10/11 host
preset: windows-msvc-direct2d
```

Confirmed before the verifier hotfix:

- configure completed;
- all 47 build steps completed;
- `avemotion.direct2d.header` passed;
- `avemotion.direct2d.contract` passed;
- native `avemotion.direct2d.smoke` passed through D3D11 WARP and real Direct2D;
- the WARP capture contains the expected opaque red fill, half-alpha red fill and green stroke;
- all eleven compiled functional/offline tests in the full suite passed;
- only `avemotion.vendor.verify` failed.

The two failing hashes were reproduced exactly by applying Windows-style
case-insensitive ordering to the same unchanged vendor files:

```text
Samsung legacy Windows ordering:
5e1115bf9fed2e246f6b1d6c5a42c4d98dcb38be7464e1995e161d03b3ebb479

Telegram legacy Windows ordering:
7deaf9545524c395ee151e356ec1d035897778fe7b066ee94bbe95e87b30aae1
```

The file counts and total byte counts remained correct. The source trees were
not corrupted; the old verifier sorted `Path` objects directly, which is
case-sensitive on POSIX and case-insensitive on Windows. Part 18.1 defines the
fingerprint order explicitly by UTF-8 POSIX relative-path bytes and adds a
portable regression test.
