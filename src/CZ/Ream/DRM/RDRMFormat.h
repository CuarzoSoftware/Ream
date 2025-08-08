#ifndef RDRMFORMAT_H
#define RDRMFORMAT_H

#include <CZ/Ream/Ream.h>
#include <CZ/Utils/CZMathUtils.h>
#include <boost/container/flat_set.hpp>
#include <string_view>
#include <drm_fourcc.h>

#define REAM_FLAT_SET boost::container::flat_set

#ifndef DRM_FORMAT_A8
#define DRM_FORMAT_A8 fourcc_code('A', '8', ' ', ' ')
#endif

namespace CZ
{
    struct RFormatInfo
    {
        /**
         * @brief The DRM format.
         */
        RFormat format;

        /**
         * @brief Number of bytes required to store one block of pixels.
         *
         * For uncompressed formats with 1x1 blocks, this typically equals the number of bytes per pixel.
         */
        UInt32 bytesPerBlock;

        /**
         * @brief Width of one block of pixels.
         *
         * The number of pixels wide in a single block for this image format (1 for uncompressed formats).
         * For example, in a format with a blockWidth of 4, each block covers 4 pixels horizontally.
         * This is important for formats that compress or group pixels in blocks rather than individually.
         */
        UInt32 blockWidth;

        /**
         * @brief Height of one block of pixels.
         *
         * The number of pixels tall in a single block for this image format (1 for uncompressed formats).
         * For example, if blockHeight is 2, each block covers 2 pixels vertically.
         * This matters for formats that group or compress pixels in blocks instead of individually.
         */
        UInt32 blockHeight;

        /**
         * @brief Bits per pixel.
         */
        UInt32 bpp;

        /**
         * @brief Number of bits that are actually used to represent color information — often excluding alpha or padding bits.
         */
        UInt32 depth;

        /**
         * @brief Number of separate memory planes used to store pixel data.
         *
         * Typically 1 for packed RGB formats, 2–3 for planar YUV formats.
         */
        UInt32 planes;

        /**
         * @brief `true` if the format includes an alpha channel.
         */
        bool alpha;

        /**
         * @brief Returns the total number of pixels covered by a single block.
         *
         * This is calculated as blockWidth × blockHeight.
         */
        constexpr UInt32 pixelsPerBlock() const noexcept
        {
            return blockWidth * blockHeight;
        }

        /**
         * @brief Computes the number of blocks required to cover a given image width.
         *
         * This uses ceiling division to ensure partial blocks are counted.
         * Important for compressed formats where data is grouped in blocks of multiple pixels.
         *
         * @param width Image width in pixels.
         * @return Number of horizontal blocks needed to cover the width.
         */
        constexpr UInt32 blocksPerRow(UInt32 width) const noexcept
        {
            return CZMathUtils::DivCeil(width, blockWidth);
        }

        /**
         * @brief Calculates the minimum required stride (in bytes) for a given image width.
         *
         * The stride must be at least this value to ensure a full row of image data can be represented in memory.
         * This value is based on the number of blocks per row and the number of bytes per block.
         *
         * @param width Image width in pixels.
         * @return Minimum stride in bytes needed to accommodate the image width.
         */
        constexpr UInt32 minStride(UInt32 width) const noexcept
        {
            return blocksPerRow(width) * bytesPerBlock;
        }

        /**
         * @brief Validates whether the provided stride matches the alignment and is sufficient for the image width.
         *
         * @param width Image width in pixels.
         * @param stride Stride in bytes.
         * @return `true` if stride is valid, `false` otherwise.
         */
        constexpr bool validateStride(UInt32 width, UInt32 stride) const noexcept
        {
            return (stride % bytesPerBlock) == 0 && stride >= minStride(width);
        }
    };

    /**
     * @brief A DRM Four Character Code format along with a set of supported modifiers.
     *
     * It can be passed to buffer allocators to let them choose an optimal modifier for the given format.
     */
    class RDRMFormat
    {
    public:

        /* Return the format name or "Unknown" if not found */
        static std::string_view FormatName(RFormat format) noexcept;
        static std::string_view ModifierName(RModifier modifier) noexcept;

