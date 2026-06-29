# Device Tree Blob Parsing

## What is the Device Tree?

A Device Tree (DT) is a data structure that describes hardware to software. Instead of hardcoding "there is a UART at address 0x09000000" into the kernel, the bootloader passes a Device Tree Blob (DTB) to the kernel at boot. The kernel reads it to discover what hardware exists.

This is the standard mechanism on ARM Linux and is followed by QEMU even for bare-metal kernels using `-kernel`. When AeonOS boots under QEMU, the firmware:

1. Generates a DTB describing the virtual machine's hardware
2. Loads it into RAM at a specific address
3. Puts that address in `x0` before jumping to `_start`

Since `boot.S` never touches `x0`, the DTB address arrives in `kernel_main` as its first argument.

## Flattened Device Tree (FDT) Format

The DTB is a binary encoding of the device tree called the Flattened Device Tree (FDT). It consists of three sections:

```
┌─────────────────────────────────────┐
│         FDT Header (40 bytes)       │
│  magic, offsets, version info       │
├─────────────────────────────────────┤
│      Memory Reservation Map         │
│  (reserved memory regions)         │
├─────────────────────────────────────┤
│         Structure Block             │
│  Tokens + node names + properties   │
├─────────────────────────────────────┤
│          Strings Block              │
│  Property name strings              │
└─────────────────────────────────────┘
```

**All multi-byte values in the FDT are big-endian**, even when the CPU runs little-endian. This is why `lib/endian.h` provides `be32_read()` and `be64_read()`.

### Header Layout

All fields are big-endian `uint32`:

| Offset | Field | Description |
|---|---|---|
| 0 | `magic` | Must be `0xD00DFEED` |
| 4 | `totalsize` | Total size of the blob |
| 8 | `off_dt_struct` | Byte offset to structure block |
| 12 | `off_dt_strings` | Byte offset to strings block |
| 16 | `off_mem_rsvmap` | Byte offset to reservation map |
| 20 | `version` | FDT version (typically 17) |
| 24 | `last_comp_version` | Minimum compatible version |
| 28 | `boot_cpuid_phys` | Physical CPU ID of boot CPU |
| 32 | `size_dt_strings` | Size of strings block |
| 36 | `size_dt_struct` | Size of structure block |

Validation: check `magic == 0xD00DFEED` before doing anything else. A wrong magic means `x0` didn't hold a valid DTB (perhaps the boot protocol wasn't followed), and we fall back to 128 MB.

### Structure Block Tokens

The structure block is a sequence of 4-byte aligned tokens:

| Token | Value | Followed by |
|---|---|---|
| `FDT_BEGIN_NODE` | `0x00000001` | Null-terminated node name, padded to 4-byte boundary |
| `FDT_END_NODE` | `0x00000002` | Nothing |
| `FDT_PROP` | `0x00000003` | uint32 length, uint32 nameoff, `length` bytes of data (padded) |
| `FDT_NOP` | `0x00000004` | Nothing (skip) |
| `FDT_END` | `0x00000009` | End of structure block |

A node name in `FDT_BEGIN_NODE` is a C string. The node name must be padded to the next 4-byte boundary after the null terminator. Example: `"memory@40000000"` is 17 characters + 1 null = 18 bytes → pad to 20 bytes (next multiple of 4).

A `FDT_PROP` record's `nameoff` field is a byte offset into the strings block, pointing to the property's null-terminated name.

### Tree Structure

The structure block encodes a tree via nested `BEGIN_NODE` / `END_NODE` pairs:

```
FDT_BEGIN_NODE ""           ← root node (empty name)
  FDT_PROP "compatible" ...
  FDT_PROP "#address-cells" 0x00000002   ← 2 cells per address
  FDT_PROP "#size-cells"    0x00000002   ← 2 cells per size

  FDT_BEGIN_NODE "memory@40000000"
    FDT_PROP "device_type" "memory\0"
    FDT_PROP "reg" <0x00000000 0x40000000 0x00000000 0x08000000>
  FDT_END_NODE

  FDT_BEGIN_NODE "cpus"
    ...
  FDT_END_NODE

  ...
FDT_END_NODE
FDT_END
```

