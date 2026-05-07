# Solver Architecture Design

## Overview

Physics Simulation Studio uses a **Domain-Specific Solver** architecture where each physics domain (Electronics, Motion, Optics, Waves) has an independent solver that updates only its components. Solvers are decoupled from each other—they can run in any order and don't share state except through the components themselves.

---

## Solver Interface Pattern

All solvers follow the same pattern: inherit from an interface, implement one key virtual method, and operate on a domain-specific context.

### The Four Solver Interfaces

```cpp
// Base interface for Electronics solver
class IElectronicsSolver {
public:
    virtual ~IElectronicsSolver() = default;
    virtual void solve(ElectronicsDomain& domain, double dt) = 0;
};

// Base interface for Motion solver
class IMotionSolver {
public:
    virtual ~IMotionSolver() = default;
    virtual void step(MotionDomain& domain, double dt) = 0;
};

// Base interface for Optics solver
class IOpticsSolver {
public:
    virtual ~IOpticsSolver() = default;
    virtual void trace(OpticalDomain& domain) = 0;
};

// Base interface for Wave solver
class IWaveSolver {
public:
    virtual ~IWaveSolver() = default;
    virtual void step(WaveDomain& domain, double dt) = 0;
};
```

### Method Naming

- **Electronics**: `solve()` — because it solves a linear system (MNA)
- **Motion**: `step()` — because it steps forward in time
- **Optics**: `trace()` — because it traces rays through space
- **Waves**: `step()` — because it steps the wave equation

The naming reflects the mathematical nature of each domain.

---

## SimulationLoop: 60 FPS Orchestrator

The `SimulationLoop` class drives the entire simulation at 60 FPS (16 milliseconds per frame).

```cpp
class SimulationLoop : public QObject {
public:
    void start();           // Begin simulation
    void pause();           // Pause simulation
    void reset();           // Reset time to 0
    void setSpeed(double);  // 0.1x to 10.0x speedup
    
signals:
    void tickComplete(double simulationTime);  // Emitted after each step

private slots:
    void tick();            // Called every 16ms
};
```

### Execution Flow

```
┌─────────────────────────────────────┐
│ SimulationLoop Constructor          │
├─────────────────────────────────────┤
│ timer.setInterval(16);              │
│ connect timer→tick slot              │
│ fixedFrameDt = 0.016 (seconds)      │
│ speed = 1.0 (multiplier)            │
└─────────────────────────────────────┘
         ↓ User clicks Play
┌─────────────────────────────────────┐
│ SimulationLoop::start()             │
├─────────────────────────────────────┤
│ running = true                      │
│ timer.start()                       │
└─────────────────────────────────────┘
         ↓ Every 16ms
┌─────────────────────────────────────┐
│ SimulationLoop::tick()              │
├─────────────────────────────────────┤
│ if (running) {                      │
│   dt = 0.016 * speed                │
│   simulationTime += dt              │
│                                     │
│   // Phase 2: Call solvers here     │
│   // electronics.solve(domain, dt)  │
│   // motion.step(domain, dt)        │
│   // optics.trace(domain)           │
│   // wave.step(domain, dt)          │
│                                     │
│   emit tickComplete(simulationTime) │
│ }                                   │
└─────────────────────────────────────┘
         ↓ Repeats at 60 FPS
```

### Time Step Calculation

```cpp
const double simulationDt = fixedFrameDt * speed;
simulationTime += simulationDt;
```

Example:
- `fixedFrameDt = 0.016` (1/60 second)
- `speed = 2.0` (2x speedup)
- `simulationDt = 0.032` (advances 32ms per frame)
- Real time: 16ms elapsed
- Sim time: 32ms elapsed
- User sees simulation running 2x faster

---

## Domain Context Classes

Each solver receives a domain context that holds all relevant state for that physics domain.

### ElectronicsDomain

```cpp
class ElectronicsDomain {
public:
    int nodeCount = 0;
    
    // To be added in Phase 2:
    // QList<BaseComponent*> components;
    // QList<Wire*> wires;
    // Eigen::MatrixXd G;  // Conductance matrix
    // Eigen::VectorXd I;  // Current vector
    // Eigen::VectorXd V;  // Voltage vector
};
```

### MotionDomain

```cpp
class MotionDomain {
    // Planned structure:
    // QList<BaseComponent*> rigidBodies;
    // QList<Spring*> springs;
    // QList<Constraint*> constraints;
    // Vector3d gravity;
};
```

### OpticalDomain

