#include <lagrange/scene/Scene.h>

namespace lagrange {
namespace scene {

template struct Scene<float, uint32_t>;
template struct Scene<double, uint32_t>;
template struct Scene<float, uint64_t>;
template struct Scene<double, uint64_t>;

} // namespace scene
} // namespace lagrange
