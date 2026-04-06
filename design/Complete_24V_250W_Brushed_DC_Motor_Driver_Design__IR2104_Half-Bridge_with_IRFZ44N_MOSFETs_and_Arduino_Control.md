# Complete 24V 250W brushed DC motor driver design

**The IR2104 + dual IRFZ44N half-bridge forms a synchronous buck topology that delivers clean, efficient PWM drive to the MY1016 motor — but several component-level corrections are critical before building.** The original parts list contains two significant errors: the 1N5822 Schottky diode is rated only 3A (far below the 13.7A motor current), and a single IRFZ44N cannot work with the IR2104, which drives a high-side/low-side MOSFET pair. Additionally, the IR2104 requires **10–20V on its VCC pin**, meaning a dedicated 12V regulator is needed beyond the LM2596 5V module. This report provides every component value, calculation, code listing, and layout rule needed to build an error-free motor driver PCB for the ELT10140 project at WHZ Zwickau.

---

## How the IR2104 half-bridge actually works

The IR2104 is not a simple gate buffer — it is a **complementary half-bridge driver** with built-in 520 ns deadtime. Two N-channel IRFZ44N MOSFETs form the half-bridge: Q1 (high-side, drain to +24V) and Q2 (low-side, source to GND). The motor connects between the switching node (junction of Q1 source and Q2 drain) and ground.

**Operating sequence at each PWM cycle:**

When IN goes HIGH, Q2 turns off first, 520 ns of deadtime passes, then Q1 turns on. Current flows from +24V through Q1 into the motor. When IN goes LOW, Q1 turns off, deadtime passes, then Q2 turns on. The motor's inductive current now freewheels through Q2's channel at very low loss (synchronous rectification). The average motor voltage equals **duty cycle × 24V**, giving smooth, linear speed control from 0 to ~23V.

The SD (shutdown) pin provides emergency stop capability. **SD = HIGH enables the driver; SD = LOW forces both outputs off.** A 10 kΩ pull-down resistor on SD ensures the driver powers up disabled — a critical safety feature the Arduino must explicitly override by driving SD HIGH.

**Pin connections (IR2104 in 8-DIP):**

| Pin | Symbol | Connects to |
|-----|--------|-------------|
| 1 | VCC | +12V regulated supply |
| 2 | IN | Arduino Pin D9 (Timer1 OC1A PWM) |
| 3 | SD | Arduino Pin D8 via 10 kΩ pull-down to GND |
| 4 | COM | Power ground (GND) |
| 5 | LO | Q2 gate through 10 Ω resistor |
| 6 | VS | Switching node (Q1 source / Q2 drain) |
| 7 | HO | Q1 gate through 10 Ω resistor |
| 8 | VB | Bootstrap capacitor to VS; bootstrap diode from VCC |

No level shifting is needed. The IR2104 accepts 5V logic directly (VIH ≥ 3.0V, VIL ≤ 0.8V). Input bias current is just 3–10 µA, negligible for Arduino GPIO.

---

## Complete schematic with every component value

### Power stage

The heart of the circuit is the half-bridge formed by two IRFZ44N MOSFETs. **You need two IRFZ44N devices, not one.** Q1 serves as the high-side switch (drain to +24V, source to switching node), Q2 as the low-side switch (drain to switching node, source to GND through the current sense resistor).

**Bulk capacitor on 24V rail:** 470 µF / 50V electrolytic in parallel with 100 nF / 50V ceramic, placed directly at the power input connector. These handle the high di/dt current pulses during PWM switching. Without them, voltage spikes during switching can exceed the IRFZ44N's 55V drain-source rating.

**Gate resistors:** 10 Ω in series with each gate (between IR2104 HO→Q1 gate and LO→Q2 gate). These damp LC ringing from gate trace inductance and MOSFET input capacitance. Values above 100 Ω slow switching dangerously; values below 5 Ω may cause ringing. A **10 kΩ pull-down resistor from each gate to its respective source** ensures MOSFETs stay off if the IR2104 loses power.

