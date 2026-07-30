/*
 * motor_driver_v2.ino
 * 24V / 250W Brushed DC Motor Driver — IR2104 Half-Bridge + IRFZ44N
 *
 * Team 1 — ELT10140 Electric Drive Systems
 * WHZ Westsächsische Hochschule Zwickau, SoSe 2026
 *
 * === PROJECT REQUIREMENT COMPLIANT ===
 * Main switch (latching) + 4 push buttons:
 *   ON / OFF / Speed Up / Speed Down
 *
 * PIN ASSIGNMENTS (Arduino Nano / ATmega328P)
 * ─────────────────────────────────────────────────────────────
 *  D9   PB1 / OC1A  → IR2104 IN  (20 kHz PWM output)
 *  D8   PB0         → IR2104 SD  (HIGH = driver enabled)
 *  A0   PC0 / ADC0  → LM358 output (amplified shunt voltage)
 *  D2   PD2         → ON button        (momentary, to GND)
 *  D3   PD3         → OFF button       (momentary, to GND)
 *  D4   PD4         → Speed Up button  (momentary, to GND)
 *  D5   PD5         → Speed Down button(momentary, to GND)
 *  D10  PB2         → Main Switch      (latching SWITCH, to GND)
 *  D6   PD6         → Green LED via 330Ω (motor running)
 *  D7   PD7         → Red LED   via 330Ω (fault indicator)
 *
 * TIMER1 FAST PWM MODE 14
 *   ICR1 = 799  →  f_PWM = 16 MHz / (1 × 800) = 20 000 Hz
 *   OCR1A range 0–799 (0–100 % duty cycle)
 *   Max duty clamped to 759 (95 %) so bootstrap cap recharges every cycle
 *
 * CURRENT SENSING
 *   Shunt R6 = 10 mΩ, LM358 gain = 16 (1 kΩ + 15 kΩ)
 *   V_out = I_motor × 0.010 × 16 = 0.16 × I_motor
 *   ADC count = I_motor × (0.16 / 5.0) × 1023 ≈ I_motor × 32.74
 *   20 A threshold → ADC ≈ 655
 *
 * STATE MACHINE
 *   OFF ──[ON btn + switch]──► STARTING ──[ramp done]──► RUNNING
 *    ▲                              │                        │
 *    │                              └─── overcurrent ────────┤
 *    │                                                       ▼
 *    └── BRAKING ◄──[OFF btn]────────────────────────── FAULT
 *
 * NOTE: This source file corresponds to the compiled HEX loaded in
 *       the Proteus VSM simulation (motor_driver.hex).
 *       To rebuild: Arduino IDE → Board: Arduino Nano (ATmega328P),
 *       Processor: ATmega328P (Old Bootloader), 16 MHz.
 */

// ═══════════════════ PIN DEFINITIONS ═══════════════════
#define PWM_PIN       9    // OC1A — Timer1 PWM → IR2104 IN
#define SD_PIN        8    // IR2104 SD (HIGH = enabled, LOW = shutdown)
#define CURRENT_PIN   A0   // LM358 output → ATmega ADC0

#define BTN_ON        2    // ON button        (active LOW, INPUT_PULLUP)
#define BTN_OFF       3    // OFF button       (active LOW, INPUT_PULLUP)
#define BTN_SPEED_UP  4    // Speed Up button  (active LOW, INPUT_PULLUP)
#define BTN_SPEED_DN  5    // Speed Down button(active LOW, INPUT_PULLUP)
#define MAIN_SWITCH   10   // Latching switch  (active LOW, INPUT_PULLUP)

#define LED_RUN       6    // Green LED — motor running
#define LED_FAULT     7    // Red LED   — fault state

// ═══════════════════ CONSTANTS ═══════════════════
#define PWM_TOP        799   // ICR1 TOP → exactly 20 kHz at 16 MHz / 1 prescaler
#define DUTY_MAX       759   // 95 % of 799 — bootstrap capacitor must recharge

#define NUM_SPEED_LEVELS  10
#define DEFAULT_LEVEL      5           // Start at level 5 = 50 % duty
#define SPEED_STEP        (PWM_TOP / NUM_SPEED_LEVELS)  // ≈ 80 counts per level

#define RAMP_TIME_MS   2000  // Soft-start: ramp from 0 to target over 2 s
#define BRAKE_TIME_MS  1000  // Braking:    ramp from target to 0 over 1 s

#define AMP_GAIN       16.0
#define SHUNT_OHM      0.010
#define VREF_V         5.0
#define ADC_BITS       1023
#define OC_THRESHOLD   655   // ADC count corresponding to 20 A overcurrent trip
#define STALL_MS       1000  // Stall = OC sustained for > 1 second

#define DEBOUNCE_MS    50

// ═══════════════════ STATE MACHINE ═══════════════════
enum MotorState { STATE_OFF, STATE_STARTING, STATE_RUNNING,
                  STATE_BRAKING, STATE_FAULT };
MotorState currentState = STATE_OFF;