```cpp
class OpticalDomain {
    // Planned structure:
    // QList<Ray*> rays;
    // QList<Surface*> surfaces;
    // double wavelength;
    // Color tracedColor;
};
```

### WaveDomain

```cpp
class WaveDomain {
    // Planned structure:
    // QList<BaseComponent*> sources;
    // Grid<Complex> fieldValues;
    // double frequency;
};
```

---

## Electronics Solver: MNA Approach

### What is MNA?

**Modified Nodal Analysis** is the standard method for circuit simulation. It converts a circuit into a linear system of equations that can be solved for node voltages and branch currents.

### MNA Matrix Structure

For a circuit with **n** nodes and **m** voltage sources:

```
    ┌────────────────┬──────────┐
    │   G[n×n]      │  B[n×m]  │
    │ (Conductance)  │          │
G = ├────────────────┼──────────┤
    │   C[m×n]      │  D[m×m]  │
    │                │ (Often 0)│
    └────────────────┴──────────┘

V = [v0, v1, ..., v(n-1), i_src0, i_src1, ..., i_src(m-1)]^T
I = [i_n0, i_n1, ..., i_n(n-1), v_src0, v_src1, ..., v_src(m-1)]^T

Solve: G * V = I
```

### Component Stamping

Each component "stamps" its contribution into the G matrix:

#### Resistor (conductance G = 1/R)

Connects nodes **a** and **b**:
```
G[a][a] += G_resistor
G[a][b] -= G_resistor
G[b][a] -= G_resistor
G[b][b] += G_resistor
```

#### Voltage Source (E volts between nodes a→b)

Adds constraint equation (row = source index):
```
C[source_idx][a] = 1
C[source_idx][b] = -1
D[source_idx][source_idx] = 0
I[n + source_idx] = E  // Voltage value
```

#### Current Source (I amps from node a to ground)

Adds to RHS vector:
```
I[a] += I_current
I[b] -= I_current
```

#### Capacitor (dynamic element, requires integration)

Uses companion model (substitutes as conductance + voltage source):
```
G_companion = C_capacitor / dt
V_companion = V_previous + I_previous * dt / C_capacitor

// Stamp as resistor + voltage source
G[a][a] += G_companion
G[a][b] -= G_companion
G[b][a] -= G_companion
G[b][b] += G_companion
I[node] += G_companion * V_companion
```

Requires RK4 integration to advance capacitor state.

---

## ElectronicsSolver Execution

### Phase 2 Implementation (Pseudocode)

```cpp
void ElectronicsSolver::solve(ElectronicsDomain& domain, double dt)
{
    // Step 1: Build node list
    Set<ConnectionPad*> allNodes;
    for (auto component : components) {
        for (auto pad : component->pads) {
            if (pad->domain == DomainType::Electrical) {
                allNodes.insert(pad);
            }
        }
    }
    Map<ConnectionPad*, int> nodeMap;
    for (int i = 0; i < allNodes.size(); ++i) {
        nodeMap[allNodes[i]] = i;
    }
    int n = allNodes.size();
    
    // Step 2: Initialize MNA matrix (accounting for voltage sources)
    int m = countVoltageSources(components);
    Eigen::MatrixXd G(n + m, n + m);
    G.setZero();
    Eigen::VectorXd I(n + m);
    I.setZero();
    
    // Step 3: Stamp components into matrix
    for (auto component : components) {
        component->stampMNA(dt);  // Each component stamps itself
    }
    
    // Step 4: Handle current sources (add to I vector)
    for (auto component : components) {
        if (auto* currentSource = dynamic_cast<CurrentSource*>(component)) {
            int aIdx = nodeMap[currentSource->pads[0]];
            int bIdx = nodeMap[currentSource->pads[1]];
            double I_val = currentSource->properties["current"].toDouble();
            I[aIdx] += I_val;
            I[bIdx] -= I_val;
        }
    }
    
    // Step 5: Solve linear system
    Eigen::VectorXd V = G.inverse() * I;
    // Or use LU decomposition for better numerics:
    // Eigen::VectorXd V = G.lu().solve(I);
    
    // Step 6: Update component states
    for (auto component : components) {
        for (auto pad : component->pads) {
            int nodeIdx = nodeMap[pad];
            double voltage = V[nodeIdx];
            component->simState["voltage@" + pad->padId] = voltage;
        }
    }
    
    // Step 7: Calculate branch currents
    for (auto wire : domain.wires) {
        ConnectionPad* start = wire->startPad;
        ConnectionPad* end = wire->endPad;
        int startIdx = nodeMap[start];
        int endIdx = nodeMap[end];
        double V_start = V[startIdx];
        double V_end = V[endIdx];
        
        // Assume resistor or direct connection
        // Current = (V_start - V_end) / R
        // For now, just store voltage difference
        wire->signal = QVariant(V_start - V_end);
    }
}
```

