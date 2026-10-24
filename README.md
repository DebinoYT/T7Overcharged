# T7Overclocked

T7Overclocked is a Black Ops III PC DLL focused on extending the `attachment`
and `attachmentunique` asset pools. It retains the existing exported `init`
entry point and BO3 build detection, and removes T7Overcharged's unrelated
Lua, UI, console, network, Discord, gameplay-patch, and hot-reload features.

## Supported pools

| Pool | Stock capacity |
| --- | ---: |
| `attachment` | 128 |
| `attachmentunique` | 1024 |

The `attachmentunique` capacity above is the supplied target stock value. The
BOIII reference table reports `0x864` for its target build, so the runtime
capacity logged for the actual executable is authoritative.

Only these two pools are currently supported. Other pool names are logged and
ignored; their engine-side limits have not been investigated. The example
`weapon=0` and `xmodel=0` entries are accepted as no-op requests; positive
capacities for them are unsupported and will be logged.

## Configuration

Place `T7Overclocked.ini` and `offsets.txt` in the BO3 working directory.

```ini
[T7Overclocked]
Enabled=true
LogLevel=info

[AssetPools]
attachment=256
attachmentunique=2048
```

A pool value of `0` leaves it unchanged. Positive values are requested
capacities. A request below the runtime capacity never shrinks a pool.
Invalid values and unsupported pool names are logged.

The shipped `offsets.txt` contains the `DB_XAssetPool` RVAs already used by
the T7Overcharged build table. The `DB_AssetPoolInit` RVA is intentionally
left unset because this repository and the supplied reference sources do not
verify that function address for the three supported executables. When pools
are not yet initialized, supply the verified RVA for the exact build before
enabling expansion. Addresses are relative virtual addresses from the BO3
image base, written as hexadecimal values.

When pools are already initialized, the extender can resize them only if no
entries have been allocated and a verified `DB_AssetPoolAlloc` hook is
available to serialize allocator activity with the metadata update. It refuses
live reallocation of populated pools because other engine objects may hold
pointers into the current pool memory.
For safe early expansion, the `DB_AssetPoolInit` hook must be configured so
the replacement allocation is installed immediately after engine
initialization and before assets are allocated.

An optional `DB_AssetPoolAlloc.<build>` RVA in `offsets.txt` enables logging
when the engine allocator returns a non-null slot beyond the stock capacity.
It is required for safe resizing when pools are already initialized. That
telemetry confirms the allocator used an expanded slot; successful asset
registration and lookup still require an in-game test with actual assets.

The DLL writes its log to `logs/T7Overclocked.log`.

## Build

1. Update submodules and dependencies.
2. Run `generate.bat` or `tools\premake5.exe vs2022`.
3. Build `build\T7Overclocked.sln` in Visual Studio.

The generated DLL is named `T7Overclocked.dll`.
