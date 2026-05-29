
## ESP32 MicroCycles Groovebox R3
## Developer Implementation Guide

This document defines the new storage architecture and menu behavior for the
ESP32 MicroCycles Groovebox R3.

This is a Developer implementation guide.

Important:

- Do NOT remove existing working functionality.
- Do NOT rewrite unrelated code.
- Do NOT invent a second popup/menu framework.
- Reuse the existing DisplayDriver/UI popup/list/input rendering logic.
- Keep audio and sequencer timing stable.
- Storage/menu actions are allowed to stop playback.
- System Settings actions are allowed to interrupt playback and force STOP.

---

# 1. Storage Overview

There are three storage locations:

1. NVS
2. LittleFS, called Local
3. SD card, called Card

Use these names consistently in UI and code:
```
NVS
Local
Card
```
---

# 2. NVS Storage

NVS stores only small persistent system state.

NVS must store:

- WiFi credentials
- active pattern group name
- active sample set name

Default active sample set:

S1

The active pattern group name is the name of the pattern group currently being edited.

The active sample set is the sample set loaded from Card:
```
/samples/S1
/samples/S2
...
/samples/S9
```
Sample set changes:

Preferred behavior:

- dynamically unload current samples
- load selected sample set
- keep system running if safe

Fallback allowed behavior:

- store selected sample set in NVS
- show message
- automatically reboot
- load selected sample set on next boot

Changing sample set may interrupt playback.

---

# 3. Local Storage Layout

Local storage means LittleFS.

Local stores:

- settings files
- active pattern group being edited
- active patterns being edited
- optionally cached active sample-set metadata, but NOT WAV sample files

Local root structure:
```
/
└── patterns
    ├── p01
    ├── p02
    ├── p03
    ├── ...
    └── p99
```
Important:

- There is no p00.
- Pattern numbering starts at p01.
- Local contains the currently active editable pattern group.
- Local p01..p99 are working copies.

---

# 4. Card Storage Layout

Card means SD card.

Card root structure:
```
/
├── patterns
│   ├── <Name>
│   │   ├── p01
│   │   ├── p02
│   │   ├── p03
│   │   ├── ...
│   │   └── p99
│   ├── <secondName>
│   │   ├── p01
│   │   ├── p02
│   │   ├── p03
│   │   ├── ...
│   │   └── p99
│   └── <lastName>
│       ├── p01
│       ├── p02
│       ├── p03
│       ├── ...
│       └── p99
└── samples
    ├── S1
    │   ├── sampleGainPercent.json
    │   ├── kick.wav
    │   ├── snare.wav
    │   ├── oh.wav
    │   ├── ch.wav
    │   ├── tone.wav
    │   └── metal.wav
    ├── S2
    │   ├── sampleGainPercent.json
    │   ├── kick.wav
    │   ├── snare.wav
    │   ├── oh.wav
    │   ├── ch.wav
    │   ├── tone.wav
    │   └── metal.wav
    ├── ...
    └── S9
    │   ├── sampleGainPercent.json
        ├── kick.wav
        ├── snare.wav
        ├── oh.wav
        ├── ch.wav
        ├── tone.wav
        └── metal.wav
```
Important:

- Pattern groups are stored under /patterns/<Name>/.
- Sample sets are stored under /samples/S1 through /samples/S9.
- Pattern group names and sample set names are different concepts.
- Do NOT confuse /patterns/<Name> with /samples/Sn.
- sampleGainPercent.json has this format:
```
{
  "sampleGainPercent": {
    "kick": 45,
    "snare": 100,
    "ch": 320,
    "oh": 260,
    "tone": 100,
    "metal": 100
  }
}
```
If the file does not exist create one with 100 for all items.

---

# 5. Naming Rules

## 5.1 Pattern Files

Use:
```
p01
p02
p03
...
p99
```
Rules:

- no p00
- lowercase p
- exactly two digits
- sequence always starts at p01

