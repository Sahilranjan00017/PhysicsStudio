# Electronics Solver Design: MNA Implementation

This document details the Modified Nodal Analysis (MNA) solver implementation plan for Phase 2. It covers the mathematical approach, component models, matrix assembly, and validation strategy.

---

## Overview

The Electronics Solver uses **Modified Nodal Analysis** (MNA) to simulate circuits. MNA converts a circuit into a linear system of equations that can be solved for steady-state or transient node voltages and branch currents.

**Key properties**:
- Linear algebra (matrix equation: G·V = I)
- Solves instantaneously each time step
- Supports resistive networks, voltage sources, current sources
- Extends to dynamic elements (capacitors, inductors) via companion models

---

## MNA Fundamentals

### Circuit Topology

Every circuit has:
- **Nodes**: connection points (pads in our system)
- **Components**: resistors, voltage sources, etc. connected between nodes
- **Ground node**: reference (node 0, voltage = 0V)

### KCL (Kirchhoff's Current Law)

At each node: sum of currents entering = sum of currents leaving

```
i_in + i_source = i_out + i_resistor
```

### MNA Matrix Equation

```
┌────────────────┬──────────┐ ┌──────┐   ┌────────┐
│   G (n×n)      │ B (n×m)  │ │ V    │   │ I_ext  │
│ (conductance)  │          │ │(volt)│ = │(source)│
├────────────────┼──────────┤ │      │   │        │
│   C (m×n)      │ D (m×m)  │ │ I_s  │   │ E      │
│                │ (often 0)│ │(curr)│   │(voltage)│
└────────────────┴──────────┘ └──────┘   └────────┘
```

Where:
- **n** = number of nodes (excluding ground)
- **m** = number of independent voltage sources
- **G** = conductance matrix (resistors)
- **B, C, D** = incidence matrices (voltage sources)
- **V** = node voltages (what we solve for)
- **I_s** = branch currents through voltage sources
- **I_ext** = external current injections
- **E** = voltage source values

---

## Component Models

### 1. Resistor (Linear, Static)

**Symbol**: ─/\/\/\─

