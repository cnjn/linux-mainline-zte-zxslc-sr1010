# ZX279133 PON SerDes Driver Evidence

## Driver Role

`drivers/phy/zte/phy-zx279133-pon-serdes.c` is a ZX279133-specific SoC SerDes
driver using the kernel Generic PHY framework. It is not the external WAN PHY.
ZX279051 is a separate PHYLIB driver on MDIO1 address 1.

## Implemented Modes

The driver accepts:

```text
PHY_MODE_ETHERNET + PHY_INTERFACE_MODE_2500BASEX  -> PON SerDes mode 9
PHY_MODE_ETHERNET + PHY_INTERFACE_MODE_SGMII      -> PON SerDes mode 8
```

2.5GBASE-X selects the CPU133 PON SerDes mode-9 profile. XMAC1 work mode 4
calls the vendor Uni mode 5, which `uni_eth_mode_change()` maps to PON mode 9.
Mode 9 and vendor P2P mode 15 share `mode_eth_2p5gbase_x_cfg()`.

SGMII selects the CPU133 PON SerDes mode-8 profile. XMAC1 work mode 3 calls
Uni mode 7, which `uni_eth_mode_change()` maps to PON mode 8; the profile is
`uni_mode_eth_1gbase_x_cfg()`. Both profiles share the same post-write
activation RMWs from `pon_serdes_init()`.

The 49 profile words at SerDes offsets `0x00..0xc0` come directly from the
CPU133 branch of recovered `mode_eth_2p5gbase_x_cfg()`. The post-write activation
updates are also recovered:

- `0x90`: clear bits 13-14, then set bit 14.
- `0x40`: set bit 15.
- `0x54`: set bit 0.

The resulting values match the U-Boot 2.5G post-initialization capture for the
registers changed by activation.

## Power Sequence

1. Enable the shared 125 MHz PON SerDes PCLK through CCF.
2. Snapshot TOPCRM shared fields and dedicated mode words.
3. Select the Ethernet PLL profile:
   - clear TOPCRM `0x10` bits 4-5;
   - preserve firmware bit 30 at mode word `0xc0` and apply `0x20106454`;
   - apply `0x04000000` at mode word `0xc4` using the recovered bit-28 latch
     sequence;
   - select TOPCRM `0x0c` bit 9 and clear bit 8.
4. Assert local reset bits 0 and 1 for 10-11 ms.
5. Deassert both resets and wait another 10-11 ms.
6. Write and activate the fixed mode-9 profile.
7. Poll common PLL lock at SerDes `0xd0` bit 0 every 2 ms for up to 2 seconds.
8. If RX LOS is clear, poll CDR lock at `0xe4` bit 1 with the same bound.

The delay conversion is exact: recovered ARM64 `__const_udelay(0x418958)` is
1000 us and is executed ten times; `0x8312b0` is 2000 us.

## Deliberate Differences from Vendor Behavior

- Common PLL lock failure aborts power-on.
- CDR timeout is a warning, not a power-on failure. CDR depends on ZX279051 and
  external line signal; an unplugged cable must not make the SoC PHY unusable.
- Probe only acquires resources and registers a provider. It performs no clock,
  reset, PLL, or SerDes writes.
- Power-on failure, power-off, and managed cleanup restore the saved TOPCRM
  fields and PLL mode words, assert resets, and release the PCLK.
- Unknown TOPCRM `0xc0` bit 30 is preserved.
- No external PLL reference clock is claimed.
- No 1G, 5G, 10G, PON protocol, PRBS, or diagnostic mode is implemented.

## Current Validation

- Kconfig selects Generic PHY and builds the driver into the diagnostic kernel.
- Docker build succeeded with FIT SHA256
  `c541ff98acf69212d2d287e653f525a7dd11084646a41401f1535e481a1b5f03`.
- With `phy@16000000` disabled, the platform driver registered but no device
  bound.
- TOPCRM and PLL state remained unchanged after boot:

```text
0x10e1000c = 0x06711277
0x10e10010 = 0x00000000
0x10e10044 = 0x00000210
0x10e10048 = 0x00000b50
0x10e100c0 = 0x60106454
0x10e100c4 = 0x04000000
```

## Next Test

Enable only the PON SerDes provider in the SR1010 board DTS. Probe must bind
without changing the register values above or enabling PCLK. Actual power-on is
deferred until that read-only lifecycle test passes.