If JSON extension is already required by existing code, use:
```
p01.json
p02.json
...
p99.json
```
But the visible/user-facing name should remain:

p01

Developer must preserve compatibility with existing pattern parser.

---

## 5.2 Pattern Group Names

Pattern group names are stored on Card under:
```
/patterns/<Name>/
```
Rules:

- maximum 8 characters
- capital letters and numbers only
- no spaces
- no path separators
- invalid characters must be rejected or skipped during input

Examples:
```
JAZZ01
TECHNO1
GROOVE9
```
---

## 5.3 Sample Set Names

Sample set names are:
```
S1
S2
S3
...
S9
```
Stored on Card under:
```
/samples/S1
/samples/S2
...
/samples/S9
```
Not all sets have to excist.

NVS stores the active sample set name.

---

# 6. Boot Behavior

On boot:

1. Load WiFi/system settings from NVS.
2. Read active pattern group name from NVS.
3. Read active sample set name from NVS, defaulting to S1.
4. Mount Local.
5. Mount Card.
6. Load active sample set from Card:
   - /samples/<activeSampleSet>/kick.wav
   - /samples/<activeSampleSet>/snare.wav
   - /samples/<activeSampleSet>/oh.wav
   - /samples/<activeSampleSet>/ch.wav
   - /samples/<activeSampleSet>/tone.wav
   - /samples/<activeSampleSet>/metal.wav
7. Load active Local working patterns from:
   - /patterns/p01
   - /patterns/p02
   - etc.

If Card is not available:

- show a clear warning
- keep UI alive if possible
- do not crash

If the active sample set is missing:

- fall back to S1 if available
- otherwise use generated fallback samples
- log warning

---

# 7. System Settings Menu

System Settings must contain:

[System Menu]
```
SSID: <AP name>           (Read Only)
IP: <ip-address>          (Read Only)
MAC: <aa:bb:cc:dd:ee:ff>  (Read Only)
Local Storage   [***]
Card Storage    [***]
Erase WiFi credentials
Start WiFiManager
Set Theme <color>
Rotate Display <1>
Encoder Order (x-y)
Restart Groovebox
Exit
```
System Settings actions may interrupt playback.

Except menu items marked with [***] ("Local Strorage" and "Card Storage") are new. All other functionality should stay the same. 
Don't change anything on the existing menu items

Before executing any System Settings action that modifies storage or WiFi:

- stop playback
- set transport status to STOP
- prevent audio task from accessing state being changed

---

# 9. Local Storage Menu

Selecting:

Local Storage

opens:

[Local Storage]

Save Pattern
New Pattern
Delete Pattern
Edit Set Gain

---

## 9.1 New Pattern

Menu item:

Save Pattern

Behavior:

- Save Active Pattern to Local

---

Menu item:

New Pattern

Behavior:

- add a new Local pattern with the next available sequence number
- sequence starts at p01
- do not create p00
- if existing Local patterns are p01, p02, p03, create p04
- if gaps exist, Developer may either:
  - create next highest + 1
  - or compact first, depending on existing storage policy
- preferred: append next highest + 1

If p99 exists:

- show error message
- do not create more patterns

## 9.2 Delete Pattern

Menu item:

Delete Pattern

Open popup:

[DELETE PATTERN]

<list of Local patterns>

Input behavior:

- rotary encoder moves cursor
- short press EN_BTN selects pattern under cursor

After selecting pattern:

Show confirmation:

Are you sure?
Yes / No

If Yes:

1. Delete selected pattern from Local.
2. Rename remaining Local patterns so numbering is sequential again.
3. Sequence must run from p01 to pNN.
4. There must be no p00.
5. There must be no gaps after compaction.

Example:

Before delete:

p01
p02
p03
p04

Delete:

p02

After compaction:

p01
p02
p03

where old p03 became p02 and old p04 became p03.

If No:

- return to Local Storage menu
- do not modify files

---
## 9.3 Edit Set Gain Percent

Menu item:

Edit Set Gain

Open popup:

