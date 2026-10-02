#pragma once

#include "intensity/cube_image.hpp"
#include "intensity/coin/coin_image_processor.hpp"
#include <string>

namespace cube::coin {

// Explicit opt-in for P3-A; the frozen runner passes this environment through.
// No switch is inferred from the existing /photo controls.
std::string p3aRepresentation();
Settings p3aCubeSettings(const std::string& representation);

// Export the existing Cubemap representation into COIN's patch-image contract.
// Upstream intensity preprocessing, patch tracking, selector, and fusion stay
// shared. Filled pixels receive a depth-derived landmark, never a fabricated
// geometric observation; these points are private to the intensity channel.
CoinFrame buildCoinCubemap(CubeImage& image,const CoinFrame& calibrated,
                          const CoinOusterProjector& calibrated_projector,
                          const CoinImageSettings& settings,
                          std::vector<CoinScanPoint>& points);

} // namespace cube::coin
