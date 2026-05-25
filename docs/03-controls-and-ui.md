# 3. Controls and Screen

## Main Screen

You see:

- Header with BPM, swing, transport state, and version
- Six track lines
- Footer with current step, cursor step, and voice activity

Tracks:

1. KICK
2. SNARE
3. CH
4. OH
5. TONE
6. METAL

## Encoder Actions

- Rotate: select track
- Rotate in EDIT mode:
  - TRIG page: move step cursor
  - VEL page: adjust velocity
  - PITCH page: adjust lock pitch
  - DECAY page: adjust lock decay
  - PROB page: adjust probability
  - MUTE page: change selected track
  - CHAIN page: adjust chain length
- Short press:
  - If not in edit mode: enter edit mode
  - If in edit mode:
    - TRIG page: toggle step on/off
    - PITCH/DECAY page: toggle lock on/off
    - MUTE page: toggle track mute
    - CHAIN page: toggle chain on/off
    - VEL/PROB page: quick trig toggle
- Medium press:
  - Normal mode: open Tempo Edit popup
  - EDIT mode: open Edit Track popup
- Long press:
  - Normal mode: open or close System Settings
  - EDIT mode: previous parameter page (when no popup is open)

## Tempo Edit Popup

- Open with encoder medium press
- Appears as an inline overlay on the Groovebox screen
- Rotate: change the selected value
- Short press: switch between BPM and SW
- Medium press: close popup
- KEY0 short/medium/long: close popup

## Edit Track Popup

- Open with encoder medium press while already in EDIT mode
- Popup entries are:
  1. VELOCITY
  2. PITCH
  3. DECAY
  4. PROBABILITY
  5. MUTE
  6. CHAIN
- TRIG is intentionally not in this popup (TRIG is already fast on the main screen)

### Edit Track Popup States

- Selection state:
  - Rotate: move between popup entries
  - Short press: enter value-edit state for selected entry
  - Medium press: keep popup open, leave value-edit state
  - Long press: close popup

- Value-edit state:
  - VELOCITY: rotate to change velocity
  - PITCH: rotate to change lock pitch
  - DECAY: rotate to change lock decay
  - PROBABILITY: rotate to change probability
  - MUTE: rotate to toggle ON/OFF
  - CHAIN: short press switches field focus between ON/OFF and Lx, rotate edits focused field
  - Medium press: return to selection state
  - Long press: close popup

### Popup Rendering Behavior

- While Edit Track popup is open, redraw is partial (popup area only)
- The full main screen is not redrawn on every encoder tick
- This keeps popup interaction visually calmer and reduces flicker

## KEY0 Actions

- Short press:
  - If in edit mode: exit edit mode
  - If not in edit mode: Play/Stop
- Medium press: open Tempo Edit popup (BPM selected)
- Long press: open Tempo Edit popup with SW selected

## Contextual Parameter Pages

The Groovebox cycles these live edit pages while staying on the main sequencer screen:

1. TRIG
2. VEL
3. PITCH
4. DECAY
5. PROB
6. MUTE
7. CHAIN

Note:

- These are the main-screen edit pages
- The Edit Track popup is a separate mechanism and excludes TRIG by design

## EDIT Visual Cue

In EDIT mode, the selected step character is highlighted for precise step editing.