[SET GAIN]
```
 >kick:<    45 
  snare:   100
  ch:      320
  oh:      260
  tone:    100
  metal:   100
```

Input behavior:

- rotary encoder moves cursor from "kick" -> "snare" -> "ch" -> "oh" -> "tone" -> "metal" -> "kick"
- short press EN_BTN selects instrument under cursor and sets Value as input, rotary encoder changes Value.
- mid-press saves Value and moves cursor back to instrument.
- short-press KEY0 saves values 
   1. Save sampleGainPercent.json to Local.
   2. Use new gain values in audioEngine
   3. return to Local Storage menu

---

# 10. Card Storage Menu

Selecting:

Card Storage

opens:

[Card Storage]

Load Pattern
Save Pattern
Rename Pattern
Copy Pattern

---

# 11. Card Storage: Load Pattern

Menu item:

Load Pattern

Open popup:

[LOAD Pattern]

Step 1:

Ask whether to save current Local working pattern group first:

Save Local Pattern first?
Yes / No

If Yes:

- save current Local working pattern group to Card under active pattern group name from NVS

If No:

- continue without saving

Step 2:

Show list of Card pattern group names from:

/patterns/

User selects a pattern group.

Behavior after selecting:

1. Load selected Card pattern group.
2. Overwrite Local working patterns.
3. Write selected pattern group name to NVS.
4. Continue editing from Local.
5. Show confirmation message.

Important:

- Card pattern group is copied into Local working storage.
- Runtime editing always works on Local working patterns.
- Card is used for storage/backup/library.

---

# 12. Card Storage: Save Pattern

Menu item:

Save Pattern

Open popup:

[SAVE Pattern]

Save?
Yes / No

If Yes:

- save Local working pattern group to Card under active pattern group name from NVS
- overwrite existing Card pattern group with same name
- preserve all valid patterns p01..pNN

If No:

- return to Card Storage menu
- do not modify Card

---

# 13. Card Storage: Rename Pattern

Menu item:

Rename Pattern

Open popup:

[RENAME Pattern]

UI behavior:

- show old name from NVS
- input new name with 8 positions
- allowed characters:
  - A..Z
  - 0..9

Input behavior:

- rotary encoder changes current character
- short press EN_BTN accepts character under cursor and moves to next position
- mid/long press EN_BTN accepts newName
- mid/long press KEY0 cancels

On accept:

1. Rename Card directory:
```
/patterns/<oldName>
```
to:
```
/patterns/<newName>
```
2. Save newName in NVS.

Important correction:

- Rename pattern group directories under /patterns/.
- Do NOT rename /samples/<oldName>.
- Samples are separate and live under /samples/S1 through /samples/S9.

If target name already exists:

- show error message
- do not overwrite
- do not change NVS

If rename fails:

- show error message
- do not change NVS

---

# 14. Card Storage: Copy Pattern

Menu item:

Copy Pattern

Open popup:

[COPY Pattern]

UI behavior:

- show active pattern group name from NVS
- input newName with 8 characters
- allowed characters:
  - A..Z
  - 0..9

Input behavior:

- rotary encoder changes current character
- short press EN_BTN accepts character under cursor and moves to next position
- mid/long press EN_BTN accepts newName
- mid/long press KEY0 cancels

On accept:

1. Create new Card pattern group directory:
```
- trim <newName> (no spaces in name allowed)

/patterns/<newName>
```
2. Copy active Card pattern group:

/patterns/<activeName>/pNN

to:

/patterns/<newName>/pNN

3. Save newName to NVS.
4. Optionally copy new pattern group into Local so editing continues on the copied group.

Important correction:

- Copy pattern group directories under /patterns/.
- Do NOT create /samples/<newName>.
- Do NOT copy samples.

If target name already exists:

- show error message
- do not overwrite

If copy fails:

- show error message
- keep NVS unchanged unless copy completed successfully

---

# 15. Sample Set Selection

There are multiple sample sets:

