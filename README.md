# 24 V / 250 W Brushed DC Motor Driver

**IR2104 half-bridge · 2× IRFZ44N · ATmega328P closed-loop control**

Design, simulation, PCB layout, firmware and bench validation of a complete motor
drive for a MY1016 24 V 250 W brushed DC motor. Built for ELT10140 *Electric Drive
Systems*, Westsächsische Hochschule Zwickau, summer semester 2026.

![Block diagram](images/ELT10140_Team1_BlockDiagram.png)

---

## What this is

A single-quadrant (unidirectional) drive with hardware and software overcurrent
protection, soft-start, stall detection and a five-state supervisory state machine.
The design was carried through the full V-cycle: analytical sizing → SPICE/VSM
co-simulation → averaged-model verification in Simulink → PCB layout → assembly →
bench test.

| Metric | Value |
|---|---|
| Supply | 24 V DC, 15 A fused |
| Rated output | 250 W |
| Switching frequency | 20 kHz (Timer1 Fast PWM, ICR1 top) |
| Duty resolution | 10 bit (ICR1 = 799 @ 16 MHz, /1 prescaler) |
| Current sense | 10 mΩ shunt, ×16 non-inverting, 0–31 A → 0–5 V |
| Overcurrent trip | Hardware SD assert + firmware stall detect (> 20 A for > 1 s) |
| Board | 2-layer FR-4, 1.55 mm, 35 µm Cu |

---

## Architecture

```
24 V ──┬── LM2596 buck ──── 5 V ──── ATmega328P (Arduino Nano)
       │                                  │ OC1A (D9) ──► IN
       ├── L7812CV linear ── 12 V ────► IR2104 ──┬──► HO ──► Q1 (IRFZ44N, high side)
       │                                  ▲      └──► LO ──► Q2 (IRFZ44N, low side)
       │                            SD ───┘
       │                          (D8 + hardware fault)
       │
       └── power rail ──► Q1 drain
                          switch node ──► MY1016 motor ──► MBR2045CT ──► shunt ──► GND
                                                                          │
                                                          LM358 ×16 ──► ADC0 (A0)
```

### Design decisions worth defending

**IR2104 VCC must be 12 V, not 24 V and not 5 V.**
The device UVLO sits at ~8.2 V rising, so 5 V logic rails cannot drive it; 24 V exceeds
its absolute maximum. A dedicated L7812CV linear regulator supplies VCC. This is the
single most common way to destroy an IR2104 and the reason the design carries a second
regulator despite the buck already producing 5 V.

**Bootstrap sizing.**
1 µF ceramic + 1N4148 fast diode. At 20 kHz the high-side gate charge (Q_g ≈ 63 nC for
IRFZ44N) is replenished every 50 µs; the capacitor must hold V_BS above the 8.0 V
bootstrap UVLO across the maximum on-time. Minimum duty is clamped in firmware so the
low-side device always conducts long enough to recharge.

**Synchronous rectification over a plain flyback diode.**
Q2's channel resistance (17.5 mΩ) dissipates less than the forward drop of the Schottky
during freewheeling. The MBR2045CT remains as a parallel path covering the dead time
and as a fail-safe if the low-side gate drive is lost.

**20 kHz PWM.**
Above the audible band, below the point where IRFZ44N switching losses become the
dominant thermal term (≈ 0.35 W per device at rated current with 10 Ω gate resistors).

**Two independent overcurrent layers.**
Hardware pulls SD low in sub-microsecond time — fast enough for a short circuit.
Firmware stall detection acts on the filtered ADC current over a 1 s window — slow, but
it distinguishes a genuine mechanical jam from inrush. Neither alone is sufficient.

---

## Repository layout

```
├── firmware/          Arduino/AVR source — PWM, ADC, state machine, protection
├── hardware/
│   ├── kicad/         KiCad 10 project: schematic, PCB, footprints
│   ├── gerbers/       Fabrication output + drill files
│   └── datasheets/    Component datasheets referenced in the design
├── simulation/
│   ├── proteus/       Proteus VSM mixed-signal co-simulation (.pdsprj)
│   ├── simulink/      Averaged-model plant + discrete state machine (.slx)
│   └── results/       Captured waveforms, step responses, fault traces
├── design/            Engineering notes and derivations
├── report/            Final report (PDF) and presentation
├── bom/               Bill of materials (CSV + PDF)
└── images/            Diagrams, board photos, schematic exports
```

