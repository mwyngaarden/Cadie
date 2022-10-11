#include "util.h"
#include "types.h"

namespace Util {

    i64 now()
    {
        using namespace std::chrono;

        auto now = high_resolution_clock::now();
        auto ms = time_point_cast<milliseconds>(now);
        auto value = ms.time_since_epoch();

        return value.count();
    }
}
