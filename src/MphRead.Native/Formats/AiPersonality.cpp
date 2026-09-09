#include "../Entities/Players/ai_func_names.hpp"
#include "Formats/ai_personality_layouts.hpp"
#include "Formats/ai_personality.hpp"

#include "Read.hpp"

#include <algorithm>
#include <array>
#include <limits>
#include <queue>
#include <stdexcept>
#include <unordered_map>
#include <unordered_set>

namespace fruityprime::ai {
namespace {

using ByteSpan = std::span<const std::uint8_t>;

[[nodiscard]] std::size_t checked_offset(std::int32_t offset,
                                         ByteSpan bytes,
                                         const char* description) {
    if (offset < 0 || static_cast<std::size_t>(offset) > bytes.size()) {
        throw std::out_of_range(std::string(description)
                                + " is outside the AI personality file");
    }
    return static_cast<std::size_t>(offset);
}

template <typename T>
[[nodiscard]] std::vector<T> read_records(ByteSpan bytes,
                                           std::int32_t offset,
                                           std::int32_t count,
                                           const char* description) {
    if (count < 0) {
        throw std::runtime_error(std::string(description)
                                 + " has a negative count");
    }
    if (count == 0 || offset == 0) {
        return {};
    }
    const std::size_t start = checked_offset(offset, bytes, description);
    return read::do_offsets<T>(bytes, start, static_cast<std::size_t>(count));
}

class Parser {
public:
    explicit Parser(ByteSpan bytes) : bytes_(bytes) {}

    [[nodiscard]] Personality parse(std::size_t root_offset) {
        if (root_offset == 0) {
            throw std::invalid_argument("AI personality root offset is zero");
        }
        if (root_offset > bytes_.size()) {
            throw std::out_of_range(
                "AI personality root offset is outside the file");
        }
        const auto roots = parse_data1(static_cast<std::int32_t>(root_offset), 1);
        if (roots.empty() || roots.front() == nullptr) {
            throw std::runtime_error("AI personality root record is empty");
        }
        Personality result{roots.front(), node_count_};
        result.set_labels();
        return result;
    }

private:
    [[nodiscard]] Parameters parse_parameters(std::int32_t type,
                                              std::int32_t offset) {
        if (offset == 0) {
            return {};
        }
        if (const auto found = parameter_cache_.find(offset);
            found != parameter_cache_.end()) {
            return found->second;
        }
        const std::size_t start = checked_offset(offset, bytes_,
                                                 "AI parameter offset");
        Parameters result;
        result.param1 = read::read_struct<std::int32_t>(bytes_, start);
        if (type == 210) {
            result.param2 = read::read_struct<std::int32_t>(bytes_, start + 4);
        }
        result.is_empty = false;
        parameter_cache_.emplace(offset, result);
        return result;
    }

    [[nodiscard]] std::vector<Data4> parse_data4(std::int32_t offset,
                                                 std::int32_t count) {
        if (count < 0) {
            throw std::runtime_error("AI data4 has a negative count");
        }
        if (count == 0 || offset == 0) {
            return {};
        }
        if (const auto found = data4_cache_.find(offset);
            found != data4_cache_.end()) {
            return found->second;
        }
        const auto raw = read_records<AiData4>(
            bytes_, offset, count, "AI data4 table");
        std::vector<Data4> result;
        result.reserve(raw.size());
        for (const auto& entry : raw) {
            result.push_back({entry.data5_type,
                              parse_parameters(entry.data5_type,
                                               entry.data5_offset)});
        }
        data4_cache_.emplace(offset, result);
        return result;
    }

    [[nodiscard]] std::vector<Data2> parse_data2(std::int32_t offset,
                                                 std::int32_t count) {
        if (count < 0) {
            throw std::runtime_error("AI data2 has a negative count");
        }
        if (count == 0 || offset == 0) {
            return {};
        }
        if (const auto found = data2_cache_.find(offset);
            found != data2_cache_.end()) {
            return found->second;
        }
        const auto raw = read_records<AiData2>(
            bytes_, offset, count, "AI data2 table");
        std::vector<Data2> result;
        result.reserve(raw.size());
        for (const auto& entry : raw) {
            result.push_back({entry.field_c,
                              entry.field_10,
                              parse_data4(entry.data4_offset,
                                          entry.data4_count),
                              entry.data5_type,
                              parse_parameters(entry.data5_type,
                                               entry.data5_offset)});
        }
        data2_cache_.emplace(offset, result);
        return result;
    }

