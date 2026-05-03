# Electronics Solver Design

The electronics domain uses Modified Nodal Analysis (MNA). This document plans the Phase 2 solver while keeping Phase 1 component and registry decisions aligned.

## MVP Scope

The first electronics MVP should support:

- Ground
- Resistor
- DC voltage source
- DC current source
- Ammeter
- Voltmeter
- Wires and electrical nodes

Next components:

- Capacitor using transient companion model
- Inductor using transient companion model
- Switch
- Lamp / LED
- Diode with Newton-Raphson linearization

## Domain Objects

Planned classes:

```text
ElectronicsDomain
  owns CircuitGraph
  owns solve settings
  owns diagnostics

CircuitGraph
  maps ConnectionPad groups to node IDs
  stores electrical BaseComponent pointers
  stores wire topology
  tracks dirty topology flag

MNABuilder
  allocates matrix A and RHS b
  provides stamp helper methods
  stores voltage source branch indices

ElectronicsSolver
  orchestrates graph update, stamping, solve, result writeback
```

## MNA Equation Shape

For `n` non-ground nodes and `m` independent voltage sources:

```text
[ G  B ] [ v ] = [ i ]
[ C  D ] [ j ]   [ e ]
```

- `G`: conductance matrix.
- `B`: voltage source incidence matrix.
- `C`: transpose of `B` for independent sources.
- `D`: usually zero for ideal voltage sources.
- `v`: unknown node voltages.
- `j`: unknown voltage-source branch currents.
- `i`: known current injections.
- `e`: known source voltages.

## Stamp Rules

### Resistor

For resistance `R` between nodes `a` and `b`:

```text
g = 1 / R
G[a][a] += g
G[b][b] += g
G[a][b] -= g
G[b][a] -= g
```

If either side is ground, skip the ground row/column and stamp only the non-ground side.

Properties:

```json
{
  "resistance": 1000.0,
  "maxCurrent": 0.25,
  "destructible": true
}
```

### Voltage Source

For voltage `V` from node `a` to `b`, allocate branch index `k`:

```text
B[a][k] += 1
B[b][k] -= 1
C[k][a] += 1
C[k][b] -= 1
e[k] = V
```

Properties:

```json
{
  "voltage": 5.0,
  "internalResistance": 0.0
}
```

### Current Source

For current `I` from node `a` to `b`:

```text
i[a] -= I
i[b] += I
```

Properties:

```json
{
  "current": 0.01
}
```

### Ammeter

MVP behavior:

- Model as a near-zero resistance element with a minimum numerical resistance such as `1e-6`.
- Store measured branch current in `simState["current"]`.

Properties:

```json
{
  "range": 1.0,
  "burdenResistance": 0.000001
}
```

### Voltmeter

MVP behavior:

- Model as a high resistance element, such as `1e9`.
- Store measured node voltage difference in `simState["voltage"]`.

Properties:

```json
{
  "range": 10.0,
  "inputResistance": 1000000000.0
}
```

## Solver Flow

```text
ElectronicsSolver::solve(domain, dt)
  if topology dirty:
      CircuitGraph rebuilds node IDs from wires and pads
      MNABuilder assigns voltage source branch indices

  MNABuilder clears A and b

  for each electronics component:
      component stamps A and b through builder helpers

  apply ground reference
  solve A*x = b
  write node voltages to graph
  compute branch currents
  update component simState
  check overloads
  publish diagnostics
```

## Numerical Plan

Phase 2 can use Eigen directly:

```cpp
Eigen::MatrixXd A;
Eigen::VectorXd b;
Eigen::VectorXd x = A.colPivHouseholderQr().solve(b);
```

Later improvements:

- Sparse matrices for larger classroom circuits.
- LU decomposition reuse when topology is unchanged.
- Newton-Raphson loop for diodes/transistors.
- Time-step companion models for capacitors and inductors.

## Overload Detection

After solving:

```text
if abs(current) > maxCurrent:
    destroyed = true
if abs(power) > maxPower:
    destroyed = true
```

When destroyed:

- Component stops stamping normal behavior.
- UI shows warning state.
- Repair action replaces live state with a fresh instance.

## Phase 1 Registry Implications

The component registry must define:

- `typeId`
- display name
- icon path
- category path
- default properties
- factory function
- pad definitions

Example:

```json
{
  "typeId": "ELEC_RES",
  "displayName": "Resistor",
  "category": "Electronics/Analog/Passive",
  "properties": {
    "resistance": 1000.0,
    "maxCurrent": 0.25,
    "destructible": true
  },
  "pads": [
    { "padId": "a", "domain": "Electrical", "type": "Bidirectional" },
    { "padId": "b", "domain": "Electrical", "type": "Bidirectional" }
  ]
}
```

## Validation Tests

Minimum Phase 2 tests:

- 10 V source with two 1 kOhm resistors: midpoint is 5 V.
- 12 V source with 2 kOhm and 4 kOhm series resistors: current is 2 mA.
- 9 V source with 3 kOhm and 6 kOhm parallel resistors: equivalent resistance is 2 kOhm.
- Voltmeter reads node difference without materially changing circuit.
- Ammeter reads branch current with negligible burden.
- Overload flag triggers when current exceeds `maxCurrent`.

