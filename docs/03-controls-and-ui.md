# 3. Controls and UI Operation

## 3.1 Main Screen Layout

Header line shows:

- BPM value
- Swing value (`SW`)
- Transport state (`PLAY` or `STOP`)
- Optional mode marker (`[BPM]` or `[SHIFT]`)

Track rows:

1. KICK
2. SNARE
3. CH
4. OH
5. TONE
6. METAL

Footer line shows:

- Current playback step (`STEP`)
- Edit cursor step (`CUR`)
- Active voice count (`V`)

## 3.2 Encoder Controls (Main Screen)

- Rotate left/right (normal mode): select track
- Rotate left/right in SHIFT mode: move step cursor
- Short press: toggle step trigger at selected track + cursor
- Medium press:
  - Normal mode: toggle BPM edit mode
  - SHIFT mode: toggle mute for selected track
- Long press: open/close System Settings

## 3.3 KEY0 Controls (Main Screen)

- Short press: Play/Stop transport
- Medium press: toggle SHIFT mode
- Long press: increase swing by +2

Swing range: 0..45.

## 3.4 BPM Edit Mode

When `[BPM]` is shown:

- Encoder left: BPM -1
- Encoder right: BPM +1
- Encoder short press: exit BPM edit mode

BPM range: 40..260.

## 3.5 Mute Indication

When a track is muted, a `*` is appended to the track label on screen.
