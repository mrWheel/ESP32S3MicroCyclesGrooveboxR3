# 6. Playback Behavior

## What To Expect

- Step timing is stable during playback.
- Screen remains responsive while audio is running.
- Start/stop actions are immediate.

## Audio Behavior

- Multiple drum hits can overlap.
- Final output level is controlled by software master gain.
- Stereo output carries the same mixed signal on left and right.
- Step lock decay can scale per-step trigger level.
- Step lock pitch is stored for future DSP phases.

## Pattern Chain Behavior

- Chain mode can cycle multiple internal pattern slots during playback.
- Chain length defines how many slots are cycled.
- Chain ON/OFF and chain length are editable from the CHAIN contextual page.

## Screen Behavior

- Main list updates continuously while playing.
- Footer values update quickly for step and cursor feedback.
- System Settings can be opened and closed without rebooting.
- Edit Track popup redraw is partial (popup area only) for smoother visual interaction.