---

## Firmware

Bare-register AVR C++, no Arduino `analogWrite`. Timer1 in Fast PWM mode 14
(TOP = ICR1) gives a fixed 20 kHz carrier with 10-bit duty resolution.

State machine:

```
OFF ──start──► STARTING ──ramp complete──► RUNNING
 ▲                 │                          │
 │                 └──────overcurrent─────────┤
 │                                            │
 └── BRAKING ◄── stop ──────────────────────► FAULT
```

- `STARTING` ramps duty linearly to limit inrush through the motor's armature inductance.
- `FAULT` latches: SD is driven low and the state persists until an explicit reset.
- `BRAKING` shorts the armature through Q2 for dynamic braking.
- Current is read in a 4-sample exponential moving average to reject switching noise
  without adding the phase lag of a long boxcar filter.

Build: Arduino IDE or PlatformIO, target `nanoatmega328new`, 16 MHz.

---

## Simulation

**Proteus VSM** — the only tool in the chain that runs the compiled firmware against a
SPICE model of the analog stage simultaneously. Used to verify gate drive timing,
bootstrap charging, and that the ADC path actually produces the expected count for a
known shunt current.

> Note: Proteus *Analogue Analysis* is pure SPICE and does not execute the MCU
> firmware. Mixed-signal capture requires the VSM Oscilloscope instrument.
> AREF (ATmega328P pin 20) must be tied to +5 V or every ADC read returns zero and the
> overcurrent path silently fails.

**Simulink** — `MotorDriverAveraged.slx` replaces the switching stage with a duty-cycle
averaged model, letting the electrical (R–L–back-EMF) and mechanical (J–B–T_load)
dynamics run at reasonable step sizes. The firmware state machine is reimplemented in a
discrete MATLAB Function block, together with 10-bit ADC quantisation and the LM358
scaling, so the simulated controller sees the same numbers the real one does.

Two validated scenarios: light-load run-and-brake, and a rotor jam triggering the fault
path.

*Known limitation:* the load-torque model is constant rather than velocity-dependent, so
after a fault the plant shows a small reverse rotation that the real machine does not.
Documented rather than hidden — fixing it requires a Coulomb-friction/zero-speed
constraint on the mechanical block.

---

## Bill of materials

Full BOM in `bom/`. Principal parts:

| Ref | Part | Function |
|---|---|---|
| M1 | MY1016 | 24 V 250 W brushed DC motor |
| U1 | IR2104 | Half-bridge gate driver, bootstrap high-side |
| Q1, Q2 | IRFZ44N | 55 V / 49 A N-channel MOSFET, R_DS(on) 17.5 mΩ |
| D2 | MBR2045CT | 20 A Schottky freewheeling diode |
| U2 | LM358 | Current-sense amplifier, gain 16 |
| U4 | LM2596 module | 24 V → 5 V switching regulator |
| U5 | L7812CV | 24 V → 12 V linear regulator (IR2104 VCC) |
| D1 | 1N4148 | Bootstrap diode |
| — | SMBJ33A | Transient voltage suppressor, input protection |
| R_sh | 10 mΩ, 3 W | Current-sense shunt |

Mechanical: 400 × 300 × 12 mm birch plywood mounting board, ferrule-crimped
connections throughout.

---

## Reproducing

1. Open `hardware/kicad/` in KiCad 10.0 or later.
2. Flash `firmware/` to an Arduino Nano (ATmega328P, 16 MHz).
3. Supply 24 V through a 15 A fuse. **Confirm the L7812CV output is 12 V before
   connecting IR2104 VCC.**
4. Verify the switch node with an isolated probe before attaching the motor.

---

## Course context

ELT10140 Electric Drive Systems, SoSe 2026 · Westsächsische Hochschule Zwickau
Supervisors: Prof. Dr.-Ing. J. Zitzelsberger, Prof. U. David, Prof. R. Lehmann
Team 1 — Leo Kämpf (team lead), Dileep Chilla (documentation, simulation, PCB),
Vaibhav, Rahul, Shashank, Usman, Vaishali, Laert

Course handouts and lecture material are the property of WHZ and are not redistributed
in this repository.

## Licence

Source code and design files: MIT (see `LICENSE`).
Documentation and figures: CC BY 4.0.
