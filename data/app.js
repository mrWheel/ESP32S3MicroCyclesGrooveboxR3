// SPA State Model
const state = {
  status: {},
  groups: [],
  activeGroup: "",
  patterns: [],
  activePatternIndex: 0,
  playingPatternIndex: 0,
  visiblePatternStartIndex: 0,
  selectedTrackIndex: 0,
  selectedStepGlobalIndex: 0,
  selectedStepLocalIndex: 0,
  selectedPatternIndex: 0,
  stepEditorOpen: false,
  stepEditorDraft: {},
  dirty: false,
};

// Track names
const trackNames = ["KICK", "SNARE", "CH", "OH", "TONE", "METAL"];

// Polling intervals
let statusInterval = null;
let playheadInterval = null;

// Initialize on page load
document.addEventListener("DOMContentLoaded", function() {
  initializeEventHandlers();
  updateStatus();
  updateGroups();
  updateSampleSets();

  // Start polling
  statusInterval = setInterval(updateStatus, 1000);
});

// ========== EVENT HANDLERS ==========

function initializeEventHandlers() {
  // Transport buttons
  document.getElementById("btnPlay").addEventListener("click", () => {
    fetch("/api/transport/play", { method: "POST" });
  });
  document.getElementById("btnStop").addEventListener("click", () => {
    fetch("/api/transport/stop", { method: "POST" });
  });
  document.getElementById("btnToggle").addEventListener("click", () => {
    fetch("/api/transport/toggle", { method: "POST" });
  });

  // BPM and Swing controls
  const sliderBpm = document.getElementById("sliderBpm");
  const inputBpm = document.getElementById("inputBpm");
  const sliderSwing = document.getElementById("sliderSwing");
  const inputSwing = document.getElementById("inputSwing");

  sliderBpm.addEventListener("input", function() {
    inputBpm.value = this.value;
    setBpm(parseInt(this.value));
  });
  inputBpm.addEventListener("change", function() {
    sliderBpm.value = this.value;
    setBpm(parseInt(this.value));
  });

  sliderSwing.addEventListener("input", function() {
    inputSwing.value = this.value;
    setSwing(parseInt(this.value));
  });
  inputSwing.addEventListener("change", function() {
    sliderSwing.value = this.value;
    setSwing(parseInt(this.value));
  });

  // Group buttons
  document.getElementById("btnSaveGroup").addEventListener("click", saveGroup);
  document.getElementById("btnLoadGroup").addEventListener("click", loadGroup);
  document.getElementById("btnNewGroup").addEventListener("click", newGroup);
  document.getElementById("btnRenameGroup").addEventListener("click", renameGroup);
  document.getElementById("btnCopyGroup").addEventListener("click", copyGroup);
  document.getElementById("btnDeleteGroup").addEventListener("click", deleteGroup);
  document.getElementById("btnCloseGroupList").addEventListener("click", hideGroupListWindow);

  // Sample set
  const selectSampleSet = document.getElementById("selectSampleSet");
  selectSampleSet.addEventListener("change", function() {
    // Load is triggered by button
  });
  document.getElementById("btnLoadSampleSet").addEventListener("click", () => {
    const setName = document.getElementById("selectSampleSet").value;
    if (setName) loadSampleSet(setName);
  });

  // Step editor
  document.getElementById("btnCloseStepEditor").addEventListener("click", closeStepEditor);
  document.getElementById("btnCloseStepEditorFooter").addEventListener("click", closeStepEditor);

  // Step editor value changes
  document.getElementById("stepTrigger").addEventListener("change", function() {
    state.stepEditorDraft.trigger = this.checked;
  });

  linkSliderAndInput("stepVelocity", "stepVelocityNum", (v) => {
    state.stepEditorDraft.velocity = parseInt(v);
  });
  linkSliderAndInput("stepProbability", "stepProbabilityNum", (v) => {
    state.stepEditorDraft.probability = parseInt(v);
  });

  document.getElementById("stepLockEnabled").addEventListener("change", function() {
    state.stepEditorDraft.lockEnabled = this.checked;
  });

  linkSliderAndInput("stepLockPitch", "stepLockPitchNum", (v) => {
    state.stepEditorDraft.lockPitch = parseInt(v);
  });
  linkSliderAndInput("stepLockDecay", "stepLockDecayNum", (v) => {
    state.stepEditorDraft.lockDecay = parseInt(v);
  });
}

