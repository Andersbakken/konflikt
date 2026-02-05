#include "konflikt/Protocol.h"

#include <glaze/glaze.hpp>

namespace konflikt {
namespace detail {
struct TypeOnly
{
    std::string type;
};
} // namespace detail
} // namespace konflikt

// Glaze metadata for TypeOnly helper
template <>
struct glz::meta<konflikt::detail::TypeOnly>
{
    using T = konflikt::detail::TypeOnly;
    static constexpr auto value = object("type", &T::type);
};

namespace konflikt {

std::optional<std::string> getMessageType(std::string_view json)
{
    // Quick extraction of "type" field without full parsing
    detail::TypeOnly result;
    auto error = glz::read_json(result, json);
    if (error) {
        return std::nullopt;
    }
    return result.type;
}

} // namespace konflikt
