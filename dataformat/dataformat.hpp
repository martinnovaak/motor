#ifndef MOTOR_DATAFORMAT_HPP
#define MOTOR_DATAFORMAT_HPP

#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <vector>

namespace motor::dataformat {

constexpr std::array<char, 8> MAGIC = {'M', 'O', 'T', 'O', 'R', 'D', 'S', '\0'};
constexpr std::uint16_t VERSION = 1;
constexpr std::uint16_t HEADER_SIZE = 64;
constexpr std::uint16_t RECORD_SIZE = 32;

enum HeaderFlag : std::uint16_t {
    Incomplete = 1 << 0,
};

enum class Result : std::uint8_t {
    Loss = 0,
    Draw = 1,
    Win = 2,
    Unknown = 3,
};

constexpr std::uint8_t NO_EN_PASSANT = 8;

struct PositionState {
    std::uint8_t castling_rights = 0;
    std::uint8_t en_passant_file = NO_EN_PASSANT;
    std::uint8_t halfmove_clock = 0;
};

struct alignas(8) DatasetHeader {
    std::array<char, 8> magic = MAGIC;
    std::uint16_t version = VERSION;
    std::uint16_t header_size = HEADER_SIZE;
    std::uint16_t record_size = RECORD_SIZE;
    std::uint16_t flags = Incomplete;
    std::uint64_t record_count = 0;
    std::uint64_t game_count = 0;
    std::uint64_t seed = 0;
    std::uint64_t nodes_per_move = 0;
    std::uint64_t created_unix_seconds = 0;
    std::array<std::uint8_t, 8> reserved{};
};

// Side-to-move-relative A1=0 position. The side to move is always represented
// as white. Black-to-move positions are rank-flipped and colour-swapped while
// packing. Kings are stored separately and are not present in occupancy. The
// remaining piece nibbles are stored in ascending occupied-square order:
// 0..4 = our Pawn..Queen, 8..12 = their Pawn..Queen. extra[0] stores
// canonical castling rights in its low nibble and en-passant file + 1 in its
// high nibble (zero means none). extra[1] stores the halfmove clock.
struct alignas(8) PositionRecord {
    std::uint64_t occupancy = 0;
    std::array<std::uint8_t, 15> pieces{};
    Result result_stm = Result::Unknown;
    std::uint16_t best_move = 0;
    std::int16_t score_stm = 0;
    std::uint8_t king_stm = 0;
    std::uint8_t king_nstm = 0;
    std::array<std::uint8_t, 2> extra{};
};

static_assert(std::endian::native == std::endian::little,
              "Motor datasets require a little-endian target");
static_assert(std::is_trivially_copyable_v<DatasetHeader>);
static_assert(std::is_trivially_copyable_v<PositionRecord>);
static_assert(sizeof(DatasetHeader) == HEADER_SIZE);
static_assert(sizeof(PositionRecord) == RECORD_SIZE);
static_assert(offsetof(PositionRecord, occupancy) == 0);
static_assert(offsetof(PositionRecord, pieces) == 8);
static_assert(offsetof(PositionRecord, result_stm) == 23);
static_assert(offsetof(PositionRecord, best_move) == 24);
static_assert(offsetof(PositionRecord, score_stm) == 26);
static_assert(offsetof(PositionRecord, king_stm) == 28);
static_assert(offsetof(PositionRecord, king_nstm) == 29);
static_assert(offsetof(PositionRecord, extra) == 30);

// White, Black, Pawn, Knight, Bishop, Rook, Queen, King.
using Bitboards = std::array<std::uint64_t, 8>;

constexpr std::uint16_t canonical_move(std::uint16_t move, bool black_to_move) {
    if (!black_to_move || move == 0) {
        return move;
    }
    constexpr std::uint16_t square_mask = 0x3f;
    const auto from = static_cast<std::uint16_t>((move & square_mask) ^ 56);
    const auto to = static_cast<std::uint16_t>(((move >> 6) & square_mask) ^ 56);
    return static_cast<std::uint16_t>((move & 0xf000) | from | (to << 6));
}

constexpr std::uint8_t canonical_castling(std::uint8_t rights,
                                          bool black_to_move) {
    rights &= 0x0f;
    return black_to_move
            ? static_cast<std::uint8_t>(((rights & 0x03) << 2) |
                                        ((rights & 0x0c) >> 2))
            : rights;
}

constexpr std::uint8_t castling_rights(const PositionRecord& record) {
    return record.extra[0] & 0x0f;
}

constexpr std::uint8_t en_passant_file(const PositionRecord& record) {
    const auto encoded = static_cast<std::uint8_t>(record.extra[0] >> 4);
    return encoded == 0 ? NO_EN_PASSANT
                        : static_cast<std::uint8_t>(encoded - 1);
}

constexpr std::uint8_t halfmove_clock(const PositionRecord& record) {
    return record.extra[1];
}

inline PositionRecord pack(const Bitboards& boards, bool black_to_move,
                           std::int16_t score_stm, std::uint16_t best_move,
                           PositionState state = {}) {
    if (state.en_passant_file > NO_EN_PASSANT) {
        throw std::invalid_argument("invalid en-passant file");
    }
    PositionRecord record;
    record.score_stm = score_stm;
    record.best_move = canonical_move(best_move, black_to_move);
    const auto encoded_en_passant = static_cast<std::uint8_t>(
            state.en_passant_file == NO_EN_PASSANT ? 0 : state.en_passant_file + 1);
    record.extra[0] = static_cast<std::uint8_t>(
            canonical_castling(state.castling_rights, black_to_move) |
            (encoded_en_passant << 4));
    record.extra[1] = state.halfmove_clock;

    const auto white_king = static_cast<std::uint8_t>(
            std::countr_zero(boards[0] & boards[7]));
    const auto black_king = static_cast<std::uint8_t>(
            std::countr_zero(boards[1] & boards[7]));
    record.king_stm = static_cast<std::uint8_t>(
            black_to_move ? black_king ^ 56 : white_king);
    record.king_nstm = static_cast<std::uint8_t>(
            black_to_move ? white_king ^ 56 : black_king);

    auto source_occupancy = (boards[0] | boards[1]) & ~boards[7];
    while (source_occupancy != 0) {
        const auto square = static_cast<std::uint8_t>(std::countr_zero(source_occupancy));
        source_occupancy &= source_occupancy - 1;
        const auto canonical_square = static_cast<std::uint8_t>(
                black_to_move ? square ^ 56 : square);
        record.occupancy |= 1ULL << canonical_square;
    }

    auto occupancy = record.occupancy;
    std::size_t index = 0;
    while (occupancy != 0) {
        const auto canonical_square = static_cast<std::uint8_t>(std::countr_zero(occupancy));
        occupancy &= occupancy - 1;
        const auto source_square = static_cast<std::uint8_t>(
                black_to_move ? canonical_square ^ 56 : canonical_square);
        const std::uint64_t source_bit = 1ULL << source_square;

        std::uint8_t piece = 0;
        while ((boards[2 + piece] & source_bit) == 0) {
            piece++;
        }
        const bool source_is_black = (boards[1] & source_bit) != 0;
        const bool canonical_is_black = source_is_black != black_to_move;
        piece |= static_cast<std::uint8_t>(canonical_is_black) << 3;
        record.pieces[index / 2] |= static_cast<std::uint8_t>(piece << (4 * (index & 1)));
        index++;
    }
    return record;
}

inline std::string to_fen(const PositionRecord& record) {
    if (record.king_stm >= 64 || record.king_nstm >= 64 ||
        record.king_stm == record.king_nstm ||
        (record.occupancy & ((1ULL << record.king_stm) |
                             (1ULL << record.king_nstm))) != 0) {
        throw std::invalid_argument("invalid king squares in position record");
    }

    constexpr std::array<char, 16> piece_chars = {
        'P', 'N', 'B', 'R', 'Q', '\0', '\0', '\0',
        'p', 'n', 'b', 'r', 'q', '\0', '\0', '\0',
    };
    std::array<char, 64> squares{};
    squares[record.king_stm] = 'K';
    squares[record.king_nstm] = 'k';

    auto occupancy = record.occupancy;
    std::size_t piece_index = 0;
    while (occupancy != 0) {
        if (piece_index >= 30) {
            throw std::invalid_argument("too many pieces in position record");
        }
        const auto square = static_cast<std::uint8_t>(std::countr_zero(occupancy));
        occupancy &= occupancy - 1;
        const auto code = static_cast<std::uint8_t>(
                (record.pieces[piece_index / 2] >> (4 * (piece_index & 1))) & 0x0f);
        if (piece_chars[code] == '\0') {
            throw std::invalid_argument("invalid piece code in position record");
        }
        squares[square] = piece_chars[code];
        piece_index++;
    }

    std::string fen;
    for (int rank = 7; rank >= 0; rank--) {
        int empty = 0;
        for (int file = 0; file < 8; file++) {
            const char piece = squares[rank * 8 + file];
            if (piece == '\0') {
                empty++;
            } else {
                if (empty != 0) {
                    fen += static_cast<char>('0' + empty);
                    empty = 0;
                }
                fen += piece;
            }
        }
        if (empty != 0) {
            fen += static_cast<char>('0' + empty);
        }
        if (rank != 0) {
            fen += '/';
        }
    }

    fen += " w ";
    const auto rights = castling_rights(record);
    if (rights == 0) {
        fen += '-';
    } else {
        if ((rights & 1) != 0) fen += 'K';
        if ((rights & 2) != 0) fen += 'Q';
        if ((rights & 4) != 0) fen += 'k';
        if ((rights & 8) != 0) fen += 'q';
    }

    fen += ' ';
    const auto ep_file = en_passant_file(record);
    if (ep_file == NO_EN_PASSANT) {
        fen += '-';
    } else {
        if (ep_file >= 8) {
            throw std::invalid_argument("invalid en-passant file in position record");
        }
        fen += static_cast<char>('a' + ep_file);
        fen += '6';
    }
    fen += ' ' + std::to_string(halfmove_clock(record)) + " 1";
    return fen;
}

constexpr Result relative_result(Result white_result, bool black_to_move) {
    if (white_result == Result::Unknown) {
        return Result::Unknown;
    }
    const auto value = static_cast<std::uint8_t>(white_result);
    return static_cast<Result>(black_to_move ? 2 - value : value);
}

inline void set_game_result(PositionRecord& record, Result white_result,
                            bool black_to_move) {
    record.result_stm = relative_result(white_result, black_to_move);
}

class Writer {
public:
    Writer(const std::filesystem::path& path, std::uint64_t seed,
           std::uint64_t nodes_per_move, std::uint64_t created_unix_seconds,
           std::size_t buffer_records = 32768)
        : output(path, std::ios::binary | std::ios::trunc) {
        if (!output) {
            throw std::runtime_error("failed to create dataset: " + path.string());
        }
        header.seed = seed;
        header.nodes_per_move = nodes_per_move;
        header.created_unix_seconds = created_unix_seconds;
        write_header();
        buffer.reserve(buffer_records);
    }

