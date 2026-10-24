#include <std_include.hpp>

#include "asset_limits.hpp"
#include "game/game.hpp"
#include "game/structs.hpp"

namespace asset_limits
{
	namespace
	{
		struct pool_request
		{
			game::XAssetType type;
			const char* name;
			std::uint32_t stock_size;
			std::uint32_t runtime_original{};
			std::uint32_t requested_size;
		};

		constexpr std::size_t pool_count = 2;
		std::array<pool_request, pool_count> requests{{
			{game::ASSET_TYPE_ATTACHMENT, "attachment", 0x80, 0, 0},
			{game::ASSET_TYPE_ATTACHMENT_UNIQUE, "attachmentunique", 0x400, 0, 0}
		}};

		void* asset_pool_init_original{};
		void* asset_pool_alloc_original{};
		std::uintptr_t asset_pool_init_address{};
		std::uintptr_t asset_pool_alloc_address{};
		std::uintptr_t asset_pools_address{};
		bool enabled = true;
		std::string log_level = "info";
		bool initialized_once{};
		bool minhook_initialized{};
		std::array<std::atomic<bool>, pool_count> beyond_stock_reported{};
		std::mutex pool_allocation_mutex;

		std::string trim(std::string value)
		{
			const auto is_space = [](const unsigned char character)
			{
				return std::isspace(character) != 0;
			};

			while (!value.empty() && is_space(static_cast<unsigned char>(value.front())))
				value.erase(value.begin());
			while (!value.empty() && is_space(static_cast<unsigned char>(value.back())))
				value.pop_back();
			return value;
		}

		std::string lower(std::string value)
		{
			std::transform(value.begin(), value.end(), value.begin(), [](const unsigned char character)
			{
				return static_cast<char>(std::tolower(character));
			});
			return value;
		}

		std::optional<std::uint32_t> parse_size(const std::string& value)
		{
			const auto text = trim(value);
			if (text.empty() || text.front() == '-')
				return std::nullopt;

			try
			{
				std::size_t consumed{};
				const auto hexadecimal = text.size() > 2 && text[0] == '0' &&
					(text[1] == 'x' || text[1] == 'X');
				const auto parsed = std::stoull(text, &consumed, hexadecimal ? 16 : 10);
				if (consumed != text.size() ||
					parsed > static_cast<unsigned long long>((std::numeric_limits<std::int32_t>::max)()))
					return std::nullopt;
				return static_cast<std::uint32_t>(parsed);
			}
			catch (const std::invalid_argument&)
			{
				return std::nullopt;
			}
			catch (const std::out_of_range&)
			{
				return std::nullopt;
			}
		}

		bool parse_bool(const std::string& value, bool& result)
		{
			const auto normalized = lower(trim(value));
			if (normalized == "true" || normalized == "1" || normalized == "yes")
			{
				result = true;
				return true;
			}
			if (normalized == "false" || normalized == "0" || normalized == "no")
			{
				result = false;
				return true;
			}
			return false;
		}

		void log(const std::string& message, const std::string& level = "info")
		{
			const auto priority = [](const std::string& name)
			{
				if (name == "debug") return 0;
				if (name == "info") return 1;
				if (name == "warning") return 2;
				return 3;
			};
			if (priority(level) < priority(log_level))
				return;
			game::minlog.WriteLine(message.c_str());
		}

		std::string build_key(const char* address_name)
		{
			return std::string(address_name) + "." + game::offsets::build_id();
		}

		bool read_rva(const std::string& key, std::uintptr_t& rva, const bool required = true)
		{
			std::ifstream input("offsets.txt");
			if (!input)
			{
				if (required)
					log("Asset pool expansion disabled: offsets.txt could not be opened.", "error");
				return false;
			}

			std::string line;
			while (std::getline(input, line))
			{
				line = trim(line);
				if (line.empty() || line.front() == '#' || line.front() == ';')
					continue;

				const auto separator = line.find('=');
				if (separator == std::string::npos || trim(line.substr(0, separator)) != key)
					continue;

				const auto value = trim(line.substr(separator + 1));
				if (value.size() < 3 || value[0] != '0' || (value[1] != 'x' && value[1] != 'X'))
					break;

				try
				{
					std::size_t consumed{};
					const auto parsed = std::stoull(value, &consumed, 16);
					if (consumed != value.size() || parsed == 0)
						break;
					rva = static_cast<std::uintptr_t>(parsed);
					return true;
				}
				catch (const std::invalid_argument&)
				{
					break;
				}
				catch (const std::out_of_range&)
				{
					break;
				}
			}

			if (required)
				log(std::string("Asset pool expansion disabled: missing or invalid offset ") + key + ".", "error");
			return false;
		}

