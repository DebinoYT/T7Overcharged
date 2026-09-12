#include <std_include.hpp>
#include "offsets.hpp"

namespace game::offsets
{
    build detect(const std::uintptr_t module)
    {
        __try
        {
            const auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(module);
            if (!dos || dos->e_magic != IMAGE_DOS_SIGNATURE || dos->e_lfanew <= 0 || dos->e_lfanew > 0x100000)
                return build::unknown;
            const auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS64*>(module + dos->e_lfanew);
            if (nt->Signature != IMAGE_NT_SIGNATURE || nt->FileHeader.Machine != IMAGE_FILE_MACHINE_AMD64 ||
                nt->FileHeader.SizeOfOptionalHeader < sizeof(IMAGE_OPTIONAL_HEADER64) ||
                nt->OptionalHeader.Magic != IMAGE_NT_OPTIONAL_HDR64_MAGIC)
                return build::unknown;
            return identify(nt->FileHeader.TimeDateStamp, nt->OptionalHeader.SizeOfImage);
        }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
            return build::unknown;
        }
    }

    build active_build()
    {
        static const auto version = detect(reinterpret_cast<std::uintptr_t>(GetModuleHandleW(nullptr)));
        return version;
    }

    const char* build_name()
    {
        switch (active_build())
        {
        case build::march_2023: return "March 2023";
        case build::february_2026: return "February 2026";
        case build::september_2026: return "September 2026";
        default: return "unsupported";
        }
    }

    std::uintptr_t resolve(const std::uintptr_t february_rva)
    {
        const auto rva = translate(active_build(), february_rva);
        return rva ? reinterpret_cast<std::uintptr_t>(GetModuleHandleW(nullptr)) + rva : 0;
    }
}
