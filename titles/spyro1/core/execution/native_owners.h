#pragma once

class Core;

namespace spyro1 {

// Installers for Spyro 1's handwritten leaf bodies.
void registerNativeRand(Core &core);
void registerNativeLeaves(Core &core);
void registerNativeVec(Core &core);
void registerNativeGte(Core &core);
void registerNativeAngle(Core &core);
void registerNativeUtil(Core &core);
// The title's cooperative CD loader leaves and their completion delivery.
void registerCdQueue(Core &core);

} // namespace spyro1
