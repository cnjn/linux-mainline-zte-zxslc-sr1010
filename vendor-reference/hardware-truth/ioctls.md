# Vendor ioctl truth

This table is derived from AArch64 handler disassembly/decompilation, not string-name guessing. Raw disassembly is retained under `ioctl-disasm/`.

## Character devices

| Module | Registration | Handler | Accepted commands |
|---|---|---|---|
| `TMI7604.ko` | major **107**, minors 0–255, name `pse_dev` | `pse_ioctl` | `0x33`, `0x34` only |
| `peripheral.ko` | major **106**, minors 0–255, name `peripheral` | `pph_drv_ioctl` | `0x12`, `0x13`, `0x20`, `0x21`, `0x28`, `0x29`, `0x31`, `0x32`, `0x41`, `0x42`, `0x51`, `0x60`, `0x61`, `0x62`, `0x63`, `0x70`, `0x80` |
| `np.ko` | major **191**, minors 0–255, name `msoft_debug_dev` | `ppu_msoft_debug_dev_ioctl` | decimal `2048..2081` (`0x800..0x821`) |
| `zx_ponreg.ko` | major **222**, minors 0–255 | `fpga_ioctl` | `0`, `1`, `3`, `0x4004de03` |

All four handlers copy a fixed vendor structure from userspace before dispatch. Exact userspace C type names are unavailable.

## `TMI7604.ko` / `pse_ioctl`

The handler copies an 8-byte request. It accepts only:

| cmd | Proven behavior |
|---:|---|
| `0x33` | validates I2C slave `0x50` and port selector 0–3, reads one of four register groups (`0x33..0x42`), packs four bytes, and copies the 8-byte request/result back to userspace |
| `0x34` | validates slave `0x50` and selector 0–3, reads register `0x14..0x17`, decodes/prints state; no userspace copy-back is visible |

Any other command returns `-1`. The module does not call `i2c_writeb` from this handler.

## `peripheral.ko` / `pph_drv_ioctl`

The handler copies a 16-byte request. Commands are raw small integers, not standard `_IO*()` values.

| cmd | Proven dispatch target / effect |
|---:|---|
| `0x12` | sequential I2C byte reads into a temporary buffer and printk dump |
| `0x13` | `i2c_writeb()` |
| `0x20` | MDIO0 Clause-22 read; copies result to userspace |
| `0x21` | `zx_mdio_write()` |
| `0x28` | external PHY register read: MDIO0 extended path or Realtek `rtk_port_phyReg_get()`; copies result |
| `0x29` | external PHY register write: `zx_mdio_0_write_extended()` or Realtek `rtk_port_phyReg_set()` |
| `0x31` | allocates/dumps an I2C byte buffer; the recovered body appears diagnostic/incomplete |
| `0x32` | I2C argument printk diagnostic; no bus operation visible |
| `0x41` | GPIO read via `zte_gpio_get_value()`; copies result |
| `0x42` | GPIO output direction + value via `zte_gpio_direction_output()` / `zte_gpio_set_value()` |
| `0x51` | vendor product/datapath state control (`rtk_init_flag`, `pon_up_flag`, LAN-up callback) |
| `0x60` | MDIO1 Clause-22 read; copies result |
| `0x61` | `zx_mdio_1_write()` |
| `0x62` | MDIO1 extended read; copies result |
| `0x63` | `zx_mdio_1_write_extended()` |
| `0x70`, `0x80` | accepted no-op success |

Unsupported commands return `-1`.

## `np.ko` / `ppu_msoft_debug_dev_ioctl`

Every command copies a 260-byte array (`65 * u32`). Commands `0x800..0x821` are the complete accepted range:

| cmd | Operation |
|---:|---|
| `0x800` | set PPU debug-mode enable |
| `0x801` | continue ME |
| `0x802` | single-step ME |
| `0x803` | accepted no-op |
| `0x804` | set real-instruction breakpoint |
| `0x805` | get breakpoint address; copy result |
| `0x806` | get ME interrupt flag; copy result |
| `0x807` | get ME interrupt status; copy result |
| `0x808` | set ME interrupt mask |
| `0x809` | get ME interrupt mask; copy result |
| `0x80a` | read 64 packet-memory words; copy result |
| `0x80b` | read 20 key-memory words; copy result |
| `0x80c` | get one SPR register; copy result |
| `0x80d` | get all 32 SPR registers; copy result |
| `0x80e` | get one RSP register; copy result |
| `0x80f` | get all 32 RSP registers; copy result |
| `0x810` | get flag register; copy result |
| `0x811` | get debug-valid state; copy result |
| `0x812` | set debug-packet-send enable |
| `0x813` | accepted no-op |
| `0x814` | set result word 0 to 1 and copy all 260 bytes back |
| `0x815` | accepted no-op |
| `0x816` | set PPU instruction |
| `0x817` | set duplicate table |
| `0x818` | program four SPA port-attribute fields |
| `0x819` | set SPA packet-type table |
| `0x81a` | set PPU interrupt enable |
| `0x81b` | get cluster-to-host-CPU interrupt; copy result |
| `0x81c` | program SDT |
| `0x81d` | program ERAM table |
| `0x81e` | program DDR table |
| `0x81f` | program hash table |
| `0x820` | get SPA port-attribute table; copy result |
| `0x821` | set one SPA port-attribute table field |

Unsupported commands return `1`. Operation-specific errors are returned by the called NP/PPU helper.

## `zx_ponreg.ko` / `fpga_ioctl`

The handler copies a 16-byte request and directly accesses shared `pon_base`, `pps_base`, and `nppt_base` mappings exported by the datapath modules.

| cmd | Proven behavior |
|---:|---|
| `0` | read and printk a sequence of registers; count is bounded to `0x7fffff`; no copy-back |
| `1` | write one selected module/register/value |
| `3` | read one register using module selector 0 and copy the 16-byte result back |
| `0x4004de03` | read one selected module/register and copy the 16-byte result back; also printk the value |

Other commands return success without an operation in this build after the initial request copy, rather than a conventional `-ENOTTY`.

## Netdev ioctl

`switch.ko` defines `ethdrv_port_dev_ioctl`, but its complete body is `return 0;`. Therefore this exact 2B5 module exposes **no functional netdev ioctl command dispatch**, despite abundant internal `ioctl_data_sweth` diagnostic strings referring to vendor management structures.
