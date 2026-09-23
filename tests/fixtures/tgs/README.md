# Part 22 TGS fixture

`repeater_content_group.tgs` is a deterministic gzip transport of:

```text
tests/fixtures/repeater_content_group.json
```

Properties:

```text
compressed bytes:   974
uncompressed bytes: 5408
MTIME:              0
FLG:                0
XFL:                2 (maximum compression)
OS:                 255 (unknown/portable)
SHA-256:            7129163eb83d0af7d25bfd2dc82ba38f0e485a9c3103362fb6ae0418060186ed
```

The runtime test requires the decompressed bytes to match the source JSON
exactly, including whitespace. The fixture is not presented as a Telegram-owned
sticker; it is an AveMotion test asset placed in the same gzip/Lottie container
format as a Telegram `.tgs` sticker.
