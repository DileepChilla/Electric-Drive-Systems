# Proteus BUTTON requires hold, not click

**The BUTTON component in Proteus is momentary — it only closes the circuit while you hold the left mouse button down, and springs open the instant you release.** This almost certainly explains why "subsequent clicks don't work": every quick click-and-release produces only a brief pulse identical to the first, and without proper pull-up resistors the ATmega328P pin floats at its previous voltage after release, making it appear stuck. The fix is either to hold the mouse button down for the full press duration, switch to a SWITCH component for toggle behavior, or add pull-up resistors to prevent floating pins.

## The BUTTON is momentary, not a toggle

The BUTTON component (SPST push button from the ACTIVE library) in Proteus ISIS simulates a real-world momentary tactile push button — like a doorbell. During VSM simulation, you must **left-click and hold** the mouse button down on the component to keep the circuit closed. The moment you release the mouse, the button springs back to its open (default) state. There is no toggle behavior whatsoever.

This is the critical misunderstanding. When a user performs a quick click-and-release, the button closes for only a tiny fraction of simulation time, then immediately reopens. The first click appears to "work" because the ATmega328P detects the brief LOW pulse. But subsequent clicks produce the exact same brief pulse — the button isn't failing to respond; it's responding identically each time with an extremely short closure that may be too brief for the firmware to detect, or the pin's floating state after release masks the change.

No special keyboard shortcut, spacebar press, or right-click action is needed. **Left-click and hold** is the only interaction method for momentary buttons. No "active component mode" or interaction mode toggle exists — active components respond to mouse input automatically during a running simulation.

## BUTTON vs SWITCH: two fundamentally different components

| Feature | BUTTON (Momentary) | SWITCH (Latching) |
|---|---|---|
| Interaction | Left-click **and hold** | Single left-click to **toggle** |
| Behavior | Closed only while held; opens on release | Stays in last position until clicked again |
| Real-world analog | Doorbell, tactile push button | Wall light switch, toggle switch |
| Best for | Brief pulse inputs, interrupt triggers | ON/OFF state control, mode selection |

If the goal is to click once to close the circuit and click again to open it, the correct component is a **SWITCH** (SPST, SPDT, etc. from the ACTIVE library), not a BUTTON. Alternatively, the **LOGICSTATE** interactive component provides a clean digital HIGH/LOW toggle that multiple experienced Proteus users recommend as more reliable for microcontroller input testing.

## Floating pins create the illusion of a stuck button

A second factor compounds the confusion. When the BUTTON opens (mouse released), the ATmega328P input pin becomes electrically disconnected. Without a pull-up or pull-down resistor, the pin **floats** — and Proteus models stray capacitance on floating nodes, causing the pin to retain its previous voltage. The visual wire-color indicator stays red (high/active) even though the button is open, making it look like the button is stuck in its pressed state.

The fix is straightforward. Add a **10kΩ pull-up resistor** from the MCU pin to VCC (for active-LOW buttons where the button pulls to GND), or configure `pinMode(buttonPin, INPUT_PULLUP)` in Arduino code. This ensures the pin returns cleanly to a defined HIGH state when the button is released, and subsequent press-and-hold actions will produce clear, detectable LOW transitions.

## CPU overload can swallow button clicks entirely

Even with correct interaction technique, Proteus may miss button clicks when the simulation is **not running in real time** due to excessive CPU load. This is the second most common cause of unresponsive interactive components. The status bar at the bottom of the Proteus window displays a warning when this occurs. Proteus is single-threaded, so multi-core processors don't help.

Several circuit design choices dramatically worsen CPU load and should be avoided:

- **External crystal oscillators wired on the schematic** create an extremely fast analog oscillation loop that bottlenecks everything. Set the MCU clock frequency in the component's properties dialog instead (right-click → Edit Properties), and either remove the crystal or mark it "Exclude from simulation."
- **Analog resistor models** instead of digital ones, particularly for I2C pull-ups and LED current limiters, add unnecessary computational overhead.
- **Animation features** like wire voltage coloring and current flow arrows consume significant CPU. Disable these under System → Set Animation Options if responsiveness is poor.
- **Reducing Frames Per Second** in animation options lowers display overhead and frees CPU cycles for processing input events.

## No Proteus 9 bug — but verify the ACTIVE library source

No specific Proteus 9 bug affecting BUTTON interaction was found in Labcenter's documentation, forums, or known issue databases. The behavior is consistent across Proteus versions and reflects the intentional momentary-action design of the component.

However, one critical verification step: the BUTTON **must** come from the **ACTIVE** library column in the Pick Devices dialog. Non-active versions of the same component exist for schematic and PCB design only — they look identical on the schematic but will not respond to any mouse interaction during simulation. Double-check this by opening the component's properties and confirming it references the ACTIVE library. Demo or evaluation versions of Proteus may also restrict interactive simulation functionality.

## Conclusion

The root cause is almost certainly a mismatch between expected toggle behavior and the BUTTON's actual momentary behavior. Three concrete actions resolve the issue: **hold the left mouse button down** for the full duration of each press instead of quick-clicking; **add a 10kΩ pull-up resistor** (or use `INPUT_PULLUP`) to prevent floating pins from masking state changes; and if toggle behavior is truly desired, **replace the BUTTON with a SWITCH or LOGICSTATE component**. If clicks still feel unresponsive after these changes, check the status bar for CPU overload warnings and reduce simulation overhead by removing crystal oscillators from the schematic, switching to digital component models, and disabling animation features.