/samples/S1
/samples/S2
...
/samples/S9

System should support selecting active sample set.

Preferred menu location:

[Card Storage]
```
Load Sample Set
```
Sample set selector behavior:

- list available sample sets S1..S9 found on Card
- only show directories that exist
- selecting sample set stores it in NVS

Preferred behavior:

1. Stop playback.
2. Unload current samples safely.
3. Load selected sample set.
4. Load sampleGainPercent.json
5. Update active sample set in NVS.
6. Return to STOP state.

Fallback allowed behavior:

1. Stop playback.
2. Save selected sample set to NVS.
3. Show:

Restarting..

4. Call:

esp.restart();

On next boot, selected sample set is loaded.

---

# 16. Groovebox Transport Behavior

In [Groovebox]:

## 16.1 Start

Short press KEY0 in STOP mode:

- starts music
- ALWAYS starts with p01
- sets status to RUN

Important:

- there is no p00 anymore
- p01 is the first pattern

---

## 16.2 Stop

Short press KEY0 in RUN mode:

- does NOT stop immediately
- ALWAYS continues the running pNN 
- stops only after completing the LAST pNN in current chain
- sets status to STOP after chain completion

Interpretation:

If active chain is:

p01 -> p02 -> p03 -> p04

and user presses KEY0 while RUN in the middle of p02:

- continue with p02 but at the end of p02:
- run p04 (last in chain)
- then stop

This gives musically clean stopping behavior.

---

# 17. Pattern Chain Rules

Pattern chaining uses Local working patterns:
```
/patterns/p01
/patterns/p02
...
```
Rules:

- chain starts at p01 when playback starts
- chain follows existing chain metadata
- stop request while RUN is deferred until running pattern finishes
- run last pattern in chain, then stop
- no p00 exists
- missing chained pattern must stop chain gracefully

Developer must ensure:

- chain playback does not access deleted/renamed patterns
- after delete/renumber operations, chain references remain valid or are safely reset
- storage operations that affect patterns force STOP before modifying files

---

# 18. Existing Popup/Menu Infrastructure

Important:

There is already DisplayDriver/UI infrastructure for rendering:

- list screens
- disabled list items
- text input
- field input
- numeric editor
- enum editor
- status/message overlays

Developer must inspect and reuse the existing UI infrastructure before adding new rendering code.

Known relevant files:

src/DisplayDriverClass.cpp
src/uiManager.cpp
include/DisplayDriverClass.h

Do NOT create a second unrelated popup framework.

Do NOT duplicate list navigation logic if existing helper functions can be extended.

Preferred approach:

- add new menu states in uiManager
- reuse DisplayDriver list/text/input rendering functions
- add small missing generic helpers only when necessary
- keep popup/status rendering visually consistent with existing UI

If existing DisplayDriver functions are insufficient:

- extend the existing DisplayDriver class
- keep naming consistent
- do not create separate standalone drawing systems

---

# 19. Implementation Modules

Suggested responsibility split:

## settingsStore

Responsible for:

- NVS active pattern group name
- NVS active sample set
- WiFi credentials integration
- Local settings files if already used

Suggested functions:

bool settingsStoreLoadActivePatternGroup(char *buffer, size_t bufferSize);
bool settingsStoreSaveActivePatternGroup(const char *name);

bool settingsStoreLoadActiveSampleSet(char *buffer, size_t bufferSize);
bool settingsStoreSaveActiveSampleSet(const char *sampleSetName);

---

## patternStorage

Create or refactor a storage layer responsible for:

- Local pattern operations
- Card pattern group operations
- copy Card group to Local
- save Local group to Card
- delete/compact Local patterns
- list Card groups
- rename Card group
- copy Card group

Suggested functions:

bool patternStorageListLocalPatterns(...);
bool patternStorageListCardGroups(...);

bool patternStorageCreateNextLocalPattern();
bool patternStorageDeleteLocalPatternAndCompact(uint8_t patternIndex);