// ═══════════════════ GLOBAL VARIABLES ═══════════════════
// Speed
int  speedLevel   = DEFAULT_LEVEL;
uint16_t targetDuty = DEFAULT_LEVEL * SPEED_STEP;  // ≈ 400 (50 %)

// State timing
unsigned long stateEntryTime = 0;

// Overcurrent / stall
unsigned long stallStartTime = 0;
bool          stallActive    = false;

// Current sensing
float filteredCurrent = 0.0;
int   zeroOffset      = 0;    // Calibrated at power-on

// LED blink (fault state)
unsigned long lastBlinkTime = 0;
bool          blinkState    = false;

// Button debounce — indexes: 0=ON, 1=OFF, 2=SpeedUp, 3=SpeedDn
#define NUM_BUTTONS 4
unsigned long debounceTime[NUM_BUTTONS] = {0, 0, 0, 0};
bool lastBtnState[NUM_BUTTONS]  = {HIGH, HIGH, HIGH, HIGH};
bool btnTriggered[NUM_BUTTONS]  = {false, false, false, false};

// ═══════════════════ TIMER1 SETUP ═══════════════════
void setupTimer1_20kHz() {
    DDRB  |= (1 << PB1);   // Pin 9 = output

    TCCR1A = 0;
    TCCR1B = 0;
    TCNT1  = 0;

    // Fast PWM Mode 14 (WGM13:WGM12:WGM11:WGM10 = 1110)
    // Non-inverting on OC1A, no prescaler (CS10 = 1)
    TCCR1A = (1 << COM1A1) | (1 << WGM11);
    TCCR1B = (1 << WGM13)  | (1 << WGM12) | (1 << CS10);

    ICR1  = PWM_TOP;
    OCR1A = 0;             // Start at 0 % duty
}

void setDuty(uint16_t duty) {
    if (duty > DUTY_MAX) duty = DUTY_MAX;
    OCR1A = duty;
}

// ═══════════════════ CURRENT SENSING ═══════════════════
void calibrateCurrentSensor() {
    long sum = 0;
    for (int i = 0; i < 64; i++) {
        sum += analogRead(CURRENT_PIN);
        delay(1);
    }
    zeroOffset = (int)(sum / 64);
}

float readCurrent() {
    int raw = analogRead(CURRENT_PIN) - zeroOffset;
    if (raw < 0) raw = 0;
    // V_adc = raw × (VREF / 1023)
    // I = V_adc / (AMP_GAIN × SHUNT_OHM)
    float current = (float)raw * (VREF_V / ADC_BITS) / (AMP_GAIN * SHUNT_OHM);
    // EMA filter — α = 0.1 (matches Simulink PT1 discretisation)
    filteredCurrent = 0.1f * current + 0.9f * filteredCurrent;
    return filteredCurrent;
}

// ═══════════════════ BUTTON DEBOUNCE ═══════════════════
// Fire exactly once per physical press; reset on release.
bool readButton(int index, int pin) {
    bool reading = digitalRead(pin);

    // Released — allow next press
    if (reading == HIGH) {
        btnTriggered[index] = false;
        lastBtnState[index] = HIGH;
        return false;
    }

    // Falling edge — start debounce timer
    if (lastBtnState[index] == HIGH && reading == LOW) {
        debounceTime[index] = millis();
        lastBtnState[index] = LOW;
    }

    // Debounce settled, not yet triggered
    if (!btnTriggered[index] &&
        (millis() - debounceTime[index]) > DEBOUNCE_MS) {
        btnTriggered[index] = true;
        return true;
    }

    return false;
}

// ═══════════════════ FAULT ENTRY ═══════════════════
void enterFault() {
    digitalWrite(SD_PIN, LOW);
    setDuty(0);
    currentState = STATE_FAULT;
    stateEntryTime = millis();
    stallActive = false;
    filteredCurrent = 0.0f;
    digitalWrite(LED_RUN, LOW);
}

// ═══════════════════ SETUP ═══════════════════
void setup() {
    // Output pins
    pinMode(SD_PIN,   OUTPUT);
    pinMode(LED_RUN,  OUTPUT);
    pinMode(LED_FAULT, OUTPUT);
    digitalWrite(SD_PIN,    LOW);
    digitalWrite(LED_RUN,   LOW);
    digitalWrite(LED_FAULT, LOW);

    // Input pins (active LOW, internal pull-up)
    pinMode(BTN_ON,       INPUT_PULLUP);
    pinMode(BTN_OFF,      INPUT_PULLUP);
    pinMode(BTN_SPEED_UP, INPUT_PULLUP);
    pinMode(BTN_SPEED_DN, INPUT_PULLUP);
    pinMode(MAIN_SWITCH,  INPUT_PULLUP);

    // ADC reference = AVCC (tied to +5 V on PCB via AREF pin)
    analogReference(DEFAULT);

    setupTimer1_20kHz();
    calibrateCurrentSensor();

    // Serial for debugging (optional — disable in release)
    // Serial.begin(115200);
}

