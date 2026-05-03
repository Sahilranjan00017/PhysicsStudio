#pragma once

#include "simulation/SolverInterfaces.h"

class ElectronicsDomain {
public:
    int nodeCount = 0;
};

class ElectronicsSolver final : public IElectronicsSolver {
public:
    void solve(ElectronicsDomain& domain, double dt) override;
};