function linkSliderAndInput(sliderId, inputId, onChangeCallback) {
  const slider = document.getElementById(sliderId);
  const input = document.getElementById(inputId);

  slider.addEventListener("input", function() {
    input.value = this.value;
    onChangeCallback(this.value);
  });
  input.addEventListener("change", function() {
    slider.value = this.value;
    onChangeCallback(this.value);
  });
}

// ========== API CALLS ==========

async function updateStatus() {
  try {
    const res = await fetch("/api/status");
    const data = await res.json();
    if (data.ok) {
      state.status = data;

      // Update header
      document.getElementById("version").textContent = data.version;
      document.getElementById("wifiStatus").textContent = data.wifiConnected ? "WiFi OK" : "WiFi --";
      document.getElementById("ipAddress").textContent = data.ip;
      document.getElementById("activeGroup").textContent = "Group: " + data.activeGroup;
      document.getElementById("activeSamples").textContent = "Samples: " + data.activeSampleSet;

      // Update dirty indicator
      if (data.patternGroupDirty) {
        document.getElementById("dirtyIndicator").style.display = "inline";
      } else {
        document.getElementById("dirtyIndicator").style.display = "none";
      }

      // Update transport UI
      updateTransportUI(data);

      // Start playhead polling if playing
      if (data.playing && !playheadInterval) {
        playheadInterval = setInterval(updatePlayhead, 250);
      } else if (!data.playing && playheadInterval) {
        clearInterval(playheadInterval);
        playheadInterval = null;
      }

      // Load patterns if group changed
      if (data.activeGroup !== state.activeGroup) {
        state.activeGroup = data.activeGroup;
        await updatePatterns();
      }
    }
  } catch (e) {
    console.error("Status update failed:", e);
  }
}

function updateTransportUI(data) {
  document.getElementById("sliderBpm").value = data.bpm;
  document.getElementById("inputBpm").value = data.bpm;
  document.getElementById("sliderSwing").value = data.swing;
  document.getElementById("inputSwing").value = data.swing;
}

async function updatePlayhead() {
  try {
    const res = await fetch("/api/sequencer/playhead");
    const data = await res.json();
    if (data.ok) {
      state.status.currentStep = data.currentStep;
      state.status.activePatternIndex = data.activePatternIndex;
      state.status.playingPatternIndex = data.playingPatternIndex;

      // Update playhead visual
      updateVisiblePatternWindow();
      renderGrid();
    }
  } catch (e) {
    console.error("Playhead update failed:", e);
  }
}

async function updateGroups() {
  try {
    const res = await fetch("/api/groups");
    const data = await res.json();

    if (data.ok) {
      state.groups = data.groups || [];
      return state.groups;
    }

    console.error("Groups update failed:", data.error);
    return [];
  } catch (e) {
    console.error("Groups update failed:", e);
    return [];
  }
}

async function updatePatterns() {
  try {
    const res = await fetch("/api/patterns");
    const data = await res.json();
    if (data.ok) {
      state.patterns = [];
      const patternNames = data.patterns || [];

      for (const patternInfo of patternNames) {
        const patRes = await fetch("/api/patterns/" + patternInfo.name);
        const patData = await patRes.json();
        if (patData.ok) {
          state.patterns.push(patData);
        }
      }

      state.activePatternIndex = data.activePatternIndex || 0;
      state.visiblePatternStartIndex = 0;
      renderGrid();
    }
  } catch (e) {
    console.error("Patterns update failed:", e);
  }
}

async function updateSampleSets() {
  try {
    const res = await fetch("/api/sample-sets");
    const data = await res.json();
    if (data.ok) {
      const select = document.getElementById("selectSampleSet");
      select.innerHTML = "";
      for (const setName of (data.sampleSets || [])) {
        const opt = document.createElement("option");
        opt.value = setName;
        opt.textContent = setName;
        if (setName === data.activeSampleSet) opt.selected = true;
        select.appendChild(opt);
      }
    }
  } catch (e) {
    console.error("Sample sets update failed:", e);
  }
}

