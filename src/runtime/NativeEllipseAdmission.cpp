#include "NativeEllipseAdmission.hpp"

#include <rapidjson/document.h>

#include <cmath>
#include <cstddef>
#include <initializer_list>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace avemotion::runtime::detail {
namespace {

using Code = NativeEllipseAdmissionCode;
using Value = rapidjson::Value;
using Range = std::pair<double, double>;

std::string pointerChild(std::string_view parent, std::string_view key) {
    std::string result{parent};
    if (result == "/") result.clear();
    result.push_back('/');
    for (char character : key) {
        if (character == '~') result += "~0";
        else if (character == '/') result += "~1";
        else result.push_back(character);
    }
    return result;
}

std::string pointerIndex(std::string_view parent, rapidjson::SizeType index) {
    return pointerChild(parent, std::to_string(index));
}

std::string_view memberName(const Value::ConstMemberIterator& member) {
    return {member->name.GetString(), member->name.GetStringLength()};
}

const Value* field(const Value& object, std::string_view key) {
    for (auto member = object.MemberBegin(); member != object.MemberEnd(); ++member) {
        if (memberName(member) == key) return &member->value;
    }
    return nullptr;
}

class Auditor final {
public:
    NativeEllipseAdmission run(const Value& root) {
        if (!resources(root)) return result_;
        rootObject(root);
        return result_;
    }

private:
    NativeEllipseAdmission result_{Code::Accepted, {}};

    bool reject(Code code, std::string path) {
        result_ = {code, std::move(path)};
        return false;
    }

    bool resources(const Value& root) {
        struct Node {
            const Value* value;
            std::size_t parent;
            std::string_view name;
            rapidjson::SizeType index;
            bool indexed;
            std::size_t depth;
        };
        std::vector<Node> nodes;
        nodes.reserve(4096);
        nodes.push_back({&root, 0, {}, 0, false, 1});
        const auto pathOf = [&nodes](std::size_t id) {
            std::vector<std::size_t> ancestors;
            while (id != 0) {
                ancestors.push_back(id);
                id = nodes[id].parent;
            }
            std::string path = "/";
            for (auto ancestor = ancestors.rbegin(); ancestor != ancestors.rend(); ++ancestor) {
                const auto& node = nodes[*ancestor];
                path = node.indexed ? pointerIndex(path, node.index)
                                    : pointerChild(path, node.name);
            }
            return path;
        };
        for (std::size_t currentIndex = 0; currentIndex < nodes.size(); ++currentIndex) {
            const Node current = nodes[currentIndex];
            if ((current.value->IsObject() || current.value->IsArray())
                && current.depth > 32) {
                return reject(Code::ResourceLimit, pathOf(currentIndex));
            }
            if (current.value->IsObject()) {
                std::vector<std::string_view> names;
                names.reserve(current.value->MemberCount());
                for (auto member = current.value->MemberBegin();
                     member != current.value->MemberEnd(); ++member) {
                    const auto name = memberName(member);
                    for (const auto prior : names) {
                        if (prior == name) {
                            return reject(Code::InvalidJson, pointerChild(pathOf(currentIndex), name));
                        }
                    }
                    names.push_back(name);
                    if (nodes.size() == 4096) {
                        return reject(Code::ResourceLimit, pointerChild(pathOf(currentIndex), name));
                    }
                    nodes.push_back({&member->value, currentIndex, name, 0, false,
                        current.depth + (member->value.IsObject() || member->value.IsArray())});
                }
            } else if (current.value->IsArray()) {
                for (rapidjson::SizeType index = 0; index < current.value->Size(); ++index) {
                    const auto& child = (*current.value)[index];
                    if (nodes.size() == 4096) {
                        return reject(Code::ResourceLimit, pointerIndex(pathOf(currentIndex), index));
                    }
                    nodes.push_back({&child, currentIndex, {}, index, true,
                        current.depth + (child.IsObject() || child.IsArray())});
                }
            }
        }
        return true;
    }

    bool inertString(const Value& value, std::string path) {
        if (!value.IsString()) return reject(Code::InvalidType, std::move(path));
        if (value.GetStringLength() > 256) return reject(Code::UnsupportedValue, std::move(path));
        for (rapidjson::SizeType index = 0; index < value.GetStringLength(); ++index) {
            if (value.GetString()[index] == '\0') {
                return reject(Code::UnsupportedValue, std::move(path));
            }
        }
        return true;
    }