        /**
         * @brief Retrieves detailed information about a DRM format.
         *
         * @param format The DRM format to query.
         * @return A pointer to an RFormatInfo struct if the format is supported, or nullptr otherwise.
         */
        static const RFormatInfo *GetInfo(RFormat format) noexcept;

        /**
         * @brief Returns a substitute format with or without alpha, depending on the input.
         *
         * For example, this can convert ARGB8888 to XRGB8888 and vice versa.
         * If no suitable substitute exists, the original format is returned.
         *
         * @param format The input DRM format.
         * @return A substitute format with modified alpha presence, or the original format.
         */
        static RFormat GetAlphaSubstitute(RFormat format) noexcept;

        /**
         * @brief Constructs a new RDRMFormat with the specified format and modifiers.
         *
         * @param format The DRM format.
         * @param modifiers A list of supported modifiers for the format.
         */
        RDRMFormat(RFormat format, std::initializer_list<RModifier> modifiers) noexcept
            : m_format(format)
        {
            for (auto mod : modifiers)
                m_modifiers.emplace(mod);
        }

        RDRMFormat(const RDRMFormat &other) noexcept
            : m_format(other.m_format),
            m_modifiers(other.m_modifiers)
        {}

        RDRMFormat& operator=(const RDRMFormat &other) noexcept
        {
            if (this != &other)
            {
                m_format = other.m_format;
                m_modifiers = other.m_modifiers;
            }
            return *this;
        }

        RDRMFormat(RDRMFormat &&other) noexcept
            : m_format(std::move(other.m_format)),
            m_modifiers(std::move(other.m_modifiers))
        {
            other.m_format = DRM_FORMAT_INVALID;
            other.m_modifiers = {};
        }

        RDRMFormat& operator=(RDRMFormat &&other) noexcept
        {
            if (this != &other)
            {
                m_format = std::move(other.m_format);
                m_modifiers = std::move(other.m_modifiers);
                other.m_format = DRM_FORMAT_INVALID;
                other.m_modifiers = {};
            }
            return *this;
        }

        /**
         * @brief Returns the DRM format.
         */
        RFormat format() const noexcept { return m_format; }

        /**
         * @brief Sets a new DRM format.
         */
        void setFormat(RFormat format) noexcept { m_format = format; }

        /**
         * @brief Returns the set of modifiers associated with this format.
         */
        const REAM_FLAT_SET<RModifier> &modifiers() const noexcept { return m_modifiers; }

        /**
         * @brief Adds a modifier to the set.
         *
         * @param modifier The modifier to add.
         * @return true if the modifier was newly added, false if it was already present.
         */
        bool addModifier(RModifier modifier) noexcept
        {
            return m_modifiers.insert(modifier).second;
        }

        /**
         * @brief Removes a modifier from the set.
         *
         * @param modifier The modifier to remove.
         * @return true if the modifier was removed, false if it was not found.
         */
        bool removeModifier(RModifier modifier) noexcept
        {
            return m_modifiers.erase(modifier) > 0;
        }

        /* Used to enable RDRMFormat comparisons */

        bool operator<(const RDRMFormat& other) const noexcept
        {
            return m_format < other.m_format;
        }

        struct Less
        {
            using is_transparent = void;

            bool operator()(const RDRMFormat& lhs, const RDRMFormat& rhs) const noexcept
            {
                return lhs.m_format < rhs.m_format;
            }

            bool operator()(const RDRMFormat& lhs, const RFormat& rhs) const noexcept
            {
                return lhs.m_format < rhs;
            }

            bool operator()(const RFormat& lhs, const RDRMFormat& rhs) const noexcept
            {
                return lhs < rhs.m_format;
            }
        };
    private:
        friend class RDRMFormatSet;
        bool addModifier(RModifier modifier) const noexcept
        {
            return m_modifiers.insert(modifier).second;
        }
        bool removeModifier(RModifier modifier) const noexcept
        {
            return m_modifiers.erase(modifier) > 0;
        }
        RFormat m_format;
        mutable REAM_FLAT_SET<RModifier> m_modifiers;
    };