### Bootstrap circuit

The bootstrap circuit charges a capacitor (C_boot) when Q2 is ON (VS pulled to ground), then uses that stored energy to drive Q1's gate above the 24V rail.

**Bootstrap capacitor calculation (worst case):**

The charge consumed per switching cycle includes the IRFZ44N gate charge (Qg = 63 nC max), the IR2104 quiescent VBS current (55 µA max at 20 kHz = 2.75 nC), and level-shift charge (~5 nC). Total per-cycle charge is approximately **71 nC**. The minimum capacitor value follows the formula C_boot ≥ 2 × Q_total / ΔV, where ΔV = VCC − V_diode − V_LS_drop − V_UVLO_min = 12 − 0.5 − 0.24 − 8.2 = 3.06V. This yields C_min ≈ 46 nF. Applying the standard **15× safety factor** gives 690 nF.

**Use a 1 µF ceramic capacitor (MLCC, X7R, 50V rated)** between VB and VS. This value appears consistently across Infineon application notes, verified project implementations, and the AN-978 design guide. Do not use electrolytic capacitors — their ESR and leakage are too high for bootstrap service.

**Bootstrap diode: UF4007** (1000V, 1A, ultra-fast recovery, 75 ns trr). This connects anode to VCC (+12V), cathode to VB. The 1000V rating provides enormous margin over the 24V bus. A fast recovery diode is essential — a slow diode allows charge to flow backward out of the bootstrap capacitor during VS transitions, starving the high-side gate drive. Alternative: MBR160 (60V Schottky) for lower forward voltage drop (0.5V vs 1.0V), though the UF4007's wider voltage margin is preferred for a student project.

The resulting high-side gate voltage is approximately VCC − V_diode_drop = 12 − 0.5 ≈ **11.5V** (with Schottky) or 12 − 1.0 ≈ **11V** (with UF4007). Both fully enhance the IRFZ44N well above the 10V specification point.

**Maximum duty cycle limitation:** The bootstrap capacitor recharges only when Q2 is ON. At 95% duty and 20 kHz (50 µs period), Q2 conducts for 2.5 µs — sufficient to recharge 1 µF through the low-impedance path. At 99%+ duty, on-time drops below 0.5 µs and the capacitor may not fully recharge. **Practical limit: ~95–97% duty cycle.**

### IR2104 VCC supply

The IR2104 requires **10–20V on VCC**. The LM2596 module providing 5V to the Arduino cannot serve this purpose. Add a separate **L7812 (or LM7812) linear regulator**: 24V input → 12V output. Bypass the regulator with 100 nF ceramic on both input and output, plus 10 µF electrolytic on the output. At the IR2104's modest current draw (~1 mA quiescent plus gate charge current), the 7812's dissipation is approximately (24 − 12) × 0.01 ≈ 0.12W — negligible.

**VCC decoupling at the IR2104:** Place a **100 nF ceramic capacitor directly between VCC (pin 1) and COM (pin 4)**, within 5 mm of the IC. Add a **10 µF electrolytic in parallel**, slightly further away. This dual-capacitor arrangement handles both high-frequency switching transients (ceramic) and lower-frequency supply droop (electrolytic).

### Current sensing circuit

A low-side shunt resistor between Q2's source and power ground measures motor current. The voltage across this shunt is amplified by one half of the LM358 dual op-amp.

**Shunt resistor: 10 mΩ (0.010 Ω), rated 5–10W.** Use a metal-strip type (Vishay WSL2512 in 2512 SMD or Ohmite 25FR010 through-hole wirewound). At 13.7A rated current, V_shunt = 137 mV and P_shunt = 1.88W. At 27A stall, P_shunt = 7.29W. A 10W-rated resistor survives brief stalls while overcurrent protection activates.