    bool object(const Value& value, std::string_view path,
                std::initializer_list<std::string_view> required,
                std::initializer_list<std::string_view> optional = {}) {
        if (!value.IsObject()) return reject(Code::InvalidType, std::string{path});
        for (auto member = value.MemberBegin(); member != value.MemberEnd(); ++member) {
            const auto name = memberName(member);
            bool known = false;
            for (const auto allowed : required) known |= name == allowed;
            for (const auto allowed : optional) known |= name == allowed;
            if (!known) return reject(Code::UnsupportedField, pointerChild(path, name));
        }
        for (const auto needed : required) {
            if (!field(value, needed)) {
                return reject(Code::UnsupportedStructure, pointerChild(path, needed));
            }
        }
        if (const auto* name = field(value, "nm")) {
            if (!inertString(*name, pointerChild(path, "nm"))) return false;
        }
        return true;
    }

    bool number(const Value& value, std::string path, double low, double high,
                double* output = nullptr) {
        if (!value.IsNumber()) return reject(Code::InvalidType, std::move(path));
        const double numeric = value.GetDouble();
        if (!std::isfinite(numeric) || numeric < low || numeric > high) {
            return reject(Code::UnsupportedValue, std::move(path));
        }
        if (output) *output = numeric;
        return true;
    }

    bool integer(const Value& value, std::string path, double low, double high,
                 double* output = nullptr) {
        double numeric = 0;
        if (!number(value, path, low, high, &numeric)) return false;
        if (std::floor(numeric) != numeric) return reject(Code::UnsupportedValue, std::move(path));
        if (output) *output = numeric;
        return true;
    }

    bool literal(const Value& value, std::string path, std::string_view expected) {
        if (!value.IsString()) return reject(Code::InvalidType, std::move(path));
        if (std::string_view{value.GetString(), value.GetStringLength()} != expected) {
            return reject(Code::UnsupportedValue, std::move(path));
        }
        return true;
    }

    bool arraySize(const Value& value, std::string path, rapidjson::SizeType size) {
        if (!value.IsArray()) return reject(Code::InvalidType, std::move(path));
        if (value.Size() != size) return reject(Code::UnsupportedStructure, std::move(path));
        return true;
    }

    bool vector(const Value& value, std::string_view path,
                std::initializer_list<Range> ranges, double* output = nullptr) {
        if (!arraySize(value, std::string{path}, static_cast<rapidjson::SizeType>(ranges.size()))) {
            return false;
        }
        rapidjson::SizeType index = 0;
        for (const auto [low, high] : ranges) {
            double numeric = 0;
            if (!number(value[index], pointerIndex(path, index), low, high, &numeric)) return false;
            if (output) output[index] = numeric;
            ++index;
        }
        return true;
    }

    bool staticScalar(const Value& value, std::string_view path, double expected) {
        if (!object(value, path, {"a", "k"})) return false;
        return integer(*field(value, "a"), pointerChild(path, "a"), 0, 0)
            && number(*field(value, "k"), pointerChild(path, "k"), expected, expected);
    }

    bool staticVector(const Value& value, std::string_view path,
                      std::initializer_list<Range> ranges) {
        if (!object(value, path, {"a", "k"})) return false;
        return integer(*field(value, "a"), pointerChild(path, "a"), 0, 0)
            && vector(*field(value, "k"), pointerChild(path, "k"), ranges);
    }

    bool easing(const Value& value, std::string_view path) {
        if (!object(value, path, {"x", "y"})) return false;
        return number(*field(value, "x"), pointerChild(path, "x"), 0, 1)
            && number(*field(value, "y"), pointerChild(path, "y"), 0, 1);
    }

