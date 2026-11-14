#include "core/ColumnsFit.h"

#include <algorithm>

namespace abp::core::columns {

FitResult fit(std::vector<bool> const& occupied, int targetSlots) {
    FitResult         r;
    std::size_t const from = static_cast<std::size_t>(std::max(0, targetSlots));
    for (std::size_t i = from; i < occupied.size(); ++i) {
        if (occupied[i]) r.errorSlots.push_back(static_cast<int>(i));
    }
    r.fits = r.errorSlots.empty();
    return r;
}

} // namespace abp::core::columns