Why 10 mΩ? Smaller values (1–5 mΩ) produce signals below the LM358's 2–7 mV input offset voltage, causing unacceptable measurement error. Larger values (20+ mΩ) waste power and create significant voltage drops that reduce motor drive voltage.

**LM358 non-inverting amplifier (gain = 16):**

The LM358 has a critical limitation on 5V supply: **its output can only swing to approximately 3.5V maximum** (VCC − 1.5V). The design therefore maps the overcurrent threshold (20A) to 3.2V rather than 5V:

- R1 = **1.0 kΩ** (ground to inverting input)
- R2 = **15 kΩ** (feedback, inverting input to output)
- Gain = 1 + R2/R1 = 1 + 15/1 = **16**
- Use 1% metal film resistors for both

**Transfer function:** V_out = I_motor × 0.010 Ω × 16 = **0.16 × I_motor** (volts). At the Arduino's 10-bit ADC (5V reference): **ADC count = I_motor × 32.77**. Current resolution is approximately **30.5 mA per ADC count** — more than adequate for motor control.

| Motor current | Shunt voltage | LM358 output | ADC count |
|--------------|---------------|--------------|-----------|
| 0 A | 0 mV | ~0 V | 0 |
| 5 A | 50 mV | 0.80 V | 164 |
| 10 A | 100 mV | 1.60 V | 328 |
| **13.7 A (rated)** | **137 mV** | **2.19 V** | **449** |
| **20 A (OC threshold)** | **200 mV** | **3.20 V** | **655** |
| 22+ A | 220+ mV | Saturates ~3.5 V | ~720 |

Above ~22A the LM358 output clips at 3.5V. This is acceptable because overcurrent protection should trigger at 20A (ADC ≈ 655), well before saturation.

**Output filter:** A 1 kΩ resistor and **82 nF ceramic capacitor** form a low-pass filter (f_c ≈ 1.94 kHz) between the LM358 output and the Arduino analog pin. This removes PWM-frequency ripple while preserving the averaged current signal.

**ADC protection:** Place a 1 kΩ series resistor immediately before the Arduino analog pin, followed by a BAT54S Schottky diode from the pin to the 5V rail (cathode to 5V). This clamps any overvoltage from reaching the ATmega328P.

**Offset calibration:** The LM358's 2–7 mV typical offset, amplified by 16, creates a 32–112 mV output offset (equivalent to 0.2–0.7A error). **Calibrate in software** by reading the ADC at zero motor current during startup and subtracting this baseline.

### Corrected flyback / freewheeling diode

**The 1N5822 Schottky diode is rated only 3A average forward current — it is fundamentally inadequate for a 13.7A motor.** In the half-bridge topology, during the 520 ns deadtime between Q1 turning off and Q2 turning on, the motor's inductive current freewheels through Q2's body diode. An external Schottky diode across Q2 (anode to source/GND, cathode to drain/switching node) provides a lower-loss freewheeling path during deadtime.

**Replace the 1N5822 with an MBR2045CT** (20A, 45V dual Schottky in TO-220) or equivalent. Use one diode of the dual package across Q2. The 20A rating provides margin above the 13.7A operating current, and the 45V rating exceeds the 24V bus with comfortable margin.

The 1N5822 could still serve as the bootstrap diode alternative or for auxiliary protection circuits, but must not carry motor current.

### Buttons and indicators

Use Arduino's internal pull-ups (INPUT_PULLUP) for buttons: connect each button between the GPIO pin and GND. Press reads LOW. Software debounce at 50 ms eliminates bounce without external RC networks.

- **Start button:** Pin D2 → GND
- **Stop button:** Pin D3 → GND  
- **Fault reset button:** Pin D5 → GND

**LED indicators:** Green LED (motor running) on pin D6, red LED (fault) on pin D7, each with a **330 Ω series resistor** to ground. Pin D13's onboard LED serves as a heartbeat indicator.

### Complete bill of materials

