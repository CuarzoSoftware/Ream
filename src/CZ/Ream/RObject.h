#ifndef ROBJECT_H
#define ROBJECT_H

#include <CZ/Core/CZObject.h>
#include <CZ/Ream/Ream.h>

/**
 * @brief Base class for Ream objects.
 *
 * Extends CZObject, providing the common object infrastructure shared by Ream types
 * such as RCore, RDevice, RImage, RSurface and RPass.
 */
class CZ::RObject : public CZObject
{
public:
    /**
     * @brief Constructs a default RObject.
     */
    RObject() = default;
};

#endif // ROBJECT_H
