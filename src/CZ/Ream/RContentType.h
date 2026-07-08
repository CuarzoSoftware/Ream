#ifndef RCONTENTTYPE_H
#define RCONTENTTYPE_H

#include <CZ/Core/Cuarzo.h>
#include <string_view>
#include <array>

namespace CZ
{
    /**
     * @brief Type of content being displayed.
     *
     * Matches the DRM "content type" connector property, which lets a display
     * optimize its processing for the given kind of content.
     */
    enum class RContentType
    {
        None,     ///< No specific content type hint.
        Graphics, ///< Desktop/graphics content.
        Photo,    ///< Photographic content.
        Video,    ///< Video content.
        Game      ///< Game content.
    };

    /**
     * @brief Returns a human-readable string for the given content type.
     *
     * @param type The content type to convert.
     * @return A string view naming the content type (e.g. "Video").
     */
    inline const std::string_view &RContentTypeString(RContentType type) noexcept
    {
        static constexpr const std::array<std::string_view, 5> strings { "None", "Graphics", "Photo", "Video", "Game" };
        return strings[static_cast<int>(type)];
    }
};

#endif // RCONTENTTYPE_H
