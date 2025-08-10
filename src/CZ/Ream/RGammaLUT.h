#ifndef CZ_RGAMMALUT_H
#define CZ_RGAMMALUT_H

#include <CZ/Ream/RObject.h>
#include <CZ/CZWeak.h>
#include <memory>
#include <span>

/**
 * @brief Gamma LUT.
 */
class CZ::RGammaLUT final : public RObject
{
public:

    /**
     * @brief Constructs an RGammaLUT with the specified size.
     *
     * @param size The size of the gamma correction table. Defaults to 0.
     */
    static std::shared_ptr<RGammaLUT> Make(size_t size = 0) noexcept;
    static std::shared_ptr<RGammaLUT> MakeFilled(size_t size, Float64 gamma, Float64 brightness, Float64 contrast) noexcept;

    /**
     * @brief Destructor for RGammaLUT.
     *
     * Destroys the gamma correction table.
     */
    ~RGammaLUT() = default;

    /**
     * @brief Set the size of the gamma correction table.
     *
     * @note This operation removes previous values.
     *
     * @param size The new size for the gamma correction table.
     */
    void setSize(size_t size) noexcept
    {
        m_table.resize(size * 3);
    }

    /**
     * @brief Gets the size of the gamma correction table.
     *
     * The size represents the number of @ref UInt16 values used to independently represent each RGB curve.
     *
     * @return The size of the gamma correction table.
     */
    size_t size() const noexcept
    {
        return m_table.size()/3;
    }

    /**
     * @brief Fill method for setting table values.
     *
     * This auxiliary method allows setting gamma, brightness, and contrast values for the table.
     *
     * @param gamma The gamma correction value.
     * @param brightness The brightness adjustment value.
     * @param contrast The contrast adjustment value.
     */
    void fill(Float64 gamma, Float64 brightness, Float64 contrast) noexcept;

    /**
     * @brief Gets a pointer to the beginning of the red curve in the array.
     *
     * @return Pointer to the red curve in the array, or `nullptr` if the table size is 0.
     */
    std::span<UInt16> red() noexcept
    {
        return std::span<UInt16>(m_table.data(), size());
    }

    /**
     * @brief Gets a pointer to the beginning of the green curve in the array.
     *
     * @return Pointer to the green curve in the array, or `nullptr` if the table size is 0.
     */
    std::span<UInt16> green() noexcept
    {
        const auto s  { size() };
        return std::span<UInt16>(m_table.data() + s, s);
    }

    /**
     * @brief Gets a pointer to the beginning of the blue curve in the array.
     *
     * @return Pointer to the blue curve in the array, or `nullptr` if the table size is 0.
     */
    std::span<UInt16> blue() noexcept
    {
        const auto s  { size() };
        return std::span<UInt16>(m_table.data() + s * 2, s);
    }

    std::span<const UInt16> red() const noexcept
    {
        return std::span<const UInt16>(m_table.data(), size());
    }

    std::span<const UInt16> green() const noexcept
    {
        const auto s  { size() };
        return std::span<const UInt16>(m_table.data() + s, s);
    }

    std::span<const UInt16> blue() const noexcept
    {
        const auto s  { size() };
        return std::span<const UInt16>(m_table.data() + s * 2, s);
    }

private:
    RGammaLUT(UInt32 size = 0) noexcept { setSize(size); }
    std::vector<UInt16> m_table;
};

#endif // RGAMMALUT_H