**Element equation** (Ohm's Law):
```
i = (v_a - v_b) / R = G · (v_a - v_b)

where G = 1/R (conductance)
```

**MNA Stamp** (connecting nodes a and b):
```
        a      b
    [  +G    -G  ]
a   [            ]
    [  -G    +G  ]
b
```

Implementation:
```cpp
void Resistor::stampMNA(double dt) {
    double R = properties["resistance"].toDouble();
    double G = 1.0 / R;  // conductance
    
    int nodeA = getNodeIndex(pads[0]);
    int nodeB = getNodeIndex(pads[1]);
    
    gMatrix[nodeA][nodeA] += G;
    gMatrix[nodeA][nodeB] -= G;
    gMatrix[nodeB][nodeA] -= G;
    gMatrix[nodeB][nodeB] += G;
}
```

### 2. Voltage Source (Linear, Independent)

**Symbol**: +−

**Element constraint**:
```
v_a - v_b = E  (E = voltage source value)
```

**MNA Stamp** (as constraint equation, row = source_index):
```
Added to augmented matrix with constraint row:

        a      b      i_s
    [ +1    -1      0  ]  (constraint: v_a - v_b - E = 0)
a   [                   ]
    [ -1    +1      0  ]  (KCL contributions)
b   [                   ]
i_s [  0     0      0  ]  (current variable)
```

Implementation:
```cpp
void VoltageSource::stampMNA(double dt) {
    double E = properties["voltage"].toDouble();
    
    int nodeA = getNodeIndex(pads[0]);
    int nodeB = getNodeIndex(pads[1]);
    int sourceRow = getSourceIndex(this);
    
    // Stamp voltage constraint
    cMatrix[sourceRow][nodeA] = 1.0;
    cMatrix[sourceRow][nodeB] = -1.0;
    eVector[sourceRow] = E;
    
    // Stamp transposed for KCL
    bMatrix[nodeA][sourceRow] = 1.0;
    bMatrix[nodeB][sourceRow] = -1.0;
}
```

### 3. Current Source (Linear, Independent)

**Symbol**: ↑ | I

**Element equation**:
```
i_injected = I  (current from a to b)
```

**MNA Stamp** (into current vector only):
```
iVector[nodeA] += I;
iVector[nodeB] -= I;
```

Implementation:
```cpp
void CurrentSource::stampMNA(double dt) {
    double I = properties["current"].toDouble();
    
    int nodeA = getNodeIndex(pads[0]);
    int nodeB = getNodeIndex(pads[1]);
    
    iVector[nodeA] += I;
    iVector[nodeB] -= I;
}
```

### 4. Capacitor (Dynamic, RK4 Integration)

**Symbol**: ╱╱

**Element equations**:
```
i = C · dv/dt  (capacitor current)
```

**Challenge**: Involves derivative (dv/dt). Can't solve directly in steady-state.

**Solution**: Use **companion model** (discretized approximation)

For time step dt:
```
Capacitor ≈ Resistor + Voltage Source

Companion conductance: G_c = C / dt
Companion voltage:     E_c = v_previous + i_previous · dt / C
```

Then stamp as resistor + voltage source.

After solving, update capacitor state:
```
i_new = G_c · (v_new - v_old)
v_old = v_new  (for next time step)
```

Implementation:
```cpp
void Capacitor::stampMNA(double dt) {
    double C = properties["capacitance"].toDouble();
    double v_prev = simState["voltage"].toDouble();
    double i_prev = simState["current"].toDouble();
    
    double G_companion = C / dt;
    double E_companion = v_prev + i_prev * dt / C;
    
    int nodeA = getNodeIndex(pads[0]);
    int nodeB = getNodeIndex(pads[1]);
    
    // Stamp as resistor
    gMatrix[nodeA][nodeA] += G_companion;
    gMatrix[nodeA][nodeB] -= G_companion;
    gMatrix[nodeB][nodeA] -= G_companion;
    gMatrix[nodeB][nodeB] += G_companion;
    
    // Stamp as voltage source (in current vector)
    iVector[nodeA] += G_companion * E_companion;
    iVector[nodeB] -= G_companion * E_companion;
}
```

After solve, update state:
```cpp
void Capacitor::postSolve(double dt) {
    double v_new = getVoltage();
    double v_prev = simState["voltage"].toDouble();
    double C = properties["capacitance"].toDouble();
    
    double i_new = C * (v_new - v_prev) / dt;
    
    simState["voltage"] = v_new;
    simState["current"] = i_new;
}
```

### 5. Inductor (Dynamic, RK4 Integration)

**Symbol**: ⚬⚬⚬

**Element equations**:
```
v = L · di/dt  (inductor voltage)
```

**Companion model**:
```
Inductor ≈ Resistor + Current Source

Companion resistance: R_c = L / dt
Companion current:    I_c = i_previous + v_previous · dt / L
```

Similar to capacitor but for current equation.

---

## Matrix Assembly Algorithm

### Phase 2a: Build Node List

```cpp
Set<ConnectionPad*> electricalNodes;

// Iterate all components
for (auto component : scene->items()) {
    if (auto* comp = dynamic_cast<BaseComponent*>(component)) {
        for (auto pad : comp->pads) {
            if (pad->domain == DomainType::Electrical) {
                electricalNodes.insert(pad);
            }
        }
    }
}

// Create node map
Map<ConnectionPad*, int> nodeMap;
int nodeIdx = 0;
for (auto pad : electricalNodes) {
    nodeMap[pad] = nodeIdx++;
}

// Ground is node 0
groundNode = 0;
numNodes = electricalNodes.size();
```

### Phase 2b: Count Voltage Sources

```cpp
int numVoltageSources = 0;
for (auto component : scene->items()) {
    if (dynamic_cast<VoltageSource*>(component)) {
        numVoltageSources++;
    }
}
```

### Phase 2c: Allocate Matrices

```cpp
int n = numNodes;
int m = numVoltageSources;

Eigen::MatrixXd G(n + m, n + m);
G.setZero();

Eigen::VectorXd I(n + m);
I.setZero();
```

### Phase 2d: Stamp Components

```cpp
for (auto component : scene->items()) {
    if (auto* comp = dynamic_cast<BaseComponent*>(component)) {
        if (comp->pads.size() > 0 && 
            comp->pads[0]->domain == DomainType::Electrical) {
            comp->stampMNA(dt);  // Component adds to G and I
        }
    }
}
```

### Phase 2e: Solve

```cpp
Eigen::VectorXd V = G.inverse() * I;
// Or better (more numerically stable):
Eigen::VectorXd V = G.lu().solve(I);
```

### Phase 2f: Extract Results

```cpp
for (auto [pad, nodeIdx] : nodeMap) {
    double voltage = V[nodeIdx];
    // Store in component state
    auto owner = pad->owner;  // Get component that owns this pad
    owner->simState["voltage@" + pad->padId] = voltage;
}

// Calculate currents through components
for (auto wire : scene->wires()) {
    ConnectionPad* padA = wire->startPad;
    ConnectionPad* padB = wire->endPad;
    
    int idxA = nodeMap[padA];
    int idxB = nodeMap[padB];
    
    double vA = V[idxA];
    double vB = V[idxB];
    
    // Assume resistive path or direct connection
    double current = (vA - vB) / DEFAULT_WIRE_RESISTANCE;
    wire->signal = QVariant(current);
}
```

---

## Validation Test Cases

### Test 1: Voltage Divider

**Circuit**: 
```
+5V ─[1kΩ]─┬─[1kΩ]─ GND
           │
          (V_mid = ?)
```

**Expected**: V_mid = 2.5V (half of 5V due to equal resistors)

**Test code**:
```cpp
TEST(ElectronicsSolver, VoltageDidider) {
    auto src = new VoltageSource();
    src->properties["voltage"] = 5.0;
    
    auto r1 = new Resistor();
    r1->properties["resistance"] = 1000.0;
    
    auto r2 = new Resistor();
    r2->properties["resistance"] = 1000.0;
    
    // Connect: src → r1 → r2 → gnd
    wire(src->pad(1), r1->pad(0));
    wire(r1->pad(1), r2->pad(0));
    wire(r2->pad(1), gnd);
    
    solver.solve(domain, 0.016);
    
    double v_mid = r2->pad(0)->voltage;
    EXPECT_NEAR(v_mid, 2.5, 0.01);
}
```

### Test 2: Series Current

**Circuit**:
```
+10V ─[1kΩ]─[1kΩ]─[1kΩ]─ GND
```

**Expected**: I = 10V / 3kΩ = 3.33mA through each resistor

**Test code**:
```cpp
TEST(ElectronicsSolver, SeriesResistor) {
    // Create series circuit
    // Expect same current through all resistors
    double expectedCurrent = 10.0 / 3000.0;  // 3.33mA
    
    // Verify with ammeter
    // ...
}
```

### Test 3: Circuit with Ammeter

**Circuit**:
```
+5V ─[1kΩ]─[Ammeter]─ GND
```

**Expected**: Ammeter reads 5mA

**Test code**:
```cpp
TEST(ElectronicsSolver, Ammeter) {
    // Ammeter is ideal (zero resistance)
    // Should read exact current
}
```

### Test 4: Voltmeter Measurement

**Circuit**:
```
+10V ─[1kΩ]─┬─[1kΩ]─ GND
            │
         [Voltmeter]
            │
           GND
```

**Expected**: Voltmeter reads 5V

---

## Phase 1 vs Phase 2

### Phase 1 (Components, No Solver)

- **Implement**: Resistor, VoltageSource, CurrentSource, Ammeter, Voltmeter
- **Stub**: Capacitor, Inductor (empty implementations)
- **Test**: Components exist, can be placed, can serialize
- **No simulation yet**

### Phase 2 (Electronics Solver)

- **Implement**: ElectronicsSolver with full MNA
- **Complete**: Capacitor, Inductor with RK4
- **Test**: MNA matrix assembly, voltage/current calculations
- **Enable**: Full circuit simulation at 60 FPS

---

## Numerical Stability Considerations

### Matrix Conditioning

MNA matrices can be ill-conditioned if:
- Very large resistances (1GΩ+) mixed with very small (1mΩ)
- Unconnected node (isolated component)

**Mitigation**:
- Use LU decomposition (more stable than inverse)
- Add small damping conductance in parallel (for stability)
- Validate node connectivity before solving

### Singular Matrix

Occurs if:
- Floating node (not connected to ground)
- Two voltage sources in parallel

**Detection**:
```cpp
if (G.determinant() == 0) {
    emit solverError("Singular matrix: check circuit topology");
    return;
}
```

### Transient Behavior

Dynamic elements (capacitors, inductors) can cause oscillation if dt too large.

**Stability condition**: dt < 2/(RC) for RC circuit

**Validation**: Run capacitor test with various dt values

---

## Performance Notes

**Expected timing** (for typical circuit with 50 nodes, 10 voltage sources):

```
Build node list:     < 1ms
Build MNA matrix:    ~5-10ms
Solve (LU):          ~20-30ms
Extract results:     < 1ms
Post-processing:     < 5ms
────────────────────────────
Total per frame:     ~35-50ms (at 60 FPS needs < 16ms)
```

**Optimization opportunities**:
- Sparse matrix format (for large circuits)
- Incremental solving (changes only affect local matrix)
- GPU acceleration (for very large networks)

Currently Phase 2 targets **< 100 node circuits** where dense matrix is fine.

---

## Dependencies

**Required**:
- Eigen3 (linear algebra)
- Qt6 (component ownership, signals)

**Optional**:
- SparseEigen (for Phase 3+ optimization)

---

## Ready for Phase 2 Implementation

This design is complete and implementable. Phase 2 execution will:

1. Implement Resistor::stampMNA()
2. Implement VoltageSource::stampMNA()
3. Implement CurrentSource::stampMNA()
4. Implement ElectronicsSolver::solve()
5. Implement post-solve state updates
6. Write validation tests (verify math)
7. Integrate with SimulationLoop
8. Test full circuit simulation

Estimated effort: **12-16 hours** for experienced developer.