// ========== TRANSPORT CONTROL ==========

async function setBpm(bpm) {
  try {
    await fetch("/api/transport/bpm", {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify({ bpm: bpm })
    });
  } catch (e) {
    console.error("Failed to set BPM:", e);
  }
}

async function setSwing(swing) {
  try {
    await fetch("/api/transport/swing", {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify({ swing: swing })
    });
  } catch (e) {
    console.error("Failed to set swing:", e);
  }
}

// ========== GROUP MANAGEMENT ==========

async function saveGroup() {
  try {
    const res = await fetch("/api/groups/save", { method: "POST" });
    const data = await res.json();
    if (data.ok) {
      alert("Group saved!");
      await updateStatus();
    } else {
      alert("Error: " + data.error);
    }
  } catch (e) {
    alert("Save failed: " + e);
  }
}

async function loadGroup() {
  const groups = await updateGroups();

  if (!groups || groups.length === 0) {
    alert("No pattern groups found on SD card");
    return;
  }

  showGroupListWindow(groups);
}


function showGroupListWindow(groups) {
  const panel = document.getElementById("groupListPanel");
  const list = document.getElementById("groupListItems");

  list.innerHTML = "";

  for (const groupName of groups) {
    const button = document.createElement("button");

    button.className = "btn btn-small group-list-button";
    button.textContent = groupName;

    button.addEventListener("click", async function() {
      await selectGroupToLoad(groupName);
    });

    list.appendChild(button);
  }

  panel.style.display = "block";
}

function hideGroupListWindow() {
  document.getElementById("groupListPanel").style.display = "none";
}

async function selectGroupToLoad(groupName) {
  try {
    const res = await fetch("/api/groups/load", {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify({ groupName: groupName })
    });

    const data = await res.json();

    if (!data.ok) {
      alert("Error: " + data.error);
      return;
    }

    hideGroupListWindow();

    state.activeGroup = groupName;
    state.visiblePatternStartIndex = 0;

    await updatePatterns();
    await updateStatus();
  } catch (e) {
    alert("Load failed: " + e);
  }
}

async function newGroup() {
  const groupName = prompt("Enter name for new group:");
  if (!groupName) return;

  try {
    const res = await fetch("/api/groups/new", {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify({ name: groupName })
    });
    const data = await res.json();
    if (data.ok) {
      await updateGroups();
      await updatePatterns();
      await updateStatus();
    } else {
      alert("Error: " + data.error);
    }
  } catch (e) {
    alert("New group failed: " + e);
  }
}

async function renameGroup() {
  const fromName = prompt("Current group name:");
  if (!fromName) return;
  const toName = prompt("New group name:");
  if (!toName) return;

  try {
    const res = await fetch("/api/groups/rename", {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify({ from: fromName, to: toName })
    });
    const data = await res.json();
    if (data.ok) {
      await updateGroups();
      if (state.activeGroup === fromName) {
        state.activeGroup = toName;
      }
    } else {
      alert("Error: " + data.error);
    }
  } catch (e) {
    alert("Rename failed: " + e);
  }
}

async function copyGroup() {
  const fromName = prompt("Group to copy:");
  if (!fromName) return;
  const toName = prompt("Copy as:");
  if (!toName) return;

  try {
    const res = await fetch("/api/groups/copy", {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify({ from: fromName, to: toName })
    });
    const data = await res.json();
    if (data.ok) {
      await updateGroups();
    } else {
      alert("Error: " + data.error);
    }
  } catch (e) {
    alert("Copy failed: " + e);
  }
}

async function deleteGroup() {
  const groupName = prompt("Group to delete:");
  if (!groupName) return;
  if (!confirm("Delete group '" + groupName + "'?")) return;

  try {
    const res = await fetch("/api/groups/delete", {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify({ name: groupName })
    });
    const data = await res.json();
    if (data.ok) {
      await updateGroups();
    } else {
      alert("Error: " + data.error);
    }
  } catch (e) {
    alert("Delete failed: " + e);
  }
}

// ========== SAMPLE SET MANAGEMENT ==========