    /**
     * @brief Represents a set of DRM formats and their associated modifiers.
     *
     * This class is typically used by subsystems (e.g. KMS, EGL, etc) to declare
     * supported format+modifier combinations.
     */
    class RDRMFormatSet
    {
    public:
        RDRMFormatSet() noexcept {}

        RDRMFormatSet(const RDRMFormatSet &other) noexcept
            : m_formats(other.m_formats)
        {}

        RDRMFormatSet& operator=(const RDRMFormatSet &other) noexcept
        {
            if (this != &other)
                m_formats = other.m_formats;
            return *this;
        }

        RDRMFormatSet(RDRMFormatSet &&other) noexcept
            : m_formats(std::move(other.m_formats))
        {
            other.m_formats = {};
        }

        RDRMFormatSet& operator=(RDRMFormatSet &&other) noexcept
        {
            if (this != &other)
            {
                m_formats = std::move(other.m_formats);
                other.m_formats = {};
            }
            return *this;
        }

        /**
         * @brief Returns the internal set of formats with their modifiers.
         */
        const REAM_FLAT_SET<RDRMFormat, RDRMFormat::Less> &formats() const noexcept { return m_formats; }

        /**
         * @brief Computes the union of two format sets.
         *
         * @param a First format set.
         * @param b Second format set.
         * @return A new set containing all format+modifier combinations from both inputs.
         */
        static RDRMFormatSet Union(const RDRMFormatSet &a, const RDRMFormatSet &b) noexcept;

        /**
         * @brief Computes the intersection of two format sets.
         *
         * Only formats and modifiers present in both sets will be retained.
         *
         * @param a First format set.
         * @param b Second format set.
         * @return A new set containing only the common format+modifier pairs.
         */
        static RDRMFormatSet Intersect(const RDRMFormatSet &a, const RDRMFormatSet &b) noexcept;

        /**
         * @brief Computes the intersection between a format set and a set of DRM formats.
         *
         * All modifiers for matching formats are retained.
         *
         * @param a Format set to filter.
         * @param b Set of DRM formats.
         * @return A new set with formats present in both inputs.
         */
        static RDRMFormatSet Intersect(const RDRMFormatSet &a, const REAM_FLAT_SET<RFormat> &b) noexcept;

        /**
         * @brief Checks if a specific format+modifier combination is present.
         *
         * @param format The DRM format to check.
         * @param modifier The modifier to check.
         * @return true if the format and modifier exist in the set, false otherwise.
         */
        bool has(RFormat format, RModifier modifier) const noexcept;

        /**
         * @brief Adds a format+modifier pair to the set.
         *
         * If the format is new, it is inserted with the modifier.
         * If the format already exists, the modifier is added to its modifier set.
         *
         * @param format The DRM format to add.
         * @param modifier The modifier to add.
         * @return true if the combination was newly added, false if it already existed.
         */
        bool add(RFormat format, RModifier modifier) noexcept;

        /**
         * @brief Removes a specific modifier from a format.
         *
         * If the modifier is the last one for that format, the entire format entry is removed.
         *
         * @param format The DRM format.
         * @param modifier The modifier to remove.
         * @return true if the modifier was removed, false if it was not found.
         */
        bool remove(RFormat format, RModifier modifier) noexcept;

        /**
         * @brief Removes all modifiers for a given format, effectively removing the format.
         *
         * @param format The DRM format to remove.
         * @return true if the format was removed, false if it was not present.
         */
        bool removeFormat(RFormat format) noexcept;

        /**
         * @brief Removes the given modifier from all formats.
         *
         * If any format ends up with no modifiers, it is removed from the set.
         *
         * @param modifier The modifier to remove.
         * @return true if at least one modifier was removed, false otherwise.
         */
        bool removeModifier(RModifier modifier) noexcept;

        /**
         * @brief Adds the given modifier to all existing formats.
         *
         * @param modifier The modifier to add.
         * @return true if the modifier was added to at least one format; false otherwise.
         */
        bool addModifier(RModifier modifier) noexcept;

        void log() const noexcept;
    private:
        REAM_FLAT_SET<RDRMFormat, RDRMFormat::Less> m_formats;
    };
}

#endif // RDRMFORMAT_H