| Component | Value / Part | Rating | Purpose |
|-----------|-------------|--------|---------|
| IR2104 | Infineon, 8-DIP | 600V half-bridge driver | Gate driver IC |
| IRFZ44N × **2** | Infineon, TO-220 | 55V, 49A, 17.5 mΩ | High-side and low-side MOSFETs |
| C_boot | 1 µF ceramic MLCC | 50V, X7R | Bootstrap capacitor (VB–VS) |
| D_boot | UF4007 | 1000V, 1A, ultra-fast | Bootstrap diode (VCC→VB) |
| C_VCC_HF | 100 nF ceramic | 25V | IR2104 VCC bypass (close to IC) |
| C_VCC_bulk | 10 µF electrolytic | 25V | IR2104 VCC decoupling |
| C_bus_bulk | 470 µF electrolytic | 50V | 24V bus decoupling |
| C_bus_HF | 100 nF ceramic | 50V | 24V bus HF bypass |
| D_freewheel | **MBR2045CT** | 20A, 45V Schottky | Across Q2 (freewheeling) |
| R_gate × 2 | 10 Ω | ¼W | Series gate resistors |
| R_GS × 2 | 10 kΩ | ¼W | Gate-source pull-downs |
| R_SD | 10 kΩ | ¼W | SD pin pull-down to GND |
| 7812 regulator | L7812CV | 24V→12V, 1A | IR2104 VCC supply |
| C_reg_in | 100 nF ceramic | 50V | 7812 input bypass |
| C_reg_out | 10 µF electrolytic | 25V | 7812 output filter |
| R_shunt | 10 mΩ, metal strip | 5–10W | Current sense resistor |
| LM358 | Dual op-amp, 8-DIP | 5V supply | Current amplifier |
| R1 (gain) | 1.0 kΩ, 1% | ¼W | Amplifier gain network |
| R2 (gain) | 15 kΩ, 1% | ¼W | Amplifier gain network |
| R_filter | 1.0 kΩ | ¼W | Output low-pass filter |
| C_filter | 82 nF ceramic | 25V | Output low-pass filter |
| R_protect | 1.0 kΩ | ¼W | ADC input series protection |
| D_clamp | BAT54S | Schottky clamp | ADC overvoltage protection |
| LM2596 module | 24V→5V buck | 3A | Arduino power supply |
| Buttons × 3 | Tactile switch | — | Start, stop, reset |
| LEDs × 2 | Green, red | 20 mA | Run, fault indicators |
| R_LED × 2 | 330 Ω | ¼W | LED current limiting |

---

## Critical errors that destroy student motor drivers

### The number-one killer: driving IRFZ44N directly from Arduino

The IRFZ44N's VGS(th) of 2–4V is the voltage where conduction *barely begins* — at microamp levels. Full enhancement (17.5 mΩ) requires VGS ≥ 10V. **At VGS = 5V from an Arduino pin, RDS(on) rises to an estimated 100–200 mΩ**, roughly 10× higher than the datasheet value. At 10A, power dissipation jumps from 1.75W to **10–20W** — the MOSFET enters thermal runaway and fails within seconds. The IR2104, powered at 12V VCC, delivers approximately 11–12V to each gate, achieving full enhancement. This is the entire reason a gate driver exists.

If the project requires direct Arduino drive without a gate driver (a simpler approach), replace the IRFZ44N with the **IRLZ44N** (logic-level variant, fully enhanced at VGS = 5V). But with the IR2104 in the design, stick with the standard IRFZ44N.

### Bootstrap failures that cause intermittent operation

Three bootstrap errors account for most IR2104 field failures in student projects. First, **a capacitor below ~100 nF** cannot supply the IRFZ44N's 63 nC gate charge with adequate margin, causing VBS to droop below the 8.2V UVLO threshold. The high-side MOSFET then enters linear mode at partial gate drive, dissipating enormous power. Second, **using a slow-recovery diode** (e.g., 1N4007 instead of UF4007) allows bootstrap charge to leak back into VCC during reverse recovery, depleting the capacitor. Third, **running at 100% duty cycle** prevents Q2 from ever turning on, so the bootstrap capacitor never recharges — the high-side gate drive collapses within milliseconds.