    Writer(const Writer&) = delete;
    Writer& operator=(const Writer&) = delete;

    ~Writer() {
        try {
            close();
        } catch (...) {
        }
    }

    void write(const PositionRecord& record) {
        if (closed) {
            throw std::runtime_error("dataset writer is closed");
        }
        buffer.push_back(record);
        if (buffer.size() == buffer.capacity()) {
            flush();
        }
    }

    void finish_game() {
        header.game_count++;
    }

    void flush() {
        if (buffer.empty()) {
            return;
        }
        output.write(reinterpret_cast<const char*>(buffer.data()),
                     static_cast<std::streamsize>(buffer.size() * sizeof(PositionRecord)));
        if (!output) {
            throw std::runtime_error("failed to write dataset");
        }
        header.record_count += buffer.size();
        buffer.clear();
    }

    void close() {
        if (closed) {
            return;
        }
        flush();
        header.flags &= static_cast<std::uint16_t>(~Incomplete);
        output.seekp(0, std::ios::beg);
        write_header();
        output.close();
        if (!output) {
            throw std::runtime_error("failed to close dataset");
        }
        closed = true;
    }

    // Close a partially generated dataset without clearing the Incomplete bit.
    // Readers can still recover all complete fixed-size records from its size.
    void abort() {
        if (closed) {
            return;
        }
        flush();
        output.close();
        closed = true;
    }