---

## Motion Solver

The `IMotionSolver` uses numerical integration to advance rigid body dynamics.

```cpp
class MotionSolver : public IMotionSolver {
public:
    void step(MotionDomain& domain, double dt) override {
        // For each component with stepMotion():
        for (auto component : domain.rigidBodies) {
            // Force calculation
            Vector3d totalForce = domain.gravity * component->mass;
            for (auto spring : domain.springs) {
                if (spring->connectedTo(component)) {
                    totalForce += spring->calculateForce();
                }
            }
            
            // RK4 integration
            Vector3d vel = component->simState["velocity"];
            Vector3d pos = component->simState["position"];
            
            Vector3d k1 = totalForce / component->mass;
            Vector3d k2 = (totalForce + dt/2 * component->mass * k1) / component->mass;
            Vector3d k3 = (totalForce + dt/2 * component->mass * k2) / component->mass;
            Vector3d k4 = (totalForce + dt * component->mass * k3) / component->mass;
            
            vel += dt/6 * (k1 + 2*k2 + 2*k3 + k4);
            pos += vel * dt;
            
            component->simState["velocity"] = vel;
            component->simState["position"] = pos;
            component->setPos(pos.x(), pos.y());
        }
    }
};
```

---

## Optics Solver

The `IOpticsSolver` traces rays through optical components.

```cpp
class OpticsSolver : public IOpticsSolver {
public:
    void trace(OpticalDomain& domain) override {
        // For each ray:
        for (auto ray : domain.rays) {
            Vector3d position = ray->startPosition;
            Vector3d direction = ray->direction;
            
            // Intersect with surfaces
            while (ray->isActive()) {
                Surface* hitSurface = findNearestIntersection(position, direction);
                if (!hitSurface) break;
                
                // Component reflects/refracts ray
                for (auto component : domain.opticalComponents) {
                    if (component->surface == hitSurface) {
                        component->traceRays(rayContext);
                        ray = rayContext.refractedRay;
                        direction = ray->direction;
                    }
                }
            }
        }
    }
};
```

---

## Wave Solver

The `IWaveSolver` solves the discrete wave equation on a grid.

```cpp
class WaveSolver : public IWaveSolver {
public:
    void step(WaveDomain& domain, double dt) override {
        // Update wave field using finite differences
        Grid<Complex> oldField = domain.fieldValues;
        Grid<Complex> newField = oldField;
        
        for (int x = 1; x < grid.width() - 1; ++x) {
            for (int y = 1; y < grid.height() - 1; ++y) {
                Complex laplacian = 
                    oldField[x+1][y] + oldField[x-1][y] +
                    oldField[x][y+1] + oldField[x][y-1] -
                    4 * oldField[x][y];
                
                double c = domain.waveSpeed;
                newField[x][y] = 2 * oldField[x][y] - oldField[x][y] +
                                 c*c * dt*dt * laplacian;
            }
        }
        
        // Apply boundary conditions
        for (auto component : domain.waveMaterials) {
            component->stepWave(waveContext, dt);  // May absorb/reflect
        }
        
        domain.fieldValues = newField;
    }
};
```

---

## Solver Execution Order

The order of solver execution matters for some multi-domain interactions:

**Typical Order**:
1. **ElectronicsSolver** - Updates voltages and currents (fastest)
2. **MotionSolver** - Updates positions based on electromagnetic forces
3. **OpticalSolver** - Traces rays affected by optical components
4. **WaveSolver** - Advances wave field

**Rationale**:
- Electronics first because it's the "source" (voltage drives motion)
- Motion second because forces depend on currents
- Optics third because it traces light through the modified scene
- Waves last because they can depend on motion (moving mirrors change boundary)

---

## Ready for Phase 2

This architecture is prepared for solver implementation. Phase 2 will:

1. **Implement ElectronicsSolver**
   - Eigen3 library for matrix operations
   - MNA matrix assembly
   - Voltage/current calculation
   - Update component simState

2. **Implement MotionSolver**
   - RK4 integration
   - Force calculations
   - Constraint handling
   - Position/velocity updates

3. **Stub OpticalSolver & WaveSolver**
   - Empty implementations until Phase 4

4. **Connect SimulationLoop**
   - Call solvers from tick()
   - Pass appropriate domain contexts

No architectural changes needed. This design scales cleanly through all physics domains.