    [[nodiscard]] std::vector<Data1Ptr> parse_data1(std::int32_t offset,
                                                    std::int32_t count) {
        if (count < 0) {
            throw std::runtime_error("AI data1 has a negative count");
        }
        if (count == 0 || offset == 0) {
            return {};
        }
        if (const auto found = data1_cache_.find(offset);
            found != data1_cache_.end()) {
            return found->second;
        }
        if (!active_data1_offsets_.insert(offset).second) {
            throw std::runtime_error("AI data1 contains a cyclic offset");
        }

        const auto raw = read_records<AiData1>(
            bytes_, offset, count, "AI data1 table");
        std::vector<Data1Ptr> result;
        result.reserve(raw.size());
        for (const auto& entry : raw) {
            auto node = std::make_shared<Data1>();
            node->func24_id = entry.field0;
            node->data1 = entry.data1_count > 0
                && entry.data1_offset != offset
                ? parse_data1(entry.data1_offset, entry.data1_count)
                : std::vector<Data1Ptr>{};
            node->data2 = parse_data2(entry.data2_offset, entry.data2_count);
            node->data3a = read_records<std::int32_t>(
                bytes_, entry.data3a_offset, entry.data3a_count,
                "AI data3a table");
            node->data3b = read_records<std::int32_t>(
                bytes_, entry.data3b_offset, entry.data3b_count,
                "AI data3b table");
            for (const auto& child : node->data1) {
                if (child != nullptr) {
                    child->parent = node;
                }
            }
            result.push_back(std::move(node));
            ++node_count_;
        }
        active_data1_offsets_.erase(offset);
        data1_cache_.emplace(offset, result);
        return result;
    }

