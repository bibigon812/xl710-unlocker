# xl710_unlock
This program unlocks Intel XL710 NICs.

## Installation
gcc -O2 xl710_unlock.c -o xl710_unlock

## Usage

```shell
# ./xl710_unlock enp2s0f0np0
EMP SR offset: 0x686e
PHY offset: 0x69be
PHY struct size: 0x000d

=== Inspecting Structure States ===
Struct 0 (word 0x69c6 / byte 0xd38c): 0x630c <- unlocked
Struct 1 (word 0x69d4 / byte 0xd3a8): 0x630c <- unlocked
Struct 2 (word 0x69e2 / byte 0xd3c4): 0x630c <- unlocked
Struct 3 (word 0x69f0 / byte 0xd3e0): 0x630c <- unlocked

=== Unlocking Physical Ports (Structs 0 & 1 -> 0x630c) ===
Struct 0 (byte 0xd38c) written to 0x630c [OK]
Struct 1 (byte 0xd3a8) written to 0x630c [OK]

=== Recalculating NVM Checksum ===
NVM Checksum updated successfully.
```