    [[nodiscard]] std::uint64_t size() const {
        return header.record_count + buffer.size();
    }

    [[nodiscard]] const DatasetHeader& get_header() const {
        return header;
    }

private:
    void write_header() {
        output.write(reinterpret_cast<const char*>(&header), sizeof(header));
        if (!output) {
            throw std::runtime_error("failed to write dataset header");
        }
    }

    std::ofstream output;
    DatasetHeader header{};
    std::vector<PositionRecord> buffer;
    bool closed = false;
};

class Reader {
public:
    explicit Reader(const std::filesystem::path& path) : input(path, std::ios::binary) {
        if (!input) {
            throw std::runtime_error("failed to open dataset: " + path.string());
        }
        input.read(reinterpret_cast<char*>(&header), sizeof(header));
        if (!input || header.magic != MAGIC || header.version != VERSION ||
            header.header_size != HEADER_SIZE || header.record_size != RECORD_SIZE) {
            throw std::runtime_error("unsupported Motor dataset");
        }

        const auto bytes = std::filesystem::file_size(path);
        if (bytes < HEADER_SIZE || (bytes - HEADER_SIZE) % RECORD_SIZE != 0) {
            throw std::runtime_error("truncated Motor dataset");
        }
        const auto records_in_file = (bytes - HEADER_SIZE) / RECORD_SIZE;
        if ((header.flags & Incomplete) != 0) {
            header.record_count = records_in_file;
        } else if (records_in_file != header.record_count) {
            throw std::runtime_error("invalid Motor dataset record count");
        }
    }

    bool read(PositionRecord& record) {
        if (records_read == header.record_count) {
            return false;
        }
        input.read(reinterpret_cast<char*>(&record), sizeof(record));
        if (!input) {
            throw std::runtime_error("failed to read Motor dataset");
        }
        records_read++;
        return true;
    }

    [[nodiscard]] const DatasetHeader& get_header() const {
        return header;
    }

private:
    std::ifstream input;
    DatasetHeader header{};
    std::uint64_t records_read = 0;
};

} // namespace motor::dataformat

#endif // MOTOR_DATAFORMAT_HPP