async function loadSampleSet(setName) {
  try {
    const res = await fetch("/api/sample-sets/load", {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify({ name: setName })
    });
    const data = await res.json();
    if (data.ok) {
      alert("Sample set loaded!");
      await updateStatus();
    } else {
      alert("Error: " + data.error);
    }
  } catch (e) {
    alert("Load failed: " + e);
  }
}

// ========== PATTERN GRID RENDERING ==========

function updateVisiblePatternWindow() {
  if (state.patterns.length <= 1 || !state.status) {
    return;
  }

  const totalPatterns = state.patterns.length;
  const playingPatternIndex = state.status.playingPatternIndex || 0;
  const currentStep = state.status.currentStep || 0;
  const middlePatternIndex = (state.visiblePatternStartIndex + 1) % totalPatterns;

  if (playingPatternIndex === middlePatternIndex && currentStep === 15) {
    state.visiblePatternStartIndex = (state.visiblePatternStartIndex + 1) % totalPatterns;
  }
}

function renderGrid() {
  const tbody = document.getElementById("gridBody");
  tbody.innerHTML = "";

  if (state.patterns.length === 0) {
    tbody.innerHTML = '<tr><td colspan="50" style="text-align:center">No patterns loaded</td></tr>';
    return;
  }

  const totalPatterns = state.patterns.length;
  const visiblePatterns = [];

  // Collect 3 visible patterns
  for (let p = 0; p < 3; p++) {
    const patIdx = (state.visiblePatternStartIndex + p) % totalPatterns;
    if (patIdx < state.patterns.length) {
      visiblePatterns.push({ index: patIdx, data: state.patterns[patIdx] });
    }
  }

  // Header row with pattern names
  const headerRow = document.createElement("tr");
  const headerCell = document.createElement("td");
  headerCell.textContent = "Track";
  headerCell.className = "track-name-cell";
  headerRow.appendChild(headerCell);

  for (let p = 0; p < visiblePatterns.length; p++) {
    for (let s = 0; s < 16; s++) {
      const cell = document.createElement("td");
      cell.textContent = visiblePatterns[p].data ? visiblePatterns[p].data.name.substring(1) : "?";
      if (s === 0 && p > 0) cell.className = "pattern-separator";
      headerRow.appendChild(cell);
    }
  }
  tbody.appendChild(headerRow);

  // Data rows (one per track)
  for (let trackIdx = 0; trackIdx < 6; trackIdx++) {
    const row = document.createElement("tr");

    // Track name cell
    const trackCell = document.createElement("td");
    trackCell.textContent = trackNames[trackIdx];
    trackCell.className = "track-name-cell";
    row.appendChild(trackCell);

    // Steps for each visible pattern
    for (let p = 0; p < visiblePatterns.length; p++) {
      const patIdx = visiblePatterns[p].index;
      const pattern = visiblePatterns[p].data;

      for (let stepIdx = 0; stepIdx < 16; stepIdx++) {
        const cell = document.createElement("td");

        // Determine step state
        let stepText = "-";
        let classNames = [];

        if (pattern && pattern.tracks && pattern.tracks[trackIdx] && pattern.tracks[trackIdx].steps) {
          const step = pattern.tracks[trackIdx].steps[stepIdx];
          if (step.trigger) {
            stepText = step.mute ? "m" : "x";
            classNames.push(step.mute ? "step-muted" : "step-active");
          }
        }

        // Check if this is playhead
        const globalStepInWindow = p * 16 + stepIdx;
        if (state.status.playingPatternIndex === patIdx && state.status.currentStep === stepIdx) {
          classNames.push("step-playhead");
        }

        // Check if this is cursor
        if (state.selectedPatternIndex === patIdx && state.selectedStepLocalIndex === stepIdx &&
            state.selectedTrackIndex === trackIdx) {
          classNames.push("step-cursor");
        }

        cell.textContent = stepText;
        cell.className = classNames.join(" ");

        if (stepIdx === 0 && p > 0) {
          cell.className += " pattern-separator";
        }

        // Open Step editor when hovering over a step.
        cell.addEventListener("mouseenter", function() {
          selectStep(patIdx, trackIdx, stepIdx);
        });

        row.appendChild(cell);
      }
    }

    tbody.appendChild(row);
  }
}