		bool resolve_rva(const std::uintptr_t rva, const std::size_t bytes,
			const bool require_executable, const bool require_writable,
			std::uintptr_t& address)
		{
			const auto module = reinterpret_cast<std::uintptr_t>(GetModuleHandleW(nullptr));
			const auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(module);
			if (!module || !dos || dos->e_magic != IMAGE_DOS_SIGNATURE || dos->e_lfanew <= 0)
				return false;

			const auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS64*>(module + dos->e_lfanew);
			if (nt->Signature != IMAGE_NT_SIGNATURE ||
				rva >= nt->OptionalHeader.SizeOfImage ||
				bytes > nt->OptionalHeader.SizeOfImage - rva)
				return false;

			address = module + rva;
			MEMORY_BASIC_INFORMATION memory{};
			if (!VirtualQuery(reinterpret_cast<const void*>(address), &memory, sizeof(memory)) ||
				memory.State != MEM_COMMIT || (memory.Protect & (PAGE_GUARD | PAGE_NOACCESS)))
				return false;

			const auto region_base = reinterpret_cast<std::uintptr_t>(memory.BaseAddress);
			if (address < region_base)
				return false;
			const auto region_offset = address - region_base;
			if (region_offset > memory.RegionSize || bytes > memory.RegionSize - region_offset)
				return false;

			const auto executable = (memory.Protect & (PAGE_EXECUTE | PAGE_EXECUTE_READ |
				PAGE_EXECUTE_READWRITE | PAGE_EXECUTE_WRITECOPY)) != 0;
			const auto writable = (memory.Protect & (PAGE_READWRITE | PAGE_WRITECOPY |
				PAGE_EXECUTE_READWRITE | PAGE_EXECUTE_WRITECOPY)) != 0;
			return (!require_executable || executable) && (!require_writable || writable);
		}

		bool load_offsets()
		{
			std::uintptr_t pool_rva{};
			if (!read_rva(build_key("DB_XAssetPool"), pool_rva) ||
				!resolve_rva(pool_rva, sizeof(game::XAssetPool) * 0x67, false, true,
					asset_pools_address) ||
				asset_pools_address % alignof(game::XAssetPool) != 0)
			{
				log("Asset pool expansion disabled: DB_XAssetPool offset is not a valid mapped address.", "error");
				return false;
			}

			return true;
		}

		bool initialize_minhook()
		{
			if (minhook_initialized)
				return true;
			const auto status = MH_Initialize();
			if (status != MH_OK && status != MH_ERROR_ALREADY_INITIALIZED)
			{
				log("Asset pool expansion disabled: MinHook initialization failed.", "error");
				return false;
			}
			minhook_initialized = true;
			return true;
		}

		game::XAssetPool* get_pool(const game::XAssetType type)
		{
			if (!asset_pools_address)
				return nullptr;
			return reinterpret_cast<game::XAssetPool*>(asset_pools_address) + static_cast<int>(type);
		}

		bool expand_pool(pool_request& request)
		{
			if (!request.requested_size)
				return true;

			std::lock_guard<std::mutex> allocation_lock(pool_allocation_mutex);
			auto* pool = get_pool(request.type);
			if (!pool)
			{
				log(std::string(request.name) + ": pool metadata is unavailable.", "error");
				return false;
			}

			const auto original_size = pool->itemCount;
			if (original_size <= 0 || !pool->pool || pool->itemSize < sizeof(game::AssetLink))
			{
				log(std::string(request.name) + ": engine pool is not initialized; resize was not attempted.",
					"error");
				return false;
			}

			log(std::string(request.name) + ": requested=" + std::to_string(request.requested_size) +
				", runtime-original=" + std::to_string(original_size) +
				", allocated=" + std::to_string(pool->itemAllocCount) +
				", entry-size=" + std::to_string(pool->itemSize) + ".");

			if (pool->isSingleton[0])
			{
				log(std::string(request.name) + ": unsupported singleton pool; left unchanged.", "warning");
				return false;
			}
			if (!request.runtime_original)
				request.runtime_original = static_cast<std::uint32_t>(original_size);
			if (request.requested_size <= static_cast<std::uint32_t>(original_size))
			{
				log(std::string(request.name) + ": unchanged; runtime capacity already meets the request.");
				return true;
			}
			if (pool->itemAllocCount != 0)
			{
				log(std::string(request.name) +
					": refused live reallocation because existing asset pointers may reference the old pool.",
					"error");
				return false;
			}

			const auto entry_size = static_cast<std::size_t>(pool->itemSize);
			const auto capacity = static_cast<std::size_t>(request.requested_size);
			if (capacity > (std::numeric_limits<std::size_t>::max)() / entry_size)
			{
				log(std::string(request.name) + ": requested allocation size overflows size_t.", "error");
				return false;
			}

			void* new_pool = std::calloc(capacity, entry_size);
			if (!new_pool)
			{
				log(std::string(request.name) + ": allocation failed for " +
					std::to_string(request.requested_size) + " entries.", "error");
				return false;
			}

			auto* base = static_cast<std::byte*>(new_pool);
			for (std::size_t index = 0; index + 1 < capacity; ++index)
			{
				auto* current = reinterpret_cast<game::AssetLink*>(base + index * entry_size);
				current->next = reinterpret_cast<game::AssetLink*>(base + (index + 1) * entry_size);
			}

			auto* last = reinterpret_cast<game::AssetLink*>(base + (capacity - 1) * entry_size);
			last->next = nullptr;

			pool->pool = new_pool;
			pool->itemSize = static_cast<std::uint32_t>(entry_size);
			pool->itemCount = static_cast<int>(request.requested_size);
			pool->itemAllocCount = 0;
			pool->freeHead = reinterpret_cast<game::AssetLink*>(new_pool);

			log(std::string(request.name) + ": allocation installed; capacity=" +
				std::to_string(pool->itemCount) + ", bytes=" +
				std::to_string(capacity * entry_size) +
				", free-list entries=" + std::to_string(capacity) + ".");
			return true;
		}

