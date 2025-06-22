#include <RCore.h>

using namespace CZ;

int main()
{
    setenv("REAM_DEBUG", "4", 0);
    RCore::Options options {};
    auto core { RCore::Make(options) };
    return 0;
}
