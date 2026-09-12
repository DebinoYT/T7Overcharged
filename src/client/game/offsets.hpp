#pragma once
#include "build_offsets.generated.hpp"

namespace game::offsets
{
    constexpr build identify(std::uint32_t timestamp, std::uint32_t image_size)
    {
        // Stock September SizeOfImage is not page aligned; dumps round it up.
        const auto size = (std::uint64_t(image_size) + 0xFFF) & ~std::uint64_t(0xFFF);
        if (timestamp == 0x63F95E3C && size == 0x1FAB7000) return build::march_2023;
        if (timestamp == 0x693D731E && size == 0x1D74B000) return build::february_2026;
        if (timestamp == 0x6A7B6355 && size == 0x1D75C000) return build::september_2026;
        return build::unknown;
    }

    build detect(std::uintptr_t module);
    build active_build();
    const char* build_name();
    std::uintptr_t resolve(std::uintptr_t february_rva);

    template<std::uintptr_t FebruaryAddress>
    std::uintptr_t address()
    {
        constexpr auto rva = FebruaryAddress - 0x140000000;
        static_assert(registered(rva), "Register and verify this BO3 address for all supported builds");
        return resolve(rva);
    }
}

// Source literals identify the February 2026 layout, not the running executable.
#define OFFSET(value) (::game::offsets::address<value>())
#define REBASE(value) OFFSET(value)