		bool apply_requests()
		{
			bool all_applied = true;
			for (auto& request : requests)
			{
				const auto applied = expand_pool(request);
				all_applied = applied && all_applied;
			}
			return all_applied;
		}

		void* __fastcall asset_pool_alloc_stub(const game::XAssetType type)
		{
			std::lock_guard<std::mutex> allocation_lock(pool_allocation_mutex);
			auto* const result = reinterpret_cast<void*(__fastcall*)(game::XAssetType)>(
				asset_pool_alloc_original)(type);

			for (std::size_t index = 0; index < requests.size(); ++index)
			{
				const auto& request = requests[index];
				if (type != request.type || !result)
					continue;

				const auto* pool = get_pool(type);
				if (!pool || !pool->pool || pool->itemSize == 0 || pool->itemCount <= 0)
					continue;

				const auto start = reinterpret_cast<std::uintptr_t>(pool->pool);
				const auto entry = reinterpret_cast<std::uintptr_t>(result);
				const auto allocated_bytes = static_cast<std::uintptr_t>(pool->itemSize) *
					static_cast<std::uintptr_t>(pool->itemCount);
				if (entry < start || entry - start >= allocated_bytes ||
					(entry - start) % pool->itemSize != 0)
					continue;

				const auto slot = (entry - start) / pool->itemSize;
				const auto baseline = (std::max)(request.stock_size, request.runtime_original);
				if (slot >= baseline &&
					!beyond_stock_reported[index].exchange(true))
				{
					log(std::string(request.name) +
						": engine allocator returned a non-null entry beyond the original capacity; slot=" +
						std::to_string(slot) + ", stock=" +
						std::to_string(request.stock_size) + ", runtime-original=" +
						std::to_string(request.runtime_original) + ", capacity=" +
						std::to_string(pool->itemCount) +
						". Registration and lookup still require runtime validation.",
						"warning");
				}
			}

			return result;
		}

		bool install_allocator_monitor()
		{
			std::uintptr_t allocator_rva{};
			if (!read_rva(build_key("DB_AssetPoolAlloc"), allocator_rva, false))
			{
				log("Allocator-use telemetry unavailable: add a verified DB_AssetPoolAlloc RVA to offsets.txt.",
					"warning");
				return false;
			}

			if (!resolve_rva(allocator_rva, 1, true, false, asset_pool_alloc_address))
			{
				log("Allocator-use telemetry unavailable: DB_AssetPoolAlloc RVA is not executable.",
					"warning");
				return false;
			}
			if (!initialize_minhook())
				return false;

			auto status = MH_CreateHook(reinterpret_cast<void*>(asset_pool_alloc_address),
				reinterpret_cast<void*>(&asset_pool_alloc_stub), &asset_pool_alloc_original);
			if (status == MH_OK)
				status = MH_EnableHook(reinterpret_cast<void*>(asset_pool_alloc_address));
			if (status != MH_OK)
			{
				MH_RemoveHook(reinterpret_cast<void*>(asset_pool_alloc_address));
				asset_pool_alloc_original = nullptr;
				log("Allocator-use telemetry unavailable: failed to install DB_AssetPoolAlloc hook.",
					"warning");
				return false;
			}

			log("Installed DB_AssetPoolAlloc monitor; beyond-stock slot use will be logged.");
			return true;
		}

		void __fastcall asset_pool_init_stub()
		{
			reinterpret_cast<void(__fastcall*)()>(asset_pool_init_original)();
			log("DB_AssetPoolInit completed; applying configured asset-pool requests.");
			apply_requests();
		}

