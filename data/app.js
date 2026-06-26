// SPA State Model
const state = {
  status : {},
  groups : [],
  activeGroup : "",
  patterns : [],
  activePatternIndex : 0,
  playingPatternIndex : 0,
  visiblePatternStartIndex : 0,
  selectedTrackIndex : 0,
  selectedStepGlobalIndex : 0,
  selectedStepLocalIndex : 0,
  selectedPatternIndex : 0,
  stepEditorOpen : false,
  stepEditorDraft : {},
  dirty : false,
  actionPopupMode : "",
  actionPopupValue : "",
  selectedGroupForLoad : "",
  editStepsEnabled : true,
  busy : false,
  lastScrollPatternIndex : -1,
  lastScrollStepIndex : -1,
  lastPatternSyncMs : 0,
  visiblePatternMiddleIndex : 1,
  visiblePatternRightIndex : 2,
  visiblePatternWindowReady : false,
};

// Track names
const trackNames = [ "KICK", "SNARE", "CH", "OH", "TONE", "METAL" ];

// Polling intervals
let statusInterval = null;
let playheadInterval = null;

// Initialize on page load
document.addEventListener("DOMContentLoaded", async function() {
  initializeEventHandlers();

  await loadInitialFirmwareState();

  statusInterval = setInterval(updateGuiSync, 1000);
  playheadInterval = setInterval(updatePlayhead, 100);
});

async function loadInitialFirmwareState()
{
  await updateStatus();
  await updateGroups();
  await updateSampleSets();
  await updateActiveGroupFromFirmware();

  if (state.activeGroup && state.activeGroup !== "-")
  {
    await ensureActiveGroupIsLoaded();
    await updatePatterns();
  }

  renderGrid();

} // loadInitialFirmwareState()

async function updateActiveGroupFromFirmware()
{
  try
  {
    const res = await fetch("/api/groups/active");
    const data = await res.json();
    if (data.ok && data.name)
    {
      state.activeGroup = data.name;
      document.getElementById("activeGroup").textContent = "Group: " + data.name;
    }
  }
  catch (e)
  {
    hideBusy();
    console.error("Active group update failed:", e);
  }

} // updateActiveGroupFromFirmware()

async function ensureActiveGroupIsLoaded()
{
  try
  {
    const patternRes = await fetch("/api/patterns");
    const patternData = await patternRes.json();
    if (patternData.ok && patternData.patterns && patternData.patterns.length > 0)
    {
      return;
    }

    const loadRes = await fetch("/api/groups/load", {
      method : "POST",
      headers : {"Content-Type" : "application/json"},
      body : JSON.stringify({groupName : state.activeGroup})
    });
    const loadData = await loadRes.json();
    if (!loadData.ok)
    {
      console.error("Initial active group load failed:", loadData.error);
    }
  }
  catch (e)
  {
    console.error("Initial active group load failed:", e);
  }

} // ensureActiveGroupIsLoaded()

// ========== EVENT HANDLERS ==========

