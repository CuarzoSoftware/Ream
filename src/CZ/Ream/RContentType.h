#ifndef RCONTENTTYPE_H
#define RCONTENTTYPE_H

#include <CZ/CZ.h>
#include <algorithm>
#include <string_view>
#include <array>

namespace CZ
{
    enum class RContentType
    {
        Graphics = 1,
        Photo,
        Video,
        Game
    };

    inline const std::string_view &RContentTypeString(RContentType type) noexcept
    {
        static constexpr const std::array<std::string_view, 5> strings { "Graphics", "Photo", "Video", "Game", "Unknown" };
        return strings[std::clamp(static_cast<UInt32>(type) - 1U, 0U, 4U)];
    }
};

#endif // RCONTENTTYPE_H