    ByteSpan bytes_;
    std::size_t node_count_ = 0;
    std::unordered_map<std::int32_t, std::vector<Data1Ptr>> data1_cache_;
    std::unordered_map<std::int32_t, std::vector<Data2>> data2_cache_;
    std::unordered_map<std::int32_t, std::vector<Data4>> data4_cache_;
    std::unordered_map<std::int32_t, Parameters> parameter_cache_;
    std::unordered_set<std::int32_t> active_data1_offsets_;
};

[[nodiscard]] std::string label_for(std::size_t id) {
    if (id == 0) {
        return "Root";
    }
    std::string result;
    std::size_t value = id - 1;
    while (true) {
        result.insert(result.begin(),
                      static_cast<char>('A' + (value % 26)));
        if (value < 26) {
            break;
        }
        value = value / 26 - 1;
    }
    return result;
}

} // namespace

void Personality::set_labels() {
    if (root == nullptr) {
        return;
    }
    std::queue<Data1Ptr> queue;
    queue.push(root);
    std::size_t id = 0;
    while (!queue.empty()) {
        const std::size_t level_count = queue.size();
        for (std::size_t i = 0; i < level_count; ++i) {
            const Data1Ptr node = queue.front();
            queue.pop();
            if (node == nullptr) {
                continue;
            }
            node->label = label_for(id++);
            for (const auto& child : node->data1) {
                queue.push(child);
            }
        }
    }
}

std::int32_t select_offset(std::uint8_t mode,
                           std::int32_t encounter_state,
                           std::uint8_t hunter) noexcept {
    // Battle and BattleTeams use the default personality.  This also keeps
    // the managed switch's default behavior for unknown modes.
    if (mode == 5 || mode == 6) {
        return 45696;
    }
    if (mode == 7 || mode == 8 || mode == 9) {
        return 32968;
    }
    if (mode == 10 || mode == 11 || mode == 12 || mode == 13) {
        return 33012;
    }
    if (mode == 14) {
        return 45220;
    }
    if (mode != 2) {
        return 32896;
    }

    if (hunter == 7) {
        return encounter_state == 2 ? 32932 : 13480;
    }
    if (encounter_state == 2) {
        return 33232;
    }
    constexpr std::array<std::array<std::int32_t, 8>, 4> offsets{{
        {{33152, 33152, 33696, 33836, 33556, 33372, 33976, 13480}},
        {{33152, 33196, 37576, 41948, 35428, 33416, 41492, 13480}},
        {{33152, 33152, 39420, 42772, 33556, 40312, 33976, 13480}},
        {{33152, 33152, 33696, 45176, 33556, 40556, 33976, 13480}}
    }};
    std::size_t index = 0;
    if (encounter_state == 1) {
        index = 1;
    } else if (encounter_state == 3) {
        index = 2;
    } else if (encounter_state == 4) {
        index = 3;
    }
    return offsets[index][std::min<std::size_t>(hunter, 7)];
}

Personality File::parse(ByteSpan bytes, std::size_t root_offset) {
    return Parser(bytes).parse(root_offset);
}

Personality File::parse(const std::vector<std::uint8_t>& bytes,
                        std::size_t root_offset) {
    return parse(ByteSpan(bytes), root_offset);
}

Personality File::read_file(const std::filesystem::path& path,
                            std::size_t root_offset) {
    const auto bytes = read::file(path);
    return parse(bytes, root_offset);
}

namespace {

[[nodiscard]] std::string join_ids(const std::vector<std::int32_t>& ids) {
    std::string out;
    for (std::size_t i = 0; i < ids.size(); ++i) {
        if (i != 0) {
            out += ", ";
        }
        out += std::to_string(ids[i]);
    }
    return out;
}

[[nodiscard]] std::string join_names(
    const std::vector<std::string_view>& names) {
    std::string out;
    for (std::size_t i = 0; i < names.size(); ++i) {
        if (i != 0) {
            out += ", ";
        }
        out += std::string(names[i]);
    }
    return out;
}

[[nodiscard]] std::string pad_left(const std::string& value,
                                   std::size_t width) {
    return value.size() >= width
        ? value
        : std::string(width - value.size(), ' ') + value;
}

// Both lists are ids into the same funcs1 table, so they render the same way.
[[nodiscard]] std::string describe_funcs1(
    const std::vector<std::int32_t>& ids) {
    if (ids.empty()) {
        return "-";
    }
    std::vector<std::string_view> names;
    names.reserve(ids.size());
    for (const std::int32_t id : ids) {
        names.push_back(players::funcs1_name(id));
    }
    return join_ids(ids) + " -> " + join_names(names);
}

} // namespace

std::string print_node(const Data1& node) {
    std::string out;
    out += node.label;
    out += "\n";
    out += "Init (3a): " + describe_funcs1(node.data3a) + "\n";
    // The same id names two different subroutines: one that runs when the node
    // is entered and one that runs every frame it is active.
    std::string init_f4 = "-";
    std::string proc_f2 = "-";
    if (node.func24_id != 0) {
        init_f4 = std::to_string(node.func24_id) + " -> "
            + std::string(players::funcs4_name(node.func24_id));
        proc_f2 = std::to_string(node.func24_id) + " -> "
            + std::string(players::funcs2_name(node.func24_id));
    }
    out += "Init (F4): " + init_f4 + "\n";
    out += "Proc (3b): " + describe_funcs1(node.data3b) + "\n";
    out += "Proc (F2): " + proc_f2 + "\n";
    if (node.data2.empty()) {
        out += "Switch(x): -\n";
        return out;
    }
    // The columns are padded to the widest value so the branches line up and
    // the weights can be compared by eye.
    std::size_t pad1 = std::to_string(node.data2.size() - 1).size();
    std::size_t pad2 = 1;
    std::size_t pad3 = 1;
    for (const Data2& data2 : node.data2) {
        pad2 = std::max(pad2, std::to_string(data2.func3_id).size());
        pad3 = std::max(pad3, std::to_string(data2.weight).size());
    }
    const Data1Ptr parent = node.parent.lock();
    for (std::size_t i = 0; i < node.data2.size(); ++i) {
        const Data2& data2 = node.data2[i];
        const std::string index = pad_left(std::to_string(i), pad1);
        if (!data2.data4.empty()) {
            std::vector<std::int32_t> ids;
            std::vector<std::string_view> names;
            ids.reserve(data2.data4.size());
            names.reserve(data2.data4.size());
            for (const Data4& data4 : data2.data4) {
                ids.push_back(data4.func3_id);
                names.push_back(players::funcs3_name(data4.func3_id));
            }
            out += "Precon(" + index + "): " + join_ids(ids) + " -> "
                + join_names(names) + "\n";
        }
        std::string selection = "-";
        if (data2.data1_select_index < 20 && parent != nullptr
            && data2.data1_select_index >= 0
            && static_cast<std::size_t>(data2.data1_select_index)
                   < parent->data1.size()) {
            selection =
                parent->data1[static_cast<std::size_t>(
                    data2.data1_select_index)]->label;
        }
        out += "Switch(" + index + "): "
            + pad_left(std::to_string(data2.func3_id), pad2) + ", "
            + pad_left(std::to_string(data2.weight), pad3) + ", s = "
            + std::to_string(data2.data1_select_index) + " (" + selection
            + ") -> " + std::string(players::funcs3_name(data2.func3_id))
            + "\n";
    }
    return out;
}

std::string print_all(const Data1& root) {
    std::string out;
    // Breadth first, with a rule between depths: the tree is wide and shallow,
    // so a depth-first dump would separate siblings that are read together.
    std::vector<const Data1*> level{&root};
    while (!level.empty()) {
        std::vector<const Data1*> next;
        for (const Data1* node : level) {
            out += print_node(*node);
            out += "\n";
            for (const Data1Ptr& child : node->data1) {
                if (child != nullptr) {
                    next.push_back(child.get());
                }
            }
        }
        if (!next.empty()) {
            out += std::string(100, '-');
            out += "\n\n";
        }
        level = std::move(next);
    }
    return out;
}

} // namespace fruityprime::ai
