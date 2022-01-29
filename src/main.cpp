#include <algorithm>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <filesystem>
#include <iostream>
#include <source_location>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <binary_io/span_stream.hpp>
#include <fmt/format.h>
#include <mmio/mmio.hpp>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/spdlog.h>

using namespace std::literals;

namespace
{
	template <class... Args>
	struct [[maybe_unused]] raise_error
	{
		raise_error() = delete;

		[[noreturn]] explicit raise_error(
			fmt::format_string<Args...> a_fmt,
			Args&&... a_args,
			std::source_location a_loc = std::source_location::current())
		{
			auto msg = fmt::format(
				"{}({}): {}"sv,
				a_loc.file_name(),
				a_loc.line(),
				fmt::format(a_fmt, std::forward<Args>(a_args)...));

			throw std::runtime_error(std::move(msg));
		}
	};

	template <class... Args>
	raise_error(fmt::format_string<Args...>, Args&&...) -> raise_error<Args...>;

	struct header_t
	{
		std::int32_t format = 0;
		std::int32_t version[4] = {};
		std::int32_t pointer_size = 0;
		std::int32_t address_count = 0;
	};

	[[nodiscard]] auto read_header(binary_io::span_istream& a_in)
		-> header_t
	{
		header_t h;

		a_in.read(h.format);
		switch (h.format) {
		case 1:
		case 2:
			break;
		default:
			raise_error("invalid header version ({})"sv, h.format);
		}

		a_in.read(h.version[0], h.version[1], h.version[2], h.version[3]);

		const auto [nameLen] = a_in.read<std::int32_t>();
		a_in.seek_relative(nameLen);

		a_in.read(h.pointer_size, h.address_count);

		return h;
	}

	[[nodiscard]] auto read_file(binary_io::span_istream& a_in)
		-> std::vector<std::pair<std::uint64_t, std::uint64_t>>
	{
		const auto header = read_header(a_in);
		auto mappings = decltype(read_file(a_in))(header.address_count);

		std::uint8_t type = 0;
		std::uint64_t id = 0;
		std::uint64_t offset = 0;
		std::uint64_t prevID = 0;
		std::uint64_t prevOffset = 0;
		for (auto& mapping : mappings) {
			a_in.read(type);
			const auto lo = static_cast<std::uint8_t>(type & 0xF);
			const auto hi = static_cast<std::uint8_t>(type >> 4);

			switch (lo) {
			case 0:
				a_in.read(id);
				break;
			case 1:
				id = prevID + 1;
				break;
			case 2:
				id = prevID + std::get<0>(a_in.read<std::uint8_t>());
				break;
			case 3:
				id = prevID - std::get<0>(a_in.read<std::uint8_t>());
				break;
			case 4:
				id = prevID + std::get<0>(a_in.read<std::uint16_t>());
				break;
			case 5:
				id = prevID - std::get<0>(a_in.read<std::uint16_t>());
				break;
			case 6:
				std::tie(id) = a_in.read<std::uint16_t>();
				break;
			case 7:
				std::tie(id) = a_in.read<std::uint32_t>();
				break;
			default:
				raise_error("unhandled type"sv);
			}

			const std::uint64_t tmp = (hi & 8) != 0 ? (prevOffset / header.pointer_size) : prevOffset;

			switch (hi & 7) {
			case 0:
				a_in.read(offset);
				break;
			case 1:
				offset = tmp + 1;
				break;
			case 2:
				offset = tmp + std::get<0>(a_in.read<std::uint8_t>());
				break;
			case 3:
				offset = tmp - std::get<0>(a_in.read<std::uint8_t>());
				break;
			case 4:
				offset = tmp + std::get<0>(a_in.read<std::uint16_t>());
				break;
			case 5:
				offset = tmp - std::get<0>(a_in.read<std::uint16_t>());
				break;
			case 6:
				std::tie(offset) = a_in.read<std::uint16_t>();
				break;
			case 7:
				std::tie(offset) = a_in.read<std::uint32_t>();
				break;
			default:
				raise_error("unhandled type"sv);
			}

			if ((hi & 8) != 0) {
				offset *= header.pointer_size;
			}

			mapping = { id, offset };

			prevOffset = offset;
			prevID = id;
		}

		return mappings;
	}

	void dump_mappings(
		binary_io::span_istream& a_in,
		spdlog::logger& a_out)
	{
		auto mappings = read_file(a_in);
		std::sort(
			mappings.begin(),
			mappings.end(),
			[](const auto& a_lhs, const auto& a_rhs) {
				return a_lhs.first < a_rhs.first;
			});

		const auto len = mappings.empty() ? 0 : fmt::to_string(mappings.back().first).length();
		for (const auto& [id, address] : mappings) {
			a_out.info("{1: >{0}}\t{2:07X}"sv, len, id, address);
		}
	}

	void do_main(std::filesystem::path a_lib)
	{
		const auto lib = mmio::mapped_file_source(a_lib);
		auto in = binary_io::span_istream(std::span{ lib.data(), lib.size() });

		const auto out = spdlog::basic_logger_st(
			"out"s,
			a_lib.replace_extension("txt"sv).string());
		out->set_pattern("%v"s);
		out->set_level(spdlog::level::trace);

		dump_mappings(in, *out);
	}
}

int wmain(int a_argc, wchar_t** a_argv)
{
	try {
		if (a_argc == 2) {
			do_main(a_argv[1]);
		} else {
			raise_error("expected only 1 argument (the file path): got {}"sv, a_argc - 1);
		}
	} catch (const std::exception& a_err) {
		std::cout << a_err.what() << '\n';
	}

	return EXIT_FAILURE;
}
