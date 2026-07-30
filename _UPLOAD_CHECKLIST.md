# Upload checklist — delete this file when you're done

## Step 0 — Remove the WHZ course PDFs first

These are the professors' material. They should not be public.

```powershell
cd C:\Users\drchi\Downloads\Electric-Drive-Systems
```

```powershell
git rm --cached docs/Practical_Course_ELT10140___Project_Hints.pdf
```

```powershell
git rm --cached docs/Practical_Course_ELT10140___Project_Requirements20260327.pdf
```

```powershell
git rm --cached docs/la_whz_26_03_19_ELT10140.pdf
```

```powershell
git rm --cached docs/la_whz_26_03_25_ELT10140_agenda.pdf
```

```powershell
git commit -m "Remove WHZ course material from repository"
```

Removing them from the current tree does **not** remove them from history. If you want
them gone entirely, the repo is small enough that the cleanest route is to delete it on
GitHub and re-push fresh. Otherwise install `git-filter-repo` and rewrite. Your call —
for a student project, removing from the tree is probably enough.

---

## Step 1 — Drop in the new top-level files

Copy `README.md`, `.gitignore`, `LICENSE` from this bundle into the repo root,
overwriting the existing README and .gitignore.

---

## Step 2 — Copy your real work in

| Repo folder | What goes in | Where it is on your machine |
|---|---|---|
| `firmware/` | `motor_driver_v2.ino`, any `.h`, the built `.hex` | wherever your Arduino sketchbook lives |
| `hardware/kicad/` | `.kicad_pro`, `.kicad_sch`, `.kicad_pcb`, `*.pretty/`, custom `.kicad_sym` | `C:\Users\drchi\Downloads\WHZ_DCMotor_Driver` |
| `hardware/gerbers/` | all Gerber layers + drill files (zip them too) | KiCad plot output folder |
| `hardware/datasheets/` | IR2104, IRFZ44N, MBR2045CT, LM358, LM2596, L7812CV, ATmega328P, MY1016 | your downloads |
| `simulation/proteus/` | `ELECTRIC DRIVE SYSTEM.pdsprj` → **rename to `motor_driver.pdsprj`** | `C:\Users\drchi\OneDrive\Documents\` |
| `simulation/simulink/` | `MotorDriverAveraged.slx` + any setup `.m` script | your MATLAB folder |
| `simulation/results/` | VSM oscilloscope captures, Simulink scope exports, ANSYS thermal contours | — |
| `design/` | the three existing `.md` files (already in repo) | already there |
| `report/` | final report PDF, `EDS_Präsentation_final.pdf` | Overleaf download |
| `bom/` | BOM as CSV **and** PDF | from the LaTeX longtable |
| `images/` | block diagram (already there), schematic PNG export, **photos of the finished board** | — |

**Rename anything with spaces or `+` to underscores before committing.** Same rule that
bit you in LaTeX bites you in Git URLs.

---

## Step 3 — Delete the placeholders

```powershell
Get-ChildItem -Recurse -Filter .gitkeep | Remove-Item
```

Only run this after you've actually put files in the folders — Git won't track an empty
directory, so any folder still empty will just vanish.

---

## Step 4 — Commit and push

```powershell
git add .
```

```powershell
git status
```

Read the status output before committing. Confirm no `slprj/`, no `*-backups/`, no
course PDFs.

```powershell
git commit -m "Add firmware, KiCad project, simulations, report and BOM; restructure repository"
```

```powershell
git push origin main
```

---

## Step 5 — Repo settings on GitHub

- **Description:** `24V 250W brushed DC motor driver — IR2104 half-bridge, ATmega328P closed-loop control, KiCad PCB, Proteus VSM + Simulink verification`
- **Topics:** `motor-control` `power-electronics` `kicad` `atmega328p` `mosfet` `pwm` `embedded-systems` `simulink` `pcb-design` `gate-driver`
- Turn off Issues and Wiki if you're not using them.

---

## The three things that make this a portfolio piece rather than a coursework dump

1. **Board photos.** One clear photo of the assembled wooden board at the top of the
   README does more than every paragraph of text under it. Right now there is no
   evidence the thing physically exists.

2. **A captured waveform.** Gate drive and switch node on the same timebase, with the
   dead time visible. That one image proves you understand what a half-bridge actually
   does — and it is exactly the kind of measurement a controls interviewer will ask you
   to interpret.

3. **A measured-vs-simulated plot.** Your Simulink step response overlaid on real bench
   data. That is system identification, and it is the single most transferable skill in
   this whole project toward vehicle dynamics work. If you have the bench data, make
   this plot. If you don't, say so in the README rather than leaving the gap silent.
