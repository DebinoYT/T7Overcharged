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
DiagnosticOnly=false

[AssetPools]
attachment=256
attachmentunique=2048
```

A pool value of `0` leaves it unchanged. Positive values are requested
capacities. A request below the runtime capacity never shrinks a pool.
Invalid values and unsupported pool names are logged.

`MonitorIntervalMs` (default `2000`, `0` disables) sets how often a read-only
monitor checks the extended pools and logs when the engine allocates entries
beyond the original capacity.

Set `DiagnosticOnly=true` to validate and log the pool table and the
`attachment`/`attachmentunique` descriptors without installing hooks or
changing pool memory.

### How pools are extended

BO3 allocates pool entries by popping `XAssetPool::freeHead` and releases them
by pushing back onto it. Live assets are referenced by pointer, so the
existing storage is never moved or copied. For each request above the runtime
capacity, the DLL allocates a separate block of `requested - capacity` entries,
chains them, and links the chain onto the tail of the existing free list with
an atomic compare-exchange. It then raises `itemCount` to the requested
capacity. Stock entries are still handed out first; the new entries are only
used once the stock storage is exhausted, which is where the engine would
otherwise fail. This is the same technique DutyCore uses from its Lua `init`,
with free-list validation and an atomic tail link added. No game code is
hooked or patched.

Before extending, the DLL checks that the descriptor's `itemSize` matches the
pool (`0x220` attachment, `0x840` attachmentunique), that it is not a
singleton, and that every free-list link stays inside the pool's storage.
The log records each pool's original, requested and final capacity and the
outcome. It also records how many RIP-relative references in the running,
decrypted executable resolve into the configured `DB_XAssetPool` table, and
which ones address the two target descriptors directly. The candidate
function RVAs cannot be checked on disk because that code is encrypted.

Known limitation: extension entries lie outside the pool's original
contiguous storage. Engine code that iterates the storage by index up to
`itemCount`, or that derives an index from an entry's offset into it, would
mishandle them. The direct-reference count in the log flags type-specific code
of that kind, but only an in-game test with more than the stock number of
assets confirms the result.

The shipped `offsets.txt` contains the `DB_XAssetPool` RVAs already used by
the T7Overcharged build table. `DB_AssetPoolInit` is only needed when the DLL
loads before the pools exist, which does not happen when it is loaded from Lua.

The DLL writes its log to `logs/T7Overclocked.log`.
## Build

1. Update submodules and dependencies.
2. Run `generate.bat` or `tools\premake5.exe vs2022`.
3. Build `build\T7Overclocked.sln` in Visual Studio.
4. Run `build\bin\x64\<config>\T7Overclocked.Tests.exe` for the offline
   free-list extension tests.

The generated DLL is named `T7Overclocked.dll`.