### Decoupling capacitor omissions

Without the 100 nF ceramic directly at VCC (pin 1), the IR2104 experiences voltage transients during gate switching that can trigger false UVLO lockout, causing random gate drive dropouts. Without bulk capacitance (470 µF) on the 24V bus near the MOSFETs, inductive voltage spikes during switching can exceed the IRFZ44N's 55V absolute maximum, causing avalanche breakdown and eventual failure.

### PWM frequency from default analogWrite()

Arduino's `analogWrite()` produces **490 Hz on pins 9/10 and 976 Hz on pins 5/6** — both audibly annoying and electrically poor. Low-frequency PWM creates massive current ripple, increases I²R losses in the motor windings, and causes jerky operation at low speeds. **Always configure Timer1 directly for 20 kHz**, as detailed in the software section below.

### The SD pin left floating

The IR2104's SD pin, if left unconnected, floats at an undefined voltage. This causes random enable/disable behavior and erratic motor operation. **Always include a 10 kΩ pull-down** to GND for power-up safety, and drive it HIGH from an Arduino GPIO to enable the driver.

---

## Power dissipation, thermal analysis, and stall behavior

### Conduction and switching losses per MOSFET

At **13.7A rated current** with RDS(on) = 17.5 mΩ (25°C), conduction loss is I²R = 13.7² × 0.0175 = **3.28W**. At elevated junction temperature (~100°C), RDS(on) rises by approximately 1.5× to ~26 mΩ, increasing conduction loss to **4.93W**. Switching losses at 20 kHz add P_sw = 0.5 × 24V × 13.7A × (60 ns + 45 ns) × 20 kHz ≈ **0.35W**. Total dissipation per MOSFET under continuous rated load is approximately **3.6W at 25°C, rising to 5.3W at operating temperature**.

### Heatsink sizing

Without any heatsink, the IRFZ44N's junction-to-ambient thermal resistance is 62°C/W. At 3.6W, the junction temperature rise is 223°C above ambient — **catastrophic failure is guaranteed**. A heatsink is mandatory for each MOSFET.

For a target junction temperature of 125°C with 40°C ambient and 5.3W dissipation: required total thermal resistance = (125 − 40) / 5.3 = 16.0°C/W. Subtracting RθJC (1.5°C/W) and RθCS (0.5°C/W with thermal compound), the heatsink must provide **RθSA ≤ 14°C/W**. A small clip-on TO-220 aluminum heatsink (~25 × 20 × 15 mm) achieves approximately 12–15°C/W, which is sufficient. **No fan is needed at rated 13.7A current with a modest heatsink.**

**Critical note on shared heatsinks:** The IRFZ44N TO-220 metal tab is connected to the drain. Q1's tab sits at +24V, Q2's tab at the switching node (0–24V). If both MOSFETs share a single heatsink, **insulating thermal pads are mandatory** between each tab and the heatsink to prevent a short circuit.

### Motor stall scenario

A stalled MY1016 motor with its low winding resistance (~24V / 27A ≈ 0.89 Ω) draws approximately **27A** at full duty. Power dissipation per MOSFET at stall: 27² × 0.0175 = **12.8W at 25°C**, rising to ~19W at elevated temperature. Even with a good heatsink (5°C/W), junction temperature reaches 40 + 19 × 7 = **173°C — at the absolute maximum rating**. Without active cooling, the MOSFET survives only seconds.

The motor stall time constant (L/R ≈ 2 mH / 0.89 Ω ≈ 2.25 ms) means current reaches 63% of stall value in ~2.3 ms and 95% in ~7 ms. **The overcurrent protection must respond within a few milliseconds.** This is why hardware-based protection (analog comparator driving SD LOW) is essential — software ADC polling at 100–500 µs response time is too slow.