		bool load_config()
		{
			std::ifstream input("T7Overclocked.ini");
			if (!input)
			{
				log("T7Overclocked.ini not found; asset pools will remain unchanged.", "error");
				return false;
			}

			std::string section;
			std::string line;
			while (std::getline(input, line))
			{
				line = trim(line);
				if (line.empty() || line.front() == '#' || line.front() == ';')
					continue;

				if (line.front() == '[' && line.back() == ']')
				{
					section = lower(trim(line.substr(1, line.size() - 2)));
					continue;
				}

				const auto separator = line.find('=');
				if (separator == std::string::npos)
					continue;

				const auto key = lower(trim(line.substr(0, separator)));
				const auto value = trim(line.substr(separator + 1));
				if (section == "t7overclocked")
				{
					if (key == "enabled")
					{
						if (!parse_bool(value, enabled))
						{
							enabled = false;
							log("Invalid [T7Overclocked] Enabled value; expected true or false.", "warning");
						}
					}
					else if (key == "loglevel")
					{
						log_level = lower(value);
						if (log_level != "debug" && log_level != "info" &&
							log_level != "warning" && log_level != "error")
						{
							log("Invalid LogLevel; using info.", "warning");
							log_level = "info";
						}
					}
				}
				else if (section == "assetpools")
				{
					const auto request = std::find_if(requests.begin(), requests.end(),
						[&key](const pool_request& pool)
						{
							return key == pool.name;
						});
					if (request == requests.end())
					{
						const auto parsed = parse_size(value);
						if (!parsed)
							log(std::string("Invalid capacity for ") + key + ": " + value + ".", "warning");
						if (parsed && *parsed == 0 && (key == "weapon" || key == "xmodel"))
						{
							log(std::string("Configuration: ") + key +
								" is unsupported but requests no change (capacity=0).");
							continue;
						}
						log(std::string("Unsupported asset pool '") + key +
							"'; this build currently supports attachment and attachmentunique only.",
							"warning");
						continue;
					}

					const auto parsed = parse_size(value);
					if (!parsed)
					{
						log(std::string("Invalid capacity for ") + key + ": " + value + ".", "warning");
						continue;
					}
					request->requested_size = *parsed;
					log(std::string("Configuration: ") + key + " requested capacity=" +
						std::to_string(request->requested_size) + ".");
				}
			}

			return true;
		}
	}

	bool initialize()
	{
		if (initialized_once)
			return true;
		initialized_once = true;

		if (!load_config())
			return false;
		if (!enabled)
		{
			log("Asset pool expansion disabled by configuration.");
			return true;
		}

		bool has_request = false;
		for (const auto& request : requests)
			has_request |= request.requested_size != 0;
		if (!has_request)
		{
			log("No asset-pool expansions requested.");
			return true;
		}
		if (!load_offsets())
			return false;

		bool initialized = true;
		for (const auto& request : requests)
		{
			if (request.requested_size == 0)
				continue;
			const auto* pool = get_pool(request.type);
			initialized &= pool && pool->itemCount > 0 && pool->pool;
		}

		if (initialized)
		{
			log("Asset pools are already initialized; live pools with allocated entries will not be relocated.",
				"warning");
			if (!install_allocator_monitor())
			{
				log("Refusing to resize initialized pools without the allocator hook needed to serialize pool changes.",
					"error");
				return false;
			}
			if (!apply_requests())
				return false;
			return true;
		}

		std::uintptr_t init_rva{};
		if (!read_rva(build_key("DB_AssetPoolInit"), init_rva) ||
			!resolve_rva(init_rva, 1, true, false, asset_pool_init_address))
		{
			log("Asset pool expansion disabled: DB_AssetPoolInit is not configured or is not executable.",
				"error");
			return false;
		}

		if (!initialize_minhook())
			return false;

		auto status = MH_CreateHook(reinterpret_cast<void*>(asset_pool_init_address),
			reinterpret_cast<void*>(&asset_pool_init_stub), &asset_pool_init_original);
		if (status != MH_OK)
		{
			log("Asset pool expansion disabled: MinHook could not create the DB_AssetPoolInit hook.",
				"error");
			return false;
		}
		status = MH_EnableHook(reinterpret_cast<void*>(asset_pool_init_address));
		if (status != MH_OK)
		{
			MH_RemoveHook(reinterpret_cast<void*>(asset_pool_init_address));
			asset_pool_init_original = nullptr;
			log("Asset pool expansion disabled: MinHook could not enable the DB_AssetPoolInit hook.",
				"error");
			return false;
		}

		install_allocator_monitor();
		log(std::string("Installed DB_AssetPoolInit hook for ") +
			game::offsets::build_name() + ".");
		return true;
	}
}
