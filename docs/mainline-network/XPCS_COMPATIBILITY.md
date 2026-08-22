# ZX279133 XPCS1 Compatibility

## Result

ZX279133 XPCS1 is compatible with the generic `snps,dw-xpcs` platform driver
for direct Clause 45 register access, 2.5GBASE-X discovery, reset, and phylink
configuration. One demonstrated SoC integration quirk remains in the MAC
consumer: CPU133 requires MMD3 register `0x8005` bit 4 after PCS configuration.

## Proven Properties

- Physical base: `0x1b000000` for XMAC1.
- Instance stride: `0x1000000` from the recovered `xmac << 24` calculation.
- Direct aperture: `0x800000`, exactly covering
  `reg-io-width * SZ_2M` for 32-bit direct Clause 45 encoding.
- Register width: 4 bytes.
- PCS ID: `0x7996ced0`, accepted as native DesignWare XPCS.
- CSR clock: shared `pon_serdes_pclk`, 125 MHz.
- Interface: generic XPCS reports `DW_2500BASEX` for
  `PHY_INTERFACE_MODE_2500BASEX`.

## Hardware Test

A temporary platform consumer called `xpcs_create_fwnode()` for XPCS1, checked
`xpcs_get_an_mode(..., PHY_INTERFACE_MODE_2500BASEX)`, and destroyed the handle.
The test passed on FIT SHA256
`cd4fd56fb5b1142c2fb2ffbb598c17553eb303eeed2707d0029e47a11145bcd6`.

Runtime state after handle destruction:

```text
dwxpcs platform device: 1b000000.ethernet-pcs
MDIO backend device:    dwxpcs-0:00
pon_serdes_pclk:        125 MHz, disabled
TOPCRM 0x44:            0x00000210
```

The test performed standard ID/interface discovery only. It did not call PCS
configuration or reset operations.

## SerDes Lifecycle Findings

- Read-only PON SerDes provider probe binds with no clock or register changes.
- Reading the SerDes APB window while PCLK is gated hangs the bus and triggers
  the platform watchdog. All diagnostics must use CCF before MMIO access.
- Fixed mode-9 power-on reached CDR lock with status
  `0x21000782/0x01000706`, then power-off restored TOPCRM/PLL state and disabled
  PCLK.
- `0xd0` bit 0 remains clear during a working vendor 2.5G TFTP link. The driver
  therefore treats that recovered common-PLL status as advisory for mode 9 and
  uses CDR lock as the signal-dependent hard condition.
- Consumer unbind, provider unbind, and provider rebind completed. A later
  platform watchdog reset occurred after the bind command had returned; it was
  not a provider bind failure.

## Next Boundary

The real NPPT MAC consumer now exercises generic PCS configuration through
phylink. FIT SHA256
`021ae3259b3a745365643d3b09f25c2ea18b8a2c3a48bcd5b6d04c46d46d8613`
passed three complete down/up cycles at 2.5G full duplex. The bypass register
read `0x10` on every up; final stop disabled the shared CSR/PON SerDes clock.
Packet DMA and interrupts remained disabled.