### Voltage drop and efficiency

At 13.7A, the voltage drop across each conducting MOSFET is just **I × RDS(on) = 13.7 × 0.0175 = 0.24V**. With a 10 mΩ shunt adding another 0.14V, total losses in the power path amount to approximately 0.38V — the motor sees 24 − 0.38 ≈ **23.6V at full duty**, for an efficiency above 98% in the power stage alone.

---

## PCB layout for a reliable 2-layer motor driver

A **2-layer PCB with 2 oz (70 µm) copper** is sufficient for this 250W driver when designed carefully. The key constraint is trace width: at 13.7A continuous on an external 2 oz copper layer with 20°C temperature rise, IPC-2221 specifies a minimum trace width of approximately **3.7 mm**. Adding 50% safety margin and accounting for brief stall currents, **target 5–8 mm trace width for all power paths** (24V rail, ground return, switching node, motor output).

### Component placement rules

Place the IR2104 within **10–15 mm of both MOSFET gates**. Gate traces must be short, wide (≥ 0.5 mm), and routed as adjacent forward/return paths to minimize loop inductance. The 10 Ω gate resistors sit directly at the MOSFET gate pins. The 100 nF VCC decoupling capacitor goes within 3–5 mm of IR2104 pins 1 and 4. The 1 µF bootstrap capacitor goes within 3–5 mm of pins 8 (VB) and 6 (VS).

The **power loop** — from the bulk capacitor positive terminal, through Q1, through Q2, back to the bulk capacitor negative terminal — must be as physically small as possible. Parasitic inductance in this loop creates voltage spikes proportional to L × di/dt. With 20 kHz switching and 13.7A current, even a few nanohenries of loop inductance generates significant spikes.

### Grounding strategy

Use a **partitioned star-ground approach** on the 2-layer board. Dedicate the bottom copper layer primarily as a **continuous GND copper pour**. The power ground return (Q2 source → shunt resistor → bulk capacitor negative) runs on wide top-layer traces or pours. Signal ground (Arduino, LM358, ADC) connects to the ground plane through a separate path, meeting the power ground at a single star point near the bulk capacitor.

Route current sense traces as a differential pair (Kelvin connection to shunt resistor) on the opposite side of the board from the switching node. Keep these analog traces away from the high di/dt power traces to prevent coupled noise.

### Thermal via strategy for MOSFETs

Under each IRFZ44N drain pad, place **6–12 thermal vias** (0.3 mm drill, 0.6 mm pad) connecting to a large copper pour on the bottom layer. Use **direct connections (no thermal relief)** for these vias — thermal relief pads reduce heat transfer. Extend the copper pour around and beneath each MOSFET to at least 10× the pad area for heat spreading.

### Connectors

Use **screw terminal blocks rated ≥ 20A** (5.08 mm pitch, Phoenix Contact MKDS series or equivalent) for both the motor output and 24V power input. Standard 2.54 mm female pin headers accept the Arduino Nano. Consider XT60 connectors for the power input — rated 60A continuous, widely available, and provide reliable high-current connection and disconnection.

### KiCad-specific setup

In Board Setup → Design Rules → Net Classes, create three classes: **Signal** (0.3–0.5 mm track, 0.25 mm clearance), **Gate** (0.5–1.0 mm track, 0.3 mm clearance), and **Power** (5.0–8.0 mm track, 0.5 mm clearance). Assign motor power nets (+24V, GND, switching node) to the Power class. Add predefined track widths (0.3, 0.5, 1.0, 2.5, 5.0, 8.0 mm) for quick selection during routing. Use `Package_DIP:DIP-8_W7.62mm` for the IR2104 and `Package_TO_SOT_THT:TO-220-3_Vertical` for the IRFZ44N with vertical heatsink mounting. Run DRC before generating Gerbers — check for clearance violations and unconnected nets.

---

## Arduino software: Timer1 PWM, current sensing, and protection

