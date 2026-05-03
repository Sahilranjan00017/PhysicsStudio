#pragma once

class ElectronicsDomain;
class MotionDomain;
class OpticalDomain;
class WaveDomain;

class IElectronicsSolver {
public:
    virtual ~IElectronicsSolver() = default;
    virtual void solve(ElectronicsDomain& domain, double dt) = 0;
};

class IMotionSolver {
public:
    virtual ~IMotionSolver() = default;
    virtual void step(MotionDomain& domain, double dt) = 0;
};

class IOpticsSolver {
public:
    virtual ~IOpticsSolver() = default;
    virtual void trace(OpticalDomain& domain) = 0;
};

class IWaveSolver {
public:
    virtual ~IWaveSolver() = default;
    virtual void step(WaveDomain& domain, double dt) = 0;
};

