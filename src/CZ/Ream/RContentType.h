#ifndef RCONTENTTYPE_H
#define RCONTENTTYPE_H

#include <CZ/Core/Cuarzo.h>
#include <string_view>
#include <array>

namespace CZ
{
    enum class RContentType
    {
        None,
        Graphics,
        Photo,
        Video,
        Game
    };

    inline const std::string_view &RContentTypeString(RContentType type) noexcept
    {
        static constexpr const std::array<std::string_view, 5> strings { "None", "Graphics", "Photo", "Video", "Game" };
        return strings[static_cast<int>(type)];
    }
};

#endif // RCONTENTTYPE_H