bool patternStorageLoadCardGroupToLocal(const char *groupName);
bool patternStorageSaveLocalToCardGroup(const char *groupName);

bool patternStorageRenameCardGroup(const char *oldName, const char *newName);
bool patternStorageCopyCardGroup(const char *sourceName, const char *targetName);

---

## sampleManager

Responsible for:

- active sample set loading
- sample set validation
- sample set reload if dynamic reload is implemented

Suggested functions:

bool sampleManagerListSampleSets(...);
bool sampleManagerLoadSampleSet(const char *sampleSetName);
bool sampleManagerSetActiveSampleSet(const char *sampleSetName);

If dynamic reload is not safe:

bool sampleManagerStoreSampleSetAndRestart(const char *sampleSetName);

---

## uiManager

Responsible for:

- System Menu
- Local Storage menu
- Card Storage menu
- all popup navigation
- confirmation dialogs
- triggering storage operations
- forcing STOP before storage/WiFi actions

---

# 20. Safety Requirements

Storage operations must:

- never run in audio task
- never run while pattern files are being read by sequencer
- force STOP before changing Local or Card pattern data
- preserve unrelated JSON fields
- avoid deleting user data without confirmation

When destructive action is requested:

- show confirmation
- default to No if uncertain
- allow KEY0 cancel where practical

---

# 21. Acceptance Tests

## 21.1 Boot

Given NVS active sample set is S2:

- firmware loads /samples/S2/*.wav
- logs active sample set S2
- does not load /samples/S1 unless S2 is missing

---

## 21.2 Local New Pattern

Given Local contains:

p01
p02

New Pattern creates:

p03

---

## 21.3 Local Delete Pattern

Given Local contains:

p01
p02
p03
p04

Delete p02 results in:

p01
p02
p03

with old p03 renamed to p02 and old p04 renamed to p03.

---

## 21.4 Card Load Pattern

Given Card contains:

/patterns/JAZZ01/p01
/patterns/JAZZ01/p02

Load Pattern JAZZ01:

- optionally asks to save Local first
- copies JAZZ01 patterns to Local /patterns/
- writes JAZZ01 to NVS as active pattern group

---

## 21.5 Card Save Pattern

Given active NVS pattern group is:

JAZZ01

Save Pattern writes Local /patterns/pNN to:

/patterns/JAZZ01/pNN

---

## 21.6 Card Rename Pattern

Given active group:

JAZZ01

Rename to:

JAZZ02

renames:

/patterns/JAZZ01

to:

/patterns/JAZZ02

and writes JAZZ02 to NVS.

It must NOT touch /samples/.

---

## 21.7 Card Copy Pattern

Given active group:

JAZZ01

Copy to:

JAZZ03

creates:

/patterns/JAZZ03

and copies all pNN files from:

/patterns/JAZZ01

It must NOT touch /samples/.

---

## 21.8 WiFiManager

Start WiFiManager:

- stops playback
- shows connection instructions
- after portal closes shows Restarting..
- calls esp.restart()

---

## 21.9 Transport Start

KEY0 short in STOP:

- starts playback from p01
- sets status RUN

---

## 21.10 Transport Stop

KEY0 short in RUN:

- requests stop after current chain finishes
- does not stop immediately
- status becomes STOP only after final chained pattern finishes

---

# 22. Important Developer Warnings

Do not confuse these paths:

Correct pattern group path:

/patterns/<Name>/

Correct sample set path:

/samples/S1/

Rename Pattern and Copy Pattern operate on:

/patterns/<Name>/

They must never operate on:

/samples/<Name>/

Sample set selection operates on:

/samples/Sn/

It must never rename or copy pattern groups.

---

# 23. Final Goal

The final storage model must behave like this:

- NVS remembers what the user was working on
- Local is the fast editable working area
- Card is the library/archive area
- samples are grouped into selectable sample sets
- destructive operations require confirmation
- storage operations may stop playback
- WiFiManager may stop playback and reboot
- UI reuses existing popup/list/input infrastructure