## Finding RAM Size

We walk the structure block linearly, tracking depth with an integer counter.

```
depth 0: before root node begins
depth 1: inside root node  (FDT_BEGIN_NODE "" → depth 1)
depth 2: inside memory node (FDT_BEGIN_NODE "memory@..." → depth 2)
```

Algorithm:

```
p = base + off_struct
depth = 0
in_memory = false

loop:
  token = be32_read(p); p += 4

  FDT_BEGIN_NODE:
    name = (char *)p
    p += align4(strlen(name) + 1)
    depth++
    if depth == 2 and name starts with "memory":
      in_memory = true

  FDT_END_NODE:
    if in_memory and depth == 2:
      in_memory = false
    depth--

  FDT_PROP:
    length  = be32_read(p)
    nameoff = be32_read(p + 4)
    data    = p + 8
    p += 8 + align4(length)
    if in_memory and depth == 2:
      prop_name = strings + nameoff
      if prop_name == "reg" and length >= 16:
        size = (uint64)(be32_read(data+8) << 32) | be32_read(data+12)
        return size >> 20    ← MB

  FDT_NOP: continue
  FDT_END: return 0 (not found)
```

### The `reg` Property Format

The `reg` property encodes pairs of (address, size). The number of 32-bit cells per address and per size is controlled by `#address-cells` and `#size-cells` in the parent node.

For QEMU's `virt` machine, the root node sets both to `2`, meaning each address and each size uses two 32-bit cells (= one 64-bit value). So the memory node's `reg` contains four 32-bit cells:

```
Cell 0: address high = 0x00000000
Cell 1: address low  = 0x40000000   → base = 0x0000000040000000
Cell 2: size high    = 0x00000000
Cell 3: size low     = 0x08000000   → size = 128 MB (0x8000000 bytes)
```

Or with 256 MB specified (`-m 256m`):
```
Cell 3: size low = 0x10000000   → size = 256 MB
```

We skip cells 0 and 1 (we don't need the base address — we know RAM starts at 0x40000000) and read cells 2 and 3:

```c
uint64_t size = ((uint64_t)be32_read(data + 8) << 32)
              | (uint64_t)be32_read(data + 12);
uint32_t mb = (uint32_t)(size >> 20);   // bytes → MB
```

## Fallback Behaviour

`dtb_get_ram_mb()` returns `0` in these cases:

- `fdt == NULL` — QEMU didn't pass a DTB
- `magic != 0xD00DFEED` — corrupt or absent DTB
- No `memory` node found in the tree
- `reg` property is shorter than 16 bytes (malformed)
- Walk exceeded 65,536 tokens (safety guard against infinite loops)

`hw_detect` treats a `0` return as "unknown" and falls back to 128 MB:
```c
uint32_t dtb_mb = dtb_get_ram_mb(dtb_addr);
info->ram_mb = (dtb_mb > 0) ? dtb_mb : 128;
```

## Future: Full DT Support

Phase 1 only needs RAM size. As AeonOS gains more drivers, the DTB will become essential for:

| Data | DTB Location | Phase Needed |
|---|---|---|
| RAM size | `/memory` node | Phase 1 ✓ |
| Interrupt controller type/address | `/intc` node | Phase 2 |
| CPU topology | `/cpus` nodes | Phase 2 |
| UART address | `/pl011@...` node | Phase 3 |
| Clock frequencies | `/clocks` nodes | Phase 3 |
| NPU / DSP nodes | `/hexagon` or `/npu` | Phase 9 |
| GPIO controllers | `/gpio` nodes | Phase 5 |

A full DTB library will be introduced in Phase 2 when we need the interrupt controller to set up exception handling.