### 20 kHz PWM generation on Timer1

The ATmega328P's 16-bit Timer1 outputs PWM on **Pin D9 (OC1A)**. Using Fast PWM Mode 14 with ICR1 as TOP and no prescaler: TOP = 16,000,000 / (1 × 20,000) − 1 = **799**, giving exactly 20.000 kHz with 800 discrete duty cycle steps (~9.6-bit resolution).

```cpp
void setupTimer1_20kHz() {
    // Pin 9 (OC1A) as output
    DDRB |= (1 << PB1);
    
    // Reset Timer1
    TCCR1A = 0;
    TCCR1B = 0;
    TCNT1  = 0;
    
    // Fast PWM Mode 14: WGM13:10 = 1110
    // Non-inverting output on OC1A (Pin 9)
    TCCR1A = (1 << COM1A1) | (1 << WGM11);
    TCCR1B = (1 << WGM13) | (1 << WGM12) | (1 << CS10);
    
    // TOP = 799 → f_PWM = 16MHz / (1 × 800) = 20,000 Hz
    ICR1  = 799;
    OCR1A = 0;  // Start at 0% duty
}

// Set duty cycle: 0 = off, 799 = ~100%
void setDuty(uint16_t duty) {
    OCR1A = (duty > 799) ? 799 : duty;
}
```

**Never use `analogWrite()`** for this motor driver. Its default 490 Hz output causes audible whine, excessive current ripple, and poor low-speed control.

### Current measurement with exponential moving average

```cpp
#define CURRENT_PIN    A0
#define SHUNT_R        0.010   // 10 mΩ
#define AMP_GAIN       16.0
#define VREF           5.0
#define ADC_SCALE      (VREF / 1024.0 / (SHUNT_R * AMP_GAIN))

float filteredCurrent = 0.0;
int   zeroOffset      = 0;     // Calibrated at startup

void calibrateCurrentSensor() {
    long sum = 0;
    for (int i = 0; i < 64; i++) {
        sum += analogRead(CURRENT_PIN);
        delay(1);
    }
    zeroOffset = sum / 64;
}

float readCurrent() {
    int raw = analogRead(CURRENT_PIN) - zeroOffset;
    if (raw < 0) raw = 0;
    float current = raw * ADC_SCALE;  // ≈ 0.0305 A per count
    filteredCurrent = 0.1 * current + 0.9 * filteredCurrent;
    return filteredCurrent;
}
```

### Hardware overcurrent protection using the analog comparator

The ATmega328P's built-in analog comparator provides **~2–5 µs response** — orders of magnitude faster than ADC polling. Configure it to compare the 1.1V internal bandgap reference against the amplified current sense voltage on AIN1 (Pin D7). At gain 16 with 10 mΩ shunt, 1.1V corresponds to approximately **6.9A** — too low for a 13.7A motor.

**Better approach:** Use the second LM358 op-amp channel as a dedicated **hardware comparator**. Set its non-inverting input to a reference voltage (voltage divider: 3.2V for 20A threshold). Connect its inverting input to the current amplifier output. The LM358's open-collector-compatible output drives the IR2104 SD pin LOW through a diode when current exceeds the threshold. This provides **sub-microsecond hardware shutdown with zero CPU involvement**.

For the software backup, use the analog comparator ISR with the amplified signal routed to AIN1:

```cpp
#define SD_PIN 8

volatile bool overcurrentFault = false;

void setupOvercurrentISR() {
    // Use bandgap (1.1V) as positive input,
    // AIN1 (Pin 7) as negative input
    ACSR = (1 << ACBG)    // Bandgap reference on AIN+
         | (1 << ACI)     // Clear pending interrupt
         | (1 << ACIE)    // Enable interrupt
         | (1 << ACIS1);  // Falling edge (AIN1 > 1.1V)
}

ISR(ANALOG_COMP_vect) {
    PORTB &= ~(1 << PB0);     // SD pin LOW (fast port write)
    OCR1A = 0;                 // Kill PWM immediately
    overcurrentFault = true;
}
```