function initializeEventHandlers()
{
  // Transport buttons
  document.getElementById("btnPlay").addEventListener("click", function() {
    closeAllPopups();
    fetch("/api/transport/play", {method : "POST"});
  });

  document.getElementById("btnPause").addEventListener("click", async function() {
    closeAllPopups();

    if (state.status && state.status.paused)
    {
      await fetch("/api/transport/continue", {method : "POST"});
    }
    else
    {
      await fetch("/api/transport/pause", {method : "POST"});
    }

    await updateStatus();
    await updateTransport();
  });

  document.getElementById("btnStop").addEventListener("click", function() {
    closeAllPopups();
    fetch("/api/transport/stop", {method : "POST"});
  });

  document.getElementById("toggleEditSteps").addEventListener("change", function() {
    state.editStepsEnabled = this.checked;

    if (!state.editStepsEnabled) {
      closeStepEditor();
    }
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
  document.getElementById("btnCancelGroupList").addEventListener("click", hideGroupListWindow);
  document.getElementById("btnAcceptGroupList").addEventListener("click", acceptGroupListSelection);
  document.getElementById("btnActionCancel").addEventListener("click", hideActionPopup);
  document.getElementById("btnActionAccept").addEventListener("click", acceptActionPopup);

  // Sample set
  const selectSampleSet = document.getElementById("selectSampleSet");
  selectSampleSet.addEventListener("change", function() {
    // Load is triggered by button
  });
  document.getElementById("btnLoadSampleSet").addEventListener("click", () => {
    const setName = document.getElementById("selectSampleSet").value;
    if (setName)
      loadSampleSet(setName);
  });

  // Step editor
  document.getElementById("btnCloseStepEditor").addEventListener("click", closeStepEditor);
  document.getElementById("btnCloseStepEditorFooter").addEventListener("click", closeStepEditor);
  document.getElementById("stepEditor").addEventListener("mouseleave", closeStepEditor);

  // Step editor value changes
  document.getElementById("stepTrigger").addEventListener("change", function() {
    state.stepEditorDraft.trigger = this.checked;
  });

  document.getElementById("stepMute").addEventListener("change", function() {
    state.stepEditorDraft.mute = this.checked;
  });

  linkSliderAndInput("stepVelocity", "stepVelocityNum",
                     (v) => { state.stepEditorDraft.velocity = parseInt(v); });
  linkSliderAndInput("stepProbability", "stepProbabilityNum",
                     (v) => { state.stepEditorDraft.probability = parseInt(v); });

  document.getElementById("stepLockEnabled").addEventListener("change", function() {
    state.stepEditorDraft.lockEnabled = this.checked;
  });

  linkSliderAndInput("stepLockPitch", "stepLockPitchNum",
                     (v) => { state.stepEditorDraft.lockPitch = parseInt(v); });
  linkSliderAndInput("stepLockDecay", "stepLockDecayNum",
                     (v) => { state.stepEditorDraft.lockDecay = parseInt(v); });

} // initializeEventHandlers()

function linkSliderAndInput(sliderId, inputId, onChangeCallback)
{
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

} // linkSliderAndInput()

function closeAllPopups()
{
  hideGroupListWindow();
  hideActionPopup();

  if (state.stepEditorOpen)
  {
    closeStepEditor();
  }

} // closeAllPopups()

function showBusy(message)
{
  state.busy = true;
  document.getElementById("busyText").textContent = message;
  document.getElementById("busyOverlay").style.display = "flex";

} // showBusy()

function hideBusy()
{
  state.busy = false;
  document.getElementById("busyOverlay").style.display = "none";

} // hideBusy()

// ========== API CALLS ==========

async function updateStatus()
{
  try
  {
    const res = await fetch("/api/status");
    const data = await res.json();
    if (data.ok)
    {
      state.status = data;

      // Update header
      document.getElementById("version").textContent = data.version;
      document.getElementById("wifiStatus").textContent =
          data.wifiConnected ? "WiFi OK" : "WiFi --";
      document.getElementById("ipAddress").textContent = data.ip;
      document.getElementById("activeGroup").textContent = "Group: " + data.activeGroup;
      document.getElementById("activeSamples").textContent = "Samples: " + data.activeSampleSet;

      // Update dirty indicator
      if (data.patternGroupDirty)
      {
        document.getElementById("dirtyIndicator").style.display = "inline";
      }
      else
      {
        document.getElementById("dirtyIndicator").style.display = "none";
      }

      // Update transport UI
      updateTransportUI(data);
      if (data.playingPatternIndex !== undefined)
      {
        state.status.playingPatternIndex = data.playingPatternIndex;
        state.playingPatternIndex = data.playingPatternIndex;
      }

      if (data.currentStep !== undefined)
      {
        state.status.currentStep = data.currentStep;
      }

      // Load patterns if group changed
      if (data.activeGroup && data.activeGroup !== state.activeGroup)
      {
        state.activeGroup = data.activeGroup;
        state.visiblePatternStartIndex = 0;
        await updatePatterns();
      }
    }
  }
  catch (e)
  {
    hideBusy();
    console.error("Status update failed:", e);
  }

} //  updateStatus()

async function updateTransport()
{
  try
  {
    const res = await fetch("/api/transport");
    const data = await res.json();

    if (!data.ok)
    {
      return;
    }

    if (data.playing !== undefined)
    {
      state.status.playing = !!data.playing;
    }

    if (data.paused !== undefined)
    {
      state.status.paused = !!data.paused;
    }

    if (data.currentStep !== undefined)
    {
      state.status.currentStep = data.currentStep;
    }

    if (data.activePatternIndex !== undefined)
    {
      state.status.activePatternIndex = normalizePatternIndex(data.activePatternIndex);
    }

    if (data.playingPatternIndex !== undefined)
    {
      state.status.playingPatternIndex = normalizePatternIndex(data.playingPatternIndex);
      state.playingPatternIndex = state.status.playingPatternIndex;
    }

    updateTransportUI(data);
  }
  catch (e)
  {
    console.error("Transport update failed:", e);
  }

} // updateTransport()

async function updateGuiSync()
{
  await updateStatus();
  await updateTransport();

  const nowMs = Date.now();

if (!state.busy && !state.stepEditorOpen && !state.status.playing && 
     !state.status.paused &&
    (nowMs - state.lastPatternSyncMs) > 2500)
  {
    state.lastPatternSyncMs = nowMs;
    await updatePatterns();
  }

} // updateGuiSync()


function updateTransportUI(data)
{
  if (data.bpm !== undefined)
  {
    document.getElementById("sliderBpm").value = data.bpm;
    document.getElementById("inputBpm").value = data.bpm;
  }

  if (data.swing !== undefined)
  {
    document.getElementById("sliderSwing").value = data.swing;
    document.getElementById("inputSwing").value = data.swing;
  }

  if (data.paused !== undefined)
  {
    const pauseButton = document.getElementById("btnPause");

    if (data.paused)
    {
      pauseButton.textContent = "CONTINUE";
      pauseButton.title = "Continue";
    }
    else
    {
      pauseButton.textContent = "PAUSE";
      pauseButton.title = "Pause";
    }
  }

} // updateTransportUI()

function normalizePatternIndex(patternIndex)
{
  return patternIndex;

} // normalizePatternIndex()

async function updatePlayhead()
{
  try
  {
    const res = await fetch("/api/sequencer/playhead");
    const data = await res.json();

    if (data.ok)
    {
      const previousStep = state.status.currentStep;
      const previousPlayingPatternIndex = state.status.playingPatternIndex;
      const previousPaused = state.status.paused;

      state.status.currentStep = data.currentStep;
      state.status.activePatternIndex = normalizePatternIndex(data.activePatternIndex);
      state.status.playingPatternIndex = normalizePatternIndex(data.playingPatternIndex);

      if (data.playing !== undefined)
      {
        state.status.playing = !!data.playing;
      }

      if (data.paused !== undefined)
      {
        state.status.paused = !!data.paused;
      }

      state.playingPatternIndex = state.status.playingPatternIndex;

      updateTransportUI(state.status);

      if (state.status.paused)
      {
        return;
      }

      if (previousStep !== state.status.currentStep ||
          previousPlayingPatternIndex !== state.status.playingPatternIndex ||
          previousPaused !== state.status.paused)
      {
        updateVisiblePatternWindow();
        renderGrid();
      }
    }
  }
  catch (e)
  {
    console.error("Playhead update failed:", e);
  }

} // updatePlayhead()

async function updateGroups()
{
  try
  {
    const res = await fetch("/api/groups");
    const data = await res.json();

    if (data.ok)
    {
      state.groups = data.groups || [];

      if (data.activeGroup && (!state.activeGroup || state.activeGroup === "-"))
      {
        state.activeGroup = data.activeGroup;
        document.getElementById("activeGroup").textContent = "Group: " + data.activeGroup;
      }

      return state.groups;
    }

    console.error("Groups update failed:", data.error);
    return [];
  }
  catch (e)
  {
    console.error("Groups update failed:", e);
    return [];
  }

} // updateGroups()

async function updatePatterns()
{
  try
  {
    const res = await fetch("/api/patterns");
    const data = await res.json();
    if (data.ok)
    {
      state.patterns = [];
      const patternNames = data.patterns || [];

      for (const patternInfo of patternNames)
      {
        const patRes = await fetch("/api/patterns/" + patternInfo.name);
        const patData = await patRes.json();
        if (patData.ok)
        {
          state.patterns.push(patData);
        }
      }

      state.activePatternIndex = data.activePatternIndex || 0;
      state.visiblePatternWindowReady = false;
      if (!state.status || !state.status.playing)
      {
        state.visiblePatternStartIndex = 0;
      }

      renderGrid();
    }
  }
  catch (e)
  {
    hideBusy();
    console.error("Patterns update failed:", e);
  }
} // updatePatterns()

async function updateSampleSets()
{
  try
  {
    const res = await fetch("/api/sample-sets");
    const data = await res.json();
    if (data.ok)
    {
      const select = document.getElementById("selectSampleSet");
      select.innerHTML = "";
      for (const setName of (data.sampleSets || []))
      {
        const opt = document.createElement("option");
        opt.value = setName;
        opt.textContent = setName;
        if (setName === data.activeSampleSet)
          opt.selected = true;
        select.appendChild(opt);
      }
    }
  }
  catch (e)
  {
    console.error("Sample sets update failed:", e);
  }

} // updateSampleSets()

// ========== TRANSPORT CONTROL ==========

async function setBpm(bpm)
{
  try
  {
    await fetch("/api/transport/bpm", {
      method : "POST",
      headers : {"Content-Type" : "application/json"},
      body : JSON.stringify({bpm : bpm})
    });
  }
  catch (e)
  {
    console.error("Failed to set BPM:", e);
  }
} // setBpm()

async function setSwing(swing)
{
  try
  {
    await fetch("/api/transport/swing", {
      method : "POST",
      headers : {"Content-Type" : "application/json"},
      body : JSON.stringify({swing : swing})
    });
  }
  catch (e)
  {
    console.error("Failed to set swing:", e);
  }
} // setSwing()

// ========== GROUP MANAGEMENT ==========

async function saveGroup()
{
  closeAllPopups();
  showBusy("Saving group...");

  try
  {
    const res = await fetch("/api/groups/save", {method : "POST"});
    const data = await res.json();

    hideBusy();

    if (data.ok)
    {
      showActionMessage("Save Group", "Group saved");
      await updateStatus();
      return;
    }

    showActionMessage("Save Group", "Error: " + data.error);
  }
  catch (e)
  {
    hideBusy();
    showActionMessage("Save Group", "Save failed: " + e);
  }

} // saveGroup()

async function loadGroup()
{
  closeAllPopups();

  const groups = await updateGroups();

  if (!groups || groups.length === 0)
  {
    showActionMessage("Load Group", "No pattern groups found on SD card");
    return;
  }

  showGroupListWindow(groups);

} // loadGroup()

function showGroupListWindow(groups)
{
  const panel = document.getElementById("groupListPanel");
  const list = document.getElementById("groupListItems");
  const loadButton = document.getElementById("btnLoadGroup");
  const buttonRect = loadButton.getBoundingClientRect();
  const activeGroupName = state.activeGroup || (state.status ? state.status.activeGroup : "");

  state.selectedGroupForLoad = "";

  list.innerHTML = "";

  for (const groupName of groups)
  {
    const button = document.createElement("button");

    button.className = "btn btn-small group-list-button";

    if (groupName === activeGroupName)
    {
      button.textContent = "* " + groupName;
      button.classList.add("active-group-button");
    }
    else
    {
      button.textContent = "  " + groupName;
    }

    button.addEventListener("click", function() {
      state.selectedGroupForLoad = groupName;

      const allButtons = list.querySelectorAll("button");
      allButtons.forEach(function(item) {
        item.classList.remove("active-group-button");
      });

      button.classList.add("active-group-button");
    });

    list.appendChild(button);
  }

  panel.style.left = buttonRect.left + "px";
  panel.style.top = (buttonRect.bottom + 6) + "px";
  panel.style.display = "block";

} // showGroupListWindow()

function hideGroupListWindow()
{
  document.getElementById("groupListPanel").style.display = "none";
} // hideGroupListWindow()

async function acceptGroupListSelection()
{
  if (!state.selectedGroupForLoad)
  {
    showActionMessage("Load Group", "Select a group first");
    return;
  }

  await selectGroupToLoad(state.selectedGroupForLoad);

} // acceptGroupListSelection()

async function selectGroupToLoad(groupName)
{
  hideGroupListWindow();
  showBusy("Loading " + groupName + "...");

  try
  {
    const res = await fetch("/api/groups/load", {
      method : "POST",
      headers : {"Content-Type" : "application/json"},
      body : JSON.stringify({groupName : groupName})
    });

    const data = await res.json();

    if (!data.ok)
    {
      hideBusy();
      showActionMessage("Load Group", "Error: " + data.error);
      return;
    }

    state.activeGroup = groupName;
    state.visiblePatternStartIndex = 0;

    await updatePatterns();
    await updateStatus();
    await updateTransport();

    document.getElementById("activeGroup").textContent = "Group: " + groupName;

    hideBusy();
  }
  catch (e)
  {
    hideBusy();
    showActionMessage("Load Group", "Load failed: " + e);
  }

} // selectGroupToLoad()

async function newGroup()
{
  showGroupNameActionPopup("new", "New Group", "", "New group name:");

} // newGroup()

async function renameGroup()
{
  const activeName = state.activeGroup || (state.status ? state.status.activeGroup : "");

  if (!activeName || activeName === "-")
  {
    showActionMessage("Rename Group", "No active group");
    return;
  }

  showGroupNameActionPopup("rename", "Rename Group", activeName,
                           "New name for " + activeName + ":");

} // renameGroup()

async function copyGroup()
{
  const activeName = state.activeGroup || (state.status ? state.status.activeGroup : "");

  if (!activeName || activeName === "-")
  {
    showActionMessage("Copy Group", "No active group");
    return;
  }

  showGroupNameActionPopup("copy", "Copy Group", activeName, "Copy " + activeName + " as:");

} // copyGroup()

async function deleteGroup()
{
  const groups = await updateGroups();

  if (!groups || groups.length === 0)
  {
    showActionMessage("Delete Group", "No pattern groups found");
    return;
  }

  showDeleteGroupActionPopup(groups);

} // deleteGroup()

function normalizeGroupName(name)
{
  return String(name || "").trim().toUpperCase();

} // normalizeGroupName()

function showActionPopup(title)
{
  const popup = document.getElementById("actionPopup");
  const loadButton = document.getElementById("btnLoadGroup");
  const buttonRect = loadButton.getBoundingClientRect();

  document.getElementById("actionPopupTitle").textContent = title;

  popup.style.left = buttonRect.left + "px";
  popup.style.top = (buttonRect.bottom + 6) + "px";
  popup.style.display = "block";

} // showActionPopup()

function hideActionPopup()
{
  document.getElementById("actionPopup").style.display = "none";
  document.getElementById("actionPopupContent").innerHTML = "";
  state.actionPopupMode = "";
  state.actionPopupValue = "";

} // hideActionPopup()

function showActionMessage(title, message)
{
  const content = document.getElementById("actionPopupContent");

  state.actionPopupMode = "message";
  state.actionPopupValue = "";

  content.innerHTML = "";

  const row = document.createElement("div");
  row.className = "action-popup-row";
  row.textContent = message;
  content.appendChild(row);

  document.getElementById("btnActionAccept").style.display = "none";

  showActionPopup(title);

} // showActionMessage()

function showGroupNameActionPopup(mode, title, sourceName, labelText)
{
  const content = document.getElementById("actionPopupContent");

  state.actionPopupMode = mode;
  state.actionPopupValue = sourceName || "";

  content.innerHTML = "";

  if (sourceName)
  {
    const sourceRow = document.createElement("div");
    sourceRow.className = "action-popup-row";
    sourceRow.textContent = "Current group: " + sourceName;
    content.appendChild(sourceRow);
  }

  const inputRow = document.createElement("div");
  inputRow.className = "action-popup-row";

  const label = document.createElement("label");
  label.textContent = labelText;

  const input = document.createElement("input");
  input.id = "actionGroupNameInput";
  input.type = "text";
  input.value = "";
  input.autocomplete = "off";

  input.addEventListener("input", function() {
    this.value = normalizeGroupName(this.value);
  });

  inputRow.appendChild(label);
  inputRow.appendChild(input);
  content.appendChild(inputRow);

  document.getElementById("btnActionAccept").style.display = "inline-block";

  showActionPopup(title);

  input.focus();

} // showGroupNameActionPopup()

function showDeleteGroupActionPopup(groups)
{
  const content = document.getElementById("actionPopupContent");
  const activeName = state.activeGroup || (state.status ? state.status.activeGroup : "");

  state.actionPopupMode = "delete";
  state.actionPopupValue = "";

  content.innerHTML = "";

  const list = document.createElement("div");
  list.className = "delete-group-list";

  for (const groupName of groups)
  {
    const button = document.createElement("button");

    button.className = "btn btn-small delete-group-button";

    if (groupName === activeName)
    {
      button.textContent = "* " + groupName + " (active, cannot delete)";
      button.disabled = true;
      button.classList.add("delete-group-button-disabled");
    }
    else
    {
      button.textContent = "  " + groupName;
      button.addEventListener("click", function() {
        state.actionPopupValue = groupName;

        const allButtons = list.querySelectorAll("button");
        allButtons.forEach(function(item) {
          item.classList.remove("active-group-button");
        });

        button.classList.add("active-group-button");
      });
    }

    list.appendChild(button);
  }

  content.appendChild(list);

  document.getElementById("btnActionAccept").style.display = "inline-block";

  showActionPopup("Delete Group");

} // showDeleteGroupActionPopup()

async function acceptActionPopup()
{
  if (state.actionPopupMode === "message")
  {
    hideActionPopup();
    return;
  }

  if (state.actionPopupMode === "new")
  {
    await acceptNewGroupAction();
  }
  else if (state.actionPopupMode === "rename")
  {
    await acceptRenameGroupAction();
  }
  else if (state.actionPopupMode === "copy")
  {
    await acceptCopyGroupAction();
  }
  else if (state.actionPopupMode === "delete")
  {
    await acceptDeleteGroupAction();
  }

} // acceptActionPopup()

async function acceptNewGroupAction()
{
  const groupName = normalizeGroupName(document.getElementById("actionGroupNameInput").value);

  if (!groupName)
  {
    showActionMessage("New Group", "Enter a group name");
    return;
  }

  hideActionPopup();
  showBusy("Creating " + groupName + "...");

  try
  {
    const res = await fetch("/api/groups/new", {
      method : "POST",
      headers : {"Content-Type" : "application/json"},
      body : JSON.stringify({name : groupName})
    });

    const data = await res.json();

    if (!data.ok)
    {
      hideBusy();
      showActionMessage("New Group", "Error: " + data.error);
      return;
    }

    showBusy("Load " + groupName + "...");

    state.activeGroup = groupName;
    state.visiblePatternStartIndex = 0;

    await updateGroups();
    await updatePatterns();
    await updateStatus();

    document.getElementById("activeGroup").textContent = "Group: " + groupName;

    hideBusy();
  }
  catch (e)
  {
    hideBusy();
    showActionMessage("New Group", "Failed: " + e);
  }

} // acceptNewGroupAction()

async function acceptRenameGroupAction()
{
  const fromName = state.actionPopupValue;
  const toName = normalizeGroupName(document.getElementById("actionGroupNameInput").value);

  if (!toName)
  {
    showActionMessage("Rename Group", "Enter a new group name");
    return;
  }

  try
  {
    hideActionPopup();
    showBusy("Renaming " + fromName + "...");
    const res = await fetch("/api/groups/rename", {
      method : "POST",
      headers : {"Content-Type" : "application/json"},
      body : JSON.stringify({from : fromName, to : toName})
    });

    const data = await res.json();

    if (!data.ok)
    {
      showActionMessage("Rename Group", "Error: " + data.error);
      return;
    }

    hideActionPopup();

    if (state.activeGroup === fromName) 
    {
      state.activeGroup = toName;
      document.getElementById("activeGroup").textContent = "Group: " + toName;
    }

    await updateGroups();
    await updatePatterns();
    await updateStatus();
  }
  catch (e)
  {
    hideBusy();
    showActionMessage("Rename Group", "Failed: " + e);
  }

} // acceptRenameGroupAction()

async function acceptCopyGroupAction()
{
  const fromName = state.actionPopupValue;
  const toName = normalizeGroupName(document.getElementById("actionGroupNameInput").value);

  if (!toName)
  {
    showActionMessage("Copy Group", "Enter a new group name");
    return;
  }

  try
  {
    hideActionPopup();
    showBusy("Copying " + fromName + "...");
    const res = await fetch("/api/groups/copy", {
      method : "POST",
      headers : {"Content-Type" : "application/json"},
      body : JSON.stringify({from : fromName, to : toName})
    });

    const data = await res.json();

    if (!data.ok)
    {
      showActionMessage("Copy Group", "Error: " + data.error);
      return;
    }

    hideActionPopup();

    await updateGroups();
    hideBusy();
  }
  catch (e)
  {
    hideBusy();
    showActionMessage("Copy Group", "Failed: " + e);
  }

} // acceptCopyGroupAction()

async function acceptDeleteGroupAction()
{
  const groupName = state.actionPopupValue;

  if (!groupName)
  {
    showActionMessage("Delete Group", "Select a group to delete");
    return;
  }

  hideActionPopup();
  showBusy("Deleting " + groupName + "...");

  try
  {
    const res = await fetch("/api/groups/delete", {
      method : "POST",
      headers : {"Content-Type" : "application/json"},
      body : JSON.stringify({name : groupName})
    });

    const data = await res.json();

    if (!data.ok)
    {
      hideBusy();
      showActionMessage("Delete Group", "Error: " + data.error);
      return;
    }

    await updateGroups();
    await updateStatus();

    hideBusy();
  }
  catch (e)
  {
    hideBusy();
    showActionMessage("Delete Group", "Failed: " + e);
  }

} // acceptDeleteGroupAction()

// ========== SAMPLE SET MANAGEMENT ==========

async function loadSampleSet(setName)
{
  try
  {
    const res = await fetch("/api/sample-sets/load", {
      method : "POST",
      headers : {"Content-Type" : "application/json"},
      body : JSON.stringify({name : setName})
    });
    const data = await res.json();
    if (data.ok)
    {
      alert("Sample set loaded!");
      await updateStatus();
    }
    else
    {
      alert("Error: " + data.error);
    }
  }
  catch (e)
  {
    hideBusy();
    alert("Load failed: " + e);
  }

} // loadSampleSet()

// ========== PATTERN GRID RENDERING ==========

function getNextPatternIndex(patternIndex)
{
  if (state.patterns.length === 0 || patternIndex < 0 || patternIndex >= state.patterns.length)
  {
    return 0;
  }

  const pattern = state.patterns[patternIndex];

  if (pattern.chainEnabled && pattern.chainTarget && pattern.chainTarget.length > 0)
  {
    for (let index = 0; index < state.patterns.length; index++)
    {
      if (state.patterns[index].name === pattern.chainTarget)
      {
        return index;
      }
    }
  }

  return (patternIndex + 1) % state.patterns.length;

} // getNextPatternIndex()

function updateVisiblePatternWindow()
{
  if (state.patterns.length <= 1 || !state.status)
  {
    return;
  }

  const playingPatternIndex = state.status.playingPatternIndex || 0;

  if (!state.visiblePatternWindowReady)
  {
    state.visiblePatternStartIndex = playingPatternIndex;
    state.visiblePatternMiddleIndex = getNextPatternIndex(state.visiblePatternStartIndex);
    state.visiblePatternRightIndex = getNextPatternIndex(state.visiblePatternMiddleIndex);
    state.visiblePatternWindowReady = true;
    return;
  }

  // Shift the visible window only after two full patterns.
  // Example: p01|p02|p03 -> p03|p05|p04.
  if (playingPatternIndex === state.visiblePatternRightIndex)
  {
    state.visiblePatternStartIndex = state.visiblePatternRightIndex;
    state.visiblePatternMiddleIndex = getNextPatternIndex(state.visiblePatternStartIndex);
    state.visiblePatternRightIndex = getNextPatternIndex(state.visiblePatternMiddleIndex);
  }

} // updateVisiblePatternWindow()

function renderGrid()
{
  const tbody = document.getElementById("gridBody");
  tbody.innerHTML = "";

  if (state.patterns.length === 0)
  {
    tbody.innerHTML = '<tr><td colspan="50" style="text-align:center">No patterns loaded</td></tr>';
    return;
  }

  const visiblePatterns = [];

  visiblePatterns.push({
    index : state.visiblePatternStartIndex,
    data : state.patterns[state.visiblePatternStartIndex]
  });

  visiblePatterns.push({
    index : state.visiblePatternMiddleIndex,
    data : state.patterns[state.visiblePatternMiddleIndex]
  });

  visiblePatterns.push({
    index : state.visiblePatternRightIndex,
    data : state.patterns[state.visiblePatternRightIndex]
  });

  const headerRow = document.createElement("tr");
  const headerCell = document.createElement("td");

  headerCell.textContent = "Track";
  headerCell.className = "track-name-cell";
  headerRow.appendChild(headerCell);

  for (let p = 0; p < visiblePatterns.length; p++)
  {
    const patIdx = visiblePatterns[p].index;

    for (let s = 0; s < 16; s++)
    {
      const cell = document.createElement("td");
      let classNames = [];

      cell.textContent = visiblePatterns[p].data ? visiblePatterns[p].data.name.substring(1) : "?";

      if (state.status.playingPatternIndex === patIdx && state.status.currentStep === s)
      {
        classNames.push("step-header-playhead");
      }

      if (s === 0 && p > 0)
      {
        classNames.push("pattern-separator");
      }

      cell.className = classNames.join(" ");
      headerRow.appendChild(cell);
    }
  }

  tbody.appendChild(headerRow);

  for (let trackIdx = 0; trackIdx < 6; trackIdx++)
  {
    const row = document.createElement("tr");

    const trackCell = document.createElement("td");

    trackCell.textContent = trackNames[trackIdx];
    trackCell.className = "track-name-cell";
    row.appendChild(trackCell);

    for (let p = 0; p < visiblePatterns.length; p++)
    {
      const patIdx = visiblePatterns[p].index;
      const pattern = visiblePatterns[p].data;

      for (let stepIdx = 0; stepIdx < 16; stepIdx++)
      {
        const cell = document.createElement("td");

        let stepText = "";
        let classNames = [];

        if (pattern && pattern.tracks && pattern.tracks[trackIdx] && pattern.tracks[trackIdx].steps)
        {
          const step = pattern.tracks[trackIdx].steps[stepIdx];

          if (step.trigger)
          {
            stepText = step.mute ? "🔇" : "●";
            classNames.push(step.mute ? "step-muted" : "step-active");
          }
        }

        if (state.selectedPatternIndex === patIdx && state.selectedStepLocalIndex === stepIdx &&
            state.selectedTrackIndex === trackIdx)
        {
          classNames.push("step-cursor");
        }

        cell.textContent = stepText;
        cell.className = classNames.join(" ");

        if (stepIdx === 0 && p > 0)
        {
          cell.className += " pattern-separator";
        }

        cell.addEventListener("click", function() {
          if (state.editStepsEnabled)
          {
            selectStep(patIdx, trackIdx, stepIdx, cell);
          }
          else
          {
            toggleStepTrigger(patIdx, trackIdx, stepIdx);
          }
        });

        row.appendChild(cell);
      }
    }

    tbody.appendChild(row);
  }

} // renderGrid()

function selectStep(patternIndex, trackIndex, stepIndex, anchorCell)
{
  state.selectedPatternIndex = patternIndex;
  state.selectedTrackIndex = trackIndex;
  state.selectedStepLocalIndex = stepIndex;

  openStepEditor(anchorCell);
  renderGrid();

} // selectStep()

async function toggleStepTrigger(patternIndex, trackIndex, stepIndex)
{
  if (patternIndex >= state.patterns.length)
  {
    return;
  }

  const pattern = state.patterns[patternIndex];

  if (!pattern.tracks || !pattern.tracks[trackIndex] || !pattern.tracks[trackIndex].steps)
  {
    return;
  }

  const step = pattern.tracks[trackIndex].steps[stepIndex];

  const updatedStep = {
    trigger : !step.trigger,
    mute : step.mute,
    velocity : step.velocity,
    probability : step.probability,
    lockEnabled : step.lockEnabled,
    lockPitch : step.lockPitch,
    lockDecay : step.lockDecay
  };

  try
  {
    const res = await fetch(
        "/api/patterns/" + pattern.name + "/tracks/" + trackIndex + "/steps/" + stepIndex, {
          method : "PUT",
          headers : {"Content-Type" : "application/json"},
          body : JSON.stringify(updatedStep)
        });

    const data = await res.json();

    if (data.ok)
    {
      step.trigger = updatedStep.trigger;
      renderGrid();
    }
    else
    {
      alert("Error: " + data.error);
    }
  }
  catch (e)
  {
    hideBusy();
    alert("Update failed: " + e);
  }

} // toggleStepTrigger()

// ========== STEP EDITOR ==========

function openStepEditor(anchorCell)
{
  const patIdx = state.selectedPatternIndex;
  const trackIdx = state.selectedTrackIndex;
  const stepIdx = state.selectedStepLocalIndex;

  if (patIdx >= state.patterns.length)
  {
    return;
  }

  const pattern = state.patterns[patIdx];
  const step = pattern.tracks[trackIdx].steps[stepIdx];

  state.stepEditorDraft = {
    trigger : step.trigger,
    mute : step.mute,
    velocity : step.velocity,
    probability : step.probability,
    lockEnabled : step.lockEnabled,
    lockPitch : step.lockPitch,
    lockDecay : step.lockDecay
  };

  document.getElementById("stepTrigger").checked = step.trigger;
  document.getElementById("stepMute").checked = step.mute;
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

  const editor = document.getElementById("stepEditor");
  editor.style.display = "block";

  if (anchorCell)
  {
    const rect = anchorCell.getBoundingClientRect();

    editor.style.left = rect.left + "px";
    editor.style.top = rect.top + "px";
  }
} // openStepEditor()

async function closeStepEditor()
{
  if (!state.stepEditorOpen)
    return;

  // Send changes if any
  const patIdx = state.selectedPatternIndex;
  const trackIdx = state.selectedTrackIndex;
  const stepIdx = state.selectedStepLocalIndex;

  if (patIdx < state.patterns.length)
  {
    const pattern = state.patterns[patIdx];
    const step = pattern.tracks[trackIdx].steps[stepIdx];

    // Check if draft differs from original
    const changed = (state.stepEditorDraft.trigger !== step.trigger ||
                     state.stepEditorDraft.mute !== step.mute ||
                     state.stepEditorDraft.velocity !== step.velocity ||
                     state.stepEditorDraft.probability !== step.probability ||
                     state.stepEditorDraft.lockEnabled !== step.lockEnabled ||
                     state.stepEditorDraft.lockPitch !== step.lockPitch ||
                     state.stepEditorDraft.lockDecay !== step.lockDecay);

    if (changed)
    {
      // Send update
      const patternName = pattern.name;
      try
      {
        const res = await fetch(
            "/api/patterns/" + patternName + "/tracks/" + trackIdx + "/steps/" + stepIdx, {
              method : "PUT",
              headers : {"Content-Type" : "application/json"},
              body : JSON.stringify(state.stepEditorDraft)
            });
        const data = await res.json();
        if (data.ok)
        {
          // Update local state
          step.trigger = state.stepEditorDraft.trigger;
          step.mute = state.stepEditorDraft.mute;
          step.velocity = state.stepEditorDraft.velocity;
          step.probability = state.stepEditorDraft.probability;
          step.lockEnabled = state.stepEditorDraft.lockEnabled;
          step.lockPitch = state.stepEditorDraft.lockPitch;
          step.lockDecay = state.stepEditorDraft.lockDecay;
          renderGrid();
        }
        else
        {
          alert("Error: " + data.error);
        }
      }
      catch (e)
      {
        hideBusy();
        alert("Update failed: " + e);
      }
    }
  }

  state.stepEditorOpen = false;
  document.getElementById("stepEditor").style.display = "none";
}

// ========== INITIALIZE ==========
console.log("Groovebox WebUI loaded");