function selectStep(patternIndex, trackIndex, stepIndex) {
  state.selectedPatternIndex = patternIndex;
  state.selectedTrackIndex = trackIndex;
  state.selectedStepLocalIndex = stepIndex;
  openStepEditor();
  renderGrid();
}

// ========== STEP EDITOR ==========

function openStepEditor() {
  const patIdx = state.selectedPatternIndex;
  const trackIdx = state.selectedTrackIndex;
  const stepIdx = state.selectedStepLocalIndex;

  if (patIdx >= state.patterns.length) return;

  const pattern = state.patterns[patIdx];
  const step = pattern.tracks[trackIdx].steps[stepIdx];

  // Initialize draft
  state.stepEditorDraft = {
    trigger: step.trigger,
    mute: step.mute,
    velocity: step.velocity,
    probability: step.probability,
    lockEnabled: step.lockEnabled,
    lockPitch: step.lockPitch,
    lockDecay: step.lockDecay
  };

  // Update UI
  document.getElementById("stepTrigger").checked = step.trigger;
  document.getElementById("stepVelocity").value = step.velocity;
  document.getElementById("stepVelocityNum").value = step.velocity;
  document.getElementById("stepProbability").value = step.probability;
  document.getElementById("stepProbabilityNum").value = step.probability;
  document.getElementById("stepLockEnabled").checked = step.lockEnabled;
  document.getElementById("stepLockPitch").value = step.lockPitch;
  document.getElementById("stepLockPitchNum").value = step.lockPitch;
  document.getElementById("stepLockDecay").value = step.lockDecay;
  document.getElementById("stepLockDecayNum").value = step.lockDecay;

  const title = "Step: " + pattern.name + " / " + trackNames[trackIdx] + " / " + (stepIdx + 1);
  document.getElementById("stepEditorTitle").textContent = title;

  state.stepEditorOpen = true;
  document.getElementById("stepEditor").style.display = "block";
}

async function closeStepEditor() {
  if (!state.stepEditorOpen) return;

  // Send changes if any
  const patIdx = state.selectedPatternIndex;
  const trackIdx = state.selectedTrackIndex;
  const stepIdx = state.selectedStepLocalIndex;

  if (patIdx < state.patterns.length) {
    const pattern = state.patterns[patIdx];
    const step = pattern.tracks[trackIdx].steps[stepIdx];

    // Check if draft differs from original
    const changed = (
      state.stepEditorDraft.trigger !== step.trigger ||
      state.stepEditorDraft.mute !== step.mute ||
      state.stepEditorDraft.velocity !== step.velocity ||
      state.stepEditorDraft.probability !== step.probability ||
      state.stepEditorDraft.lockEnabled !== step.lockEnabled ||
      state.stepEditorDraft.lockPitch !== step.lockPitch ||
      state.stepEditorDraft.lockDecay !== step.lockDecay
    );

    if (changed) {
      // Send update
      const patternName = pattern.name;
      try {
        const res = await fetch("/api/patterns/" + patternName + "/tracks/" + trackIdx + "/steps/" + stepIdx, {
          method: "PUT",
          headers: { "Content-Type": "application/json" },
          body: JSON.stringify(state.stepEditorDraft)
        });
        const data = await res.json();
        if (data.ok) {
          // Update local state
          step.trigger = state.stepEditorDraft.trigger;
          step.mute = state.stepEditorDraft.mute;
          step.velocity = state.stepEditorDraft.velocity;
          step.probability = state.stepEditorDraft.probability;
          step.lockEnabled = state.stepEditorDraft.lockEnabled;
          step.lockPitch = state.stepEditorDraft.lockPitch;
          step.lockDecay = state.stepEditorDraft.lockDecay;
          renderGrid();
        } else {
          alert("Error: " + data.error);
        }
      } catch (e) {
        alert("Update failed: " + e);
      }
    }
  }

  state.stepEditorOpen = false;
  document.getElementById("stepEditor").style.display = "none";
}

// ========== INITIALIZE ==========
console.log("Groovebox WebUI loaded");