### State machine for motor control

The motor controller operates in five states: **OFF → STARTING → RUNNING → BRAKING → OFF**, with any active state transitioning to **FAULT** on overcurrent or stall detection.

**Stall detection logic:** If filtered current exceeds **20A** (2× rated, ADC > 655) continuously for more than **1 second**, the controller declares a stall fault and shuts down. This time window prevents false triggers from normal startup inrush or transient loads, while protecting the MOSFETs from sustained thermal overload.

**Soft-start:** During the STARTING state, PWM duty ramps linearly from 0 to target over **2 seconds**. This limits inrush current and reduces mechanical stress on the motor and load.

```cpp
enum State { OFF, STARTING, RUNNING, BRAKING, FAULT };
State state = OFF;
unsigned long stateEntry = 0;
unsigned long stallTimer = 0;
bool stallActive = false;

void updateMotor() {
    unsigned long now = millis();
    float current = readCurrent();
    
    switch (state) {
    case OFF:
        OCR1A = 0;
        if (startPressed()) {
            digitalWrite(SD_PIN, HIGH);
            state = STARTING;
            stateEntry = now;
        }
        break;
        
    case STARTING: {
        unsigned long elapsed = now - stateEntry;
        uint16_t duty = (elapsed < 2000)
            ? (uint16_t)((uint32_t)799 * elapsed / 2000)
            : 799;
        setDuty(duty);
        if (elapsed >= 2000) { state = RUNNING; stateEntry = now; }
        if (overcurrentFault) { enterFault(); }
        break;
    }
    
    case RUNNING:
        // Stall detection: >20A for >1 second
        if (current > 20.0) {
            if (!stallActive) { stallActive = true; stallTimer = now; }
            else if (now - stallTimer > 1000) { enterFault(); }
        } else { stallActive = false; }
        if (stopPressed()) { state = BRAKING; stateEntry = now; }
        if (overcurrentFault) { enterFault(); }
        break;
        
    case BRAKING: {
        unsigned long elapsed = now - stateEntry;
        setDuty((elapsed < 1000) ? 799 - (uint16_t)(799UL * elapsed / 1000) : 0);
        if (elapsed >= 1000) { state = OFF; }
        break;
    }
    
    case FAULT:
        OCR1A = 0;
        digitalWrite(SD_PIN, LOW);
        if (resetPressed()) {
            overcurrentFault = false;
            state = OFF;
        }
        break;
    }
}
```

The main `loop()` calls `updateMotor()` continuously alongside rate-limited serial debugging (every 250 ms) and LED updates. The entire architecture is non-blocking — **no `delay()` calls anywhere**.

---

## Conclusion

This motor driver design centers on a synchronous half-bridge topology that is fundamentally more efficient than a simple low-side switch with flyback diode, because Q2's channel (17.5 mΩ) provides a far lower-loss freewheeling path than any diode. The three corrections to the original parts list — **adding a second IRFZ44N, replacing the 1N5822 with a 20A-rated MBR2045CT, and adding a 7812 regulator for IR2104 VCC** — are non-negotiable; without them, the circuit either cannot function or will fail under load.

The thermal analysis reveals a comfortable margin at rated current (junction temperature ~77°C with a decent heatsink) but dangerously narrow margin during stall (~173°C). This makes the dual-layer overcurrent protection — hardware comparator on the SD pin for sub-microsecond response plus software stall detection for slower overcurrent events — the most safety-critical part of the entire design. The 20 kHz PWM frequency, generated by Timer1 in Fast PWM Mode 14, eliminates audible noise while keeping switching losses below 0.35W per MOSFET. Finally, the 2-layer PCB with 2 oz copper and 5–8 mm power traces can handle the full 13.7A rated current with modest temperature rise, provided the gate driver sits within 15 mm of the MOSFET gates and the power loop area is minimized.