    bool rootObject(const Value& root);
    bool layer(const Value& value, std::string_view path, double rootOp);
    bool layerTransform(const Value& value, std::string_view path);
    bool group(const Value& value, std::string_view path, double rootOp);
    bool ellipse(const Value& value, std::string_view path, double rootOp);
    bool fill(const Value& value, std::string_view path);
    bool groupTransform(const Value& value, std::string_view path);
    bool position(const Value& value, std::string_view path, double rootOp);
};

bool Auditor::position(const Value& value, std::string_view path, double rootOp) {
    if (!object(value, path, {"a", "k"})) return false;
    double animated = 0;
    if (!integer(*field(value, "a"), pointerChild(path, "a"), 0, 1, &animated)) return false;
    const auto kPath = pointerChild(path, "k");
    const auto& k = *field(value, "k");
    if (animated == 0) {
        return vector(k, kPath, {{-32768, 32768}, {-32768, 32768}});
    }
    if (!arraySize(k, kPath, 2)) return false;
    const auto firstPath = pointerIndex(kPath, 0);
    const auto lastPath = pointerIndex(kPath, 1);
    const auto& first = k[0];
    const auto& last = k[1];
    if (!object(first, firstPath, {"t", "s", "e", "i", "o"})) return false;
    if (!integer(*field(first, "t"), pointerChild(firstPath, "t"), 0, 0)) return false;
    if (!vector(*field(first, "s"), pointerChild(firstPath, "s"),
                {{-32768, 32768}, {-32768, 32768}})) return false;
    double end[2]{};
    if (!vector(*field(first, "e"), pointerChild(firstPath, "e"),
                {{-32768, 32768}, {-32768, 32768}}, end)) return false;
    if (!easing(*field(first, "i"), pointerChild(firstPath, "i"))) return false;
    if (!easing(*field(first, "o"), pointerChild(firstPath, "o"))) return false;
    if (!object(last, lastPath, {"t", "s"})) return false;
    if (!integer(*field(last, "t"), pointerChild(lastPath, "t"), rootOp - 1, rootOp - 1)) {
        return false;
    }
    double start[2]{};
    if (!vector(*field(last, "s"), pointerChild(lastPath, "s"),
                {{-32768, 32768}, {-32768, 32768}}, start)) return false;
    if (start[0] != end[0] || start[1] != end[1]) {
        return reject(Code::UnsupportedValue, pointerChild(lastPath, "s"));
    }
    return true;
}

bool Auditor::layerTransform(const Value& value, std::string_view path) {
    if (!object(value, path, {"o", "r", "p", "a", "s"})) return false;
    return staticScalar(*field(value, "o"), pointerChild(path, "o"), 100)
        && staticScalar(*field(value, "r"), pointerChild(path, "r"), 0)
        && staticVector(*field(value, "p"), pointerChild(path, "p"),
            {{-32768, 32768}, {-32768, 32768}, {0, 0}})
        && staticVector(*field(value, "a"), pointerChild(path, "a"),
            {{0, 0}, {0, 0}, {0, 0}})
        && staticVector(*field(value, "s"), pointerChild(path, "s"),
            {{100, 100}, {100, 100}, {100, 100}});
}

bool Auditor::ellipse(const Value& value, std::string_view path, double rootOp) {
    if (!object(value, path, {"ty", "d", "s", "p"}, {"nm"})) return false;
    if (!literal(*field(value, "ty"), pointerChild(path, "ty"), "el")) return false;
    if (!integer(*field(value, "d"), pointerChild(path, "d"), 1, 1)) return false;
    const auto sPath = pointerChild(path, "s");
    const auto& s = *field(value, "s");
    if (!object(s, sPath, {"a", "k"})) return false;
    if (!integer(*field(s, "a"), pointerChild(sPath, "a"), 0, 0)) return false;
    const auto kPath = pointerChild(sPath, "k");
    const auto& k = *field(s, "k");
    if (!vector(k, kPath, {{0, 16384}, {0, 16384}})) return false;
    for (rapidjson::SizeType index = 0; index < 2; ++index) {
        if (k[index].GetDouble() <= 0) {
            return reject(Code::UnsupportedValue, pointerIndex(kPath, index));
        }
    }
    return position(*field(value, "p"), pointerChild(path, "p"), rootOp);
}

bool Auditor::fill(const Value& value, std::string_view path) {
    if (!object(value, path, {"ty", "c", "o", "r"}, {"nm"})) return false;
    if (!literal(*field(value, "ty"), pointerChild(path, "ty"), "fl")) return false;
    return staticVector(*field(value, "c"), pointerChild(path, "c"),
            {{0, 1}, {0, 1}, {0, 1}, {1, 1}})
        && staticScalar(*field(value, "o"), pointerChild(path, "o"), 100)
        && integer(*field(value, "r"), pointerChild(path, "r"), 1, 1);
}

bool Auditor::groupTransform(const Value& value, std::string_view path) {
    if (!object(value, path, {"ty", "p", "a", "s", "r", "sk", "sa", "o"}, {"nm"})) {
        return false;
    }
    if (!literal(*field(value, "ty"), pointerChild(path, "ty"), "tr")) return false;
    return staticVector(*field(value, "p"), pointerChild(path, "p"), {{0, 0}, {0, 0}})
        && staticVector(*field(value, "a"), pointerChild(path, "a"), {{0, 0}, {0, 0}})
        && staticVector(*field(value, "s"), pointerChild(path, "s"), {{100, 100}, {100, 100}})
        && staticScalar(*field(value, "r"), pointerChild(path, "r"), 0)
        && staticScalar(*field(value, "sk"), pointerChild(path, "sk"), 0)
        && staticScalar(*field(value, "sa"), pointerChild(path, "sa"), 0)
        && staticScalar(*field(value, "o"), pointerChild(path, "o"), 100);
}

bool Auditor::group(const Value& value, std::string_view path, double rootOp) {
    if (!object(value, path, {"ty", "it"}, {"nm"})) return false;
    if (!literal(*field(value, "ty"), pointerChild(path, "ty"), "gr")) return false;
    const auto itPath = pointerChild(path, "it");
    const auto& items = *field(value, "it");
    if (!arraySize(items, itPath, 3)) return false;
    return ellipse(items[0], pointerIndex(itPath, 0), rootOp)
        && fill(items[1], pointerIndex(itPath, 1))
        && groupTransform(items[2], pointerIndex(itPath, 2));
}

bool Auditor::layer(const Value& value, std::string_view path, double rootOp) {
    if (!object(value, path,
                {"ddd", "ind", "ty", "sr", "ks", "ao", "shapes", "ip", "op", "st", "bm"},
                {"nm"})) return false;
    if (!integer(*field(value, "ddd"), pointerChild(path, "ddd"), 0, 0)
        || !integer(*field(value, "ind"), pointerChild(path, "ind"), 1, 2147483647)
        || !integer(*field(value, "ty"), pointerChild(path, "ty"), 4, 4)
        || !number(*field(value, "sr"), pointerChild(path, "sr"), 1, 1)
        || !layerTransform(*field(value, "ks"), pointerChild(path, "ks"))
        || !integer(*field(value, "ao"), pointerChild(path, "ao"), 0, 0)) return false;
    const auto shapesPath = pointerChild(path, "shapes");
    const auto& shapes = *field(value, "shapes");
    if (!arraySize(shapes, shapesPath, 1)) return false;
    if (!group(shapes[0], pointerIndex(shapesPath, 0), rootOp)) return false;
    double ip = 0;
    double op = 0;
    if (!integer(*field(value, "ip"), pointerChild(path, "ip"), 0, rootOp, &ip)
        || !integer(*field(value, "op"), pointerChild(path, "op"), 0, rootOp, &op)) return false;
    if (ip >= op) return reject(Code::UnsupportedValue, pointerChild(path, "op"));
    return integer(*field(value, "st"), pointerChild(path, "st"), 0, 0)
        && integer(*field(value, "bm"), pointerChild(path, "bm"), 0, 0);
}

bool Auditor::rootObject(const Value& root) {
    constexpr std::string_view path = "/";
    if (!object(root, path,
                {"fr", "ip", "op", "w", "h", "ddd", "assets", "markers", "layers"},
                {"v", "nm"})) return false;
    if (const auto* version = field(root, "v")) {
        if (!inertString(*version, pointerChild(path, "v"))) return false;
    }
    double rootOp = 0;
    if (!number(*field(root, "fr"), "/fr", 0, 240)) return false;
    if (field(root, "fr")->GetDouble() <= 0) return reject(Code::UnsupportedValue, "/fr");
    if (!integer(*field(root, "ip"), "/ip", 0, 0)
        || !integer(*field(root, "op"), "/op", 2, 10000, &rootOp)
        || !integer(*field(root, "w"), "/w", 1, 8192)
        || !integer(*field(root, "h"), "/h", 1, 8192)
        || !integer(*field(root, "ddd"), "/ddd", 0, 0)
        || !arraySize(*field(root, "assets"), "/assets", 0)
        || !arraySize(*field(root, "markers"), "/markers", 0)) return false;
    const auto& layers = *field(root, "layers");
    if (!arraySize(layers, "/layers", 1)) return false;
    return layer(layers[0], "/layers/0", rootOp);
}

} // namespace

NativeEllipseAdmission auditNativeEllipseInput(std::string_view json) {
    if (json.size() > 1'048'576) return {Code::ResourceLimit, "/"};
    if (json.empty() || json.find('\0') != std::string_view::npos) {
        return {Code::InvalidJson, "/"};
    }
    rapidjson::Document document;
    document.Parse<rapidjson::kParseIterativeFlag |
                   rapidjson::kParseValidateEncodingFlag>(json.data(), json.size());
    if (document.HasParseError()) return {Code::InvalidJson, "/"};
    return Auditor{}.run(document);
}

} // namespace avemotion::runtime::detail