// ═══════════════════ MAIN LOOP ═══════════════════
void loop() {
    unsigned long now = millis();

    // Read main switch and buttons
    bool switchOn    = (digitalRead(MAIN_SWITCH) == LOW);
    bool onPressed   = readButton(0, BTN_ON);
    bool offPressed  = readButton(1, BTN_OFF);
    bool upPressed   = readButton(2, BTN_SPEED_UP);
    bool downPressed = readButton(3, BTN_SPEED_DN);

    // ── Global: main switch OFF → force everything off ──
    if (!switchOn && currentState != STATE_OFF && currentState != STATE_FAULT) {
        setDuty(0);
        digitalWrite(SD_PIN, LOW);
        currentState = STATE_OFF;
    }

    // ── Overcurrent check (every state except OFF/FAULT) ──
    if (currentState == STATE_STARTING || currentState == STATE_RUNNING ||
        currentState == STATE_BRAKING) {
        float I = readCurrent();
        int rawADC = analogRead(CURRENT_PIN) - zeroOffset;
        if (rawADC < 0) rawADC = 0;

        if (rawADC > OC_THRESHOLD) {
            // Instantaneous overcurrent
            enterFault();
            return;
        }
        if (I > 20.0f) {
            // Stall detection: sustained overcurrent
            if (!stallActive) {
                stallActive = true;
                stallStartTime = now;
            } else if ((now - stallStartTime) >= STALL_MS) {
                enterFault();
                return;
            }
        } else {
            stallActive = false;
        }
    }

    // ── State machine ──
    switch (currentState) {

        // ─── OFF ───────────────────────────────────────────
        case STATE_OFF:
            setDuty(0);
            digitalWrite(SD_PIN, LOW);
            digitalWrite(LED_RUN,   LOW);
            digitalWrite(LED_FAULT, LOW);

            if (switchOn && onPressed) {
                targetDuty = speedLevel * SPEED_STEP;
                if (targetDuty > DUTY_MAX) targetDuty = DUTY_MAX;
                currentState   = STATE_STARTING;
                stateEntryTime = now;
                stallActive    = false;
                filteredCurrent = 0.0f;
                digitalWrite(SD_PIN, HIGH);   // Enable IR2104
            }
            break;

        // ─── STARTING (soft-start ramp) ────────────────────
        case STATE_STARTING: {
            unsigned long elapsed = now - stateEntryTime;
            uint16_t rampDuty;

            if (elapsed >= RAMP_TIME_MS) {
                rampDuty = targetDuty;
                setDuty(rampDuty);
                currentState = STATE_RUNNING;
            } else {
                // Linear ramp: 0 → targetDuty over RAMP_TIME_MS
                rampDuty = (uint16_t)((float)targetDuty * elapsed / RAMP_TIME_MS);
                setDuty(rampDuty);
            }
            digitalWrite(LED_RUN, HIGH);

            if (offPressed) {
                currentState   = STATE_BRAKING;
                stateEntryTime = now;
            }
            break;
        }

        // ─── RUNNING ───────────────────────────────────────
        case STATE_RUNNING:
            setDuty(targetDuty);
            digitalWrite(LED_RUN, HIGH);

            // Speed Up
            if (upPressed && speedLevel < NUM_SPEED_LEVELS) {
                speedLevel++;
                targetDuty = speedLevel * SPEED_STEP;
                if (targetDuty > DUTY_MAX) targetDuty = DUTY_MAX;
            }
            // Speed Down
            if (downPressed && speedLevel > 1) {
                speedLevel--;
                targetDuty = speedLevel * SPEED_STEP;
            }
            // Stop
            if (offPressed || !switchOn) {
                currentState   = STATE_BRAKING;
                stateEntryTime = now;
            }
            break;

        // ─── BRAKING (ramp down) ───────────────────────────
        case STATE_BRAKING: {
            unsigned long elapsed = now - stateEntryTime;
            uint16_t brakeDuty;

            if (elapsed >= BRAKE_TIME_MS) {
                setDuty(0);
                digitalWrite(SD_PIN, LOW);
                digitalWrite(LED_RUN, LOW);
                currentState = STATE_OFF;
            } else {
                // Linear ramp down from targetDuty to 0
                brakeDuty = (uint16_t)((float)targetDuty *
                            (1.0f - (float)elapsed / BRAKE_TIME_MS));
                setDuty(brakeDuty);
                digitalWrite(LED_RUN, HIGH);
            }
            break;
        }

        // ─── FAULT ─────────────────────────────────────────
        case STATE_FAULT:
            setDuty(0);
            digitalWrite(SD_PIN, LOW);
            digitalWrite(LED_RUN, LOW);

            // Blink red LED at 2 Hz
            if ((now - lastBlinkTime) >= 250) {
                lastBlinkTime = now;
                blinkState = !blinkState;
                digitalWrite(LED_FAULT, blinkState);
            }

            // Clear fault: cycle main switch (OFF then ON) then press ON
            if (!switchOn) {
                // Switch is off — waiting for it to be turned on again
                // Fault cleared only when switch toggled back
                ;
            } else if (onPressed) {
                digitalWrite(LED_FAULT, LOW);
                blinkState    = false;
                currentState  = STATE_OFF;
                filteredCurrent = 0.0f;
                stallActive   = false;
            }
            break;
    }
}
