/* compat.h -- force-included (-include) into every engine file (mpc-vst-liminal-amp).
 * 1. NeuralAmpModelerCore pins a newer Eigen whose placeholders namespace holds lastN(); Eigen 3.4.0 has the same
 *    function as Eigen::lastN. It is the only newer-Eigen feature NAM uses (NAM/lstm.h).
 * 2. AudioDSPTools' resampler (from iPlug2) uses iplug::PI and DEFAULT_BLOCK_SIZE, which the iPlug2 framework
 *    defines (as below); this port doesn't use iPlug2. DEFAULT_BLOCK_SIZE is only a default argument here. */
#pragma once
#include <Eigen/Core>
namespace Eigen { namespace placeholders { using Eigen::lastN; } }
namespace iplug { static constexpr double PI = 3.14159265358979323846; }
#ifndef DEFAULT_BLOCK_SIZE
#define DEFAULT_BLOCK_SIZE 1024
#endif
