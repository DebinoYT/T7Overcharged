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

The DLL derives its containing mod directory from its own loaded module path.
It requires the DLL to be installed in `mods/<mod>/zone`; this identifies the
mod containing the DLL and does not independently verify that BO3 has activated
that mod. It loads both files from that `zone` directory and writes the log to
the containing mod's `logs` directory:

```text
Black Ops III/
└── mods/
    └── <active-mod>/
        ├── logs/
        │   └── T7Overclocked.log
        └── zone/
            ├── T7Overclocked.ini
            └── offsets.txt
```

At startup the boot diagnostic and normal log record all resolved paths. There
is no fallback to the current working directory or an unrelated directory.
Paths are derived from the DLL's loaded module handle using
`GetModuleFileNameW`; no BO3 active-mod structure offsets are used for path
resolution. If the DLL is not under a validated `mods/<mod>/zone` directory,
configuration-dependent behavior is disabled safely.

At the earliest init entry, the DLL also creates a temporary diagnostic at
`%TEMP%\T7Overclocked_boot_<pid>_<tick>.log`. It records entry, process ID,
DLL-path resolution, failures, and progress opening the mod log and
initializing configuration. This diagnostic is retained to assist testing.
Initialization remains on the loader's calling thread; if DLL-path resolution
fails, the diagnostic records the error and configuration-dependent code is
not initialized. The normal file logger and debugger output do not require a
BO3 console to be attached.

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
