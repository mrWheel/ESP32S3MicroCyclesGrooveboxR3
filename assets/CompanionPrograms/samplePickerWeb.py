#!/usr/bin/env python3
# -*- coding: utf-8 -*-

"""
Groovebox Sample Picker Web

Start:
  ./samplePickerWeb.py

Optional:
  ./samplePickerWeb.py "/path/to/source samples" "/path/to/destination-root"

Requirements:
  - macOS
  - Python 3 standard library only
  - afplay, included with macOS
  - ffmpeg:
      brew install ffmpeg

This version does NOT use Tkinter.
It starts a local web UI and opens it in your browser.

Important behavior:
  - Letter keys jump to the first source sample whose filename starts with that letter.
  - Play Original plays the currently selected source sample.
  - Play Groovebox plays the currently existing target sample in the selected sample set if it exists.
  - If the target sample does not exist yet, Play Groovebox creates a temporary converted preview from the source sample.
  - Select / Convert writes the converted sample to the selected sample set.
  - setGain.json is created automatically and controlled by sliders.
"""

import argparse
import json
import shutil
import subprocess
import sys
import tempfile
import webbrowser
from http.server import BaseHTTPRequestHandler
from http.server import ThreadingHTTPServer
from pathlib import Path


SUPPORTED_EXTENSIONS = {
  ".wav",
  ".aif",
  ".aiff",
  ".mp3",
  ".flac",
  ".m4a",
  ".caf",
}


GROOVEBOX_TYPES = [
  "kick",
  "snare",
  "ch",
  "oh",
  "tone",
  "metal",
]


SAMPLE_SETS = [
  "S1",
  "S2",
  "S3",
  "S4",
  "S5",
  "S6",
  "S7",
  "S8",
  "S9",
]


DEFAULT_GAIN_PERCENT = 100
MIN_GAIN_PERCENT = 0
MAX_GAIN_PERCENT = 400


HTML = r"""<!doctype html>
<html>
<head>
  <meta charset="utf-8">
  <title>Groovebox Sample Picker</title>
  <style>
    body {
      font-family: -apple-system, BlinkMacSystemFont, Helvetica, Arial, sans-serif;
      margin: 24px;
      color: #111;
      background: #f7f7f7;
    }
    .card {
      background: white;
      border: 1px solid #ddd;
      border-radius: 14px;
      padding: 16px;
      margin-bottom: 16px;
      box-shadow: 0 2px 10px rgba(0,0,0,0.04);
    }
    h1 {
      margin-top: 0;
      font-size: 24px;
    }
    h2 {
      margin-top: 0;
      font-size: 18px;
    }
    label {
      font-weight: 600;
      display: block;
      margin-bottom: 6px;
    }
    input[type=text] {
      width: calc(100% - 130px);
      padding: 8px;
      border: 1px solid #ccc;
      border-radius: 8px;
      font-size: 14px;
    }
    button, select {
      padding: 8px 12px;
      border-radius: 8px;
      border: 1px solid #aaa;
      background: #fff;
      font-size: 14px;
      cursor: pointer;
    }
    button:hover {
      background: #eee;
    }
    .row {
      display: flex;
      gap: 8px;
      align-items: center;
      margin-bottom: 12px;
      flex-wrap: wrap;
    }
    .path {
      font-size: 13px;
      color: #555;
      word-break: break-all;
    }
    .sample-name {
      font-size: 22px;
      font-weight: 700;
      margin: 8px 0;
    }
    .target-name {
      font-size: 15px;
      font-weight: 600;
      color: #333;
      margin: 6px 0;
    }
    .status {
      font-weight: 600;
      color: #064;
    }
    .warning {
      color: #900;
    }
    .gain-row {
      display: grid;
      grid-template-columns: 90px 1fr 60px 2fr;
      gap: 12px;
      align-items: center;
      margin: 8px 0;
    }
    .gain-row button {
      width: 90px;
    }
    input[type=range] {
      width: 100%;
    }
    .source-input {
      width: calc(100% - 18px);
      padding: 7px;
      border: 1px solid #ccc;
      border-radius: 8px;
      font-size: 13px;
    }
    .small {
      font-size: 13px;
      color: #666;
    }
    .hint {
      color: #555;
      font-size: 13px;
      margin-top: 6px;
    }
    .kbd {
      border: 1px solid #bbb;
      border-bottom-width: 2px;
      border-radius: 5px;
      padding: 1px 5px;
      background: #fafafa;
      font-family: ui-monospace, SFMono-Regular, Menlo, monospace;
    }
  </style>
</head>
<body>
  <h1>Groovebox Sample Picker</h1>

  <div class="card">
    <h2>Directories</h2>

    <label>Source samples</label>
    <div class="row">
      <input id="sourceDir" type="text">
      <button onclick="chooseSource()">Choose...</button>
    </div>

    <label>Destination root</label>
    <div class="row">
      <input id="destinationRoot" type="text">
      <button onclick="chooseDestination()">Choose...</button>
    </div>

    <div class="row">
      <label style="margin:0;">Sample set</label>
      <select id="sampleSet" onchange="setSampleSet()">
        <option>S1</option>
        <option>S2</option>
        <option>S3</option>
        <option>S4</option>
        <option>S5</option>
        <option>S6</option>
        <option>S7</option>
        <option>S8</option>
        <option>S9</option>
      </select>
      <button onclick="reloadState()">Reload</button>
    </div>

    <div class="small">Actual output:</div>
    <div id="actualOutput" class="path"></div>
  </div>

  <div class="card">
    <h2>Sample Browser</h2>
    <div id="counter" class="small">0 samples</div>
    <div id="sampleName" class="sample-name">No sample loaded</div>
    <div id="samplePath" class="path"></div>
    <div id="targetInfo" class="target-name"></div>
    <div class="hint">
      Keyboard: <span class="kbd">←</span>/<span class="kbd">→</span> navigate,
      <span class="kbd">space</span> play Groovebox,
      <span class="kbd">enter</span> convert,
      type <span class="kbd">a</span>..<span class="kbd">z</span> to jump to first filename starting with that letter.
    </div>

    <div class="row" style="margin-top:16px;">
      <button onclick="backSample()">Back</button>
      <button onclick="playOriginal()">Play Original</button>
      <button onclick="playGroovebox()">Play Groovebox</button>
      <button onclick="forwardSample()">Forward</button>
    </div>

    <div class="row">
      <button id="removeSilenceButton" onclick="toggleRemoveSilence()">Remove silence: ON</button>
      <span class="small">Used for Select / Convert and generated Groovebox previews.</span>
    </div>

    <div class="row">
      <label style="margin:0;">Groovebox type</label>
      <select id="grooveboxType" onchange="typeChanged()">
        <option>kick</option>
        <option>snare</option>
        <option>ch</option>
        <option>oh</option>
        <option>tone</option>
        <option>metal</option>
      </select>
      <button onclick="convertSelected()">Select / Convert</button>
    </div>
  </div>

  <div class="card">
    <h2>setGain.json</h2>
    <div id="gainSliders"></div>
  </div>

  <div class="card">
    <div id="status" class="status">Ready</div>
  </div>

<script>
let state = {};

async function api(path, payload = null) {
  const options = payload ? {
    method: "POST",
    headers: {"Content-Type": "application/json"},
    body: JSON.stringify(payload)
  } : {};
  const response = await fetch(path, options);
  const data = await response.json();
  if (!response.ok || data.ok === false) {
    throw new Error(data.error || "Request failed");
  }
  return data;
}

function selectedGrooveboxType() {
  return document.getElementById("grooveboxType").value;
}

function escapeHtml(text) {
  return String(text)
    .replace(/&/g, "&amp;")
    .replace(/</g, "&lt;")
    .replace(/>/g, "&gt;")
    .replace(/"/g, "&quot;");
}

function setStatus(text, warning=false) {
  const el = document.getElementById("status");
  el.textContent = text;
  el.className = warning ? "status warning" : "status";
}

function render() {
  document.getElementById("sourceDir").value = state.sourceDir || "";
  document.getElementById("destinationRoot").value = state.destinationRoot || "";
  document.getElementById("sampleSet").value = state.sampleSet || "S1";
  document.getElementById("actualOutput").textContent = state.destinationDir || "";

  const removeSilenceButton = document.getElementById("removeSilenceButton");
  if (removeSilenceButton) {
    removeSilenceButton.textContent = state.removeSilence ? "Remove silence: ON" : "Remove silence: OFF";
  }

  document.getElementById("counter").textContent =
    state.sampleCount ? `Sample ${state.currentIndex + 1} of ${state.sampleCount}` : "0 samples";

  document.getElementById("sampleName").textContent =
    state.currentSampleName || "No sample loaded";

  document.getElementById("samplePath").textContent =
    state.currentSamplePath || "";

  updateTargetInfo();

  const gainRoot = document.getElementById("gainSliders");
  gainRoot.innerHTML = "";

  const types = ["kick", "snare", "ch", "oh", "tone", "metal"];
  for (const name of types) {
    const value = (state.gainValues && state.gainValues[name] !== undefined) ? state.gainValues[name] : 100;
    const sourceValue = (state.sourceValues && state.sourceValues[name] !== undefined) ? state.sourceValues[name] : "";
    const row = document.createElement("div");
    row.className = "gain-row";
    row.innerHTML = `
      <button onclick="playGainPreview('${name}')">${name}</button>
      <input id="range-gain-${name}" type="range" min="0" max="400" value="${value}" oninput="gainChanged('${name}', this.value)">
      <div id="gain-${name}">${value}%</div>
      <input id="source-${name}" class="source-input" type="text" value="${escapeHtml(sourceValue)}" onchange="sourceChanged('${name}', this.value)">
    `;
    gainRoot.appendChild(row);
  }

  if (state.status) {
    setStatus(state.status, false);
  }
}

function updateTargetInfo() {
  const type = selectedGrooveboxType();
  const targetExists = state.targetExistsByType && state.targetExistsByType[type];
  const targetPath = state.targetPathByType ? state.targetPathByType[type] : "";
  const gain = state.gainValues && state.gainValues[type] !== undefined ? state.gainValues[type] : 100;

  document.getElementById("targetInfo").textContent =
    targetExists
      ? `Groovebox target exists: ${targetPath} (${gain}%)`
      : `Groovebox target missing: ${targetPath || "-"} (${gain}%). Preview will be generated from source.`;
}

async function reloadState() {
  try {
    const sourceDir = document.getElementById("sourceDir").value;
    const destinationRoot = document.getElementById("destinationRoot").value;
    const sampleSet = document.getElementById("sampleSet").value;
    state = await api("/api/reload", {sourceDir, destinationRoot, sampleSet});
    render();
  } catch (e) {
    setStatus(e.message, true);
  }
}

async function chooseSource() {
  try {
    state = await api("/api/choose-source", {});
    render();
  } catch (e) {
    setStatus(e.message, true);
  }
}

async function chooseDestination() {
  try {
    state = await api("/api/choose-destination", {});
    render();
  } catch (e) {
    setStatus(e.message, true);
  }
}

async function setSampleSet() {
  await reloadState();
}

function typeChanged() {
  updateTargetInfo();
}

async function toggleRemoveSilence() {
  try {
    const removeSilence = !state.removeSilence;
    state = await api("/api/remove-silence", {removeSilence});
    render();
  } catch (e) {
    setStatus(e.message, true);
  }
}

async function backSample() {
  try {
    state = await api("/api/back", {});
    render();
  } catch (e) {
    setStatus(e.message, true);
  }
}

async function forwardSample() {
  try {
    state = await api("/api/forward", {});
    render();
  } catch (e) {
    setStatus(e.message, true);
  }
}

async function jumpToLetter(letter) {
  try {
    state = await api("/api/jump-letter", {letter});
    render();
  } catch (e) {
    setStatus(e.message, true);
  }
}

async function playOriginal() {
  try {
    state = await api("/api/play-original", {});
    render();
  } catch (e) {
    setStatus(e.message, true);
  }
}

async function playGroovebox() {
  try {
    const grooveboxType = selectedGrooveboxType();
    state = await api("/api/play-groovebox", {grooveboxType});
    render();
  } catch (e) {
    setStatus(e.message, true);
  }
}

async function convertSelected() {
  try {
    const grooveboxType = selectedGrooveboxType();
    state = await api("/api/convert", {grooveboxType});
    render();
  } catch (e) {
    setStatus(e.message, true);
  }
}

async function playGainPreview(name) {
  try {
    const slider = document.getElementById(`range-gain-${name}`);
    const value = slider ? Number(slider.value) : 100;
    state = await api("/api/play-gain-preview", {name, value});
    render();
  } catch (e) {
    setStatus(e.message, true);
  }
}

let gainTimer = null;
function gainChanged(name, value) {
  document.getElementById(`gain-${name}`).textContent = `${value}%`;
  if (gainTimer) {
    clearTimeout(gainTimer);
  }
  gainTimer = setTimeout(async () => {
    try {
      state = await api("/api/gain", {name, value: Number(value)});
      render();
    } catch (e) {
      setStatus(e.message, true);
    }
  }, 120);
}

document.addEventListener("keydown", (event) => {
  const tagName = document.activeElement ? document.activeElement.tagName.toLowerCase() : "";
  const isTypingField = tagName === "input" || tagName === "textarea" || tagName === "select";

  if (event.key === "ArrowLeft") {
    event.preventDefault();
    backSample();
    return;
  }

  if (event.key === "ArrowRight") {
    event.preventDefault();
    forwardSample();
    return;
  }

  if (event.key === " ") {
    event.preventDefault();
    playGroovebox();
    return;
  }

  if (event.key === "Enter") {
    event.preventDefault();
    convertSelected();
    return;
  }

  if (!isTypingField && event.key.length === 1 && /^[a-zA-Z0-9]$/.test(event.key)) {
    event.preventDefault();
    jumpToLetter(event.key.toLowerCase());
  }
});

reloadState();
</script>
</body>
</html>
"""


class AppState:
  def __init__(self, source_dir=None, destination_root=None):
    self.source_dir = source_dir
    self.destination_root = destination_root
    self.sample_set = "S1"
    self.sample_paths = []
    self.current_index = 0
    self.play_process = None
    self.preview_path = Path(tempfile.gettempdir()) / "groovebox_sample_picker_preview.wav"
    self.gain_preview_path = Path(tempfile.gettempdir()) / "groovebox_sample_picker_gain_preview.wav"
    self.remove_silence = True
    self.status = "Ready"

  def validate_tools(self):
    if shutil.which("afplay") is None:
      raise RuntimeError("afplay was not found. This script is intended for macOS.")

    if shutil.which("ffmpeg") is None:
      raise RuntimeError("ffmpeg was not found. Install it with: brew install ffmpeg")

  def destination_dir(self):
    if self.destination_root is None:
      return None

    if self.destination_root.name in SAMPLE_SETS:
      return self.destination_root

    return self.destination_root / self.sample_set

  def reload(self):
    self.sample_paths = []
    self.current_index = 0

    if self.source_dir and self.source_dir.exists() and self.source_dir.is_dir():
      self.sample_paths = sorted(
        [
          path
          for path in self.source_dir.rglob("*")
          if path.is_file() and path.suffix.lower() in SUPPORTED_EXTENSIONS
        ],
        key=lambda item: str(item).lower()
      )

    destination_dir = self.destination_dir()
    if destination_dir:
      destination_dir.mkdir(parents=True, exist_ok=True)
      self.ensure_gain_file()

    if not self.sample_paths:
      self.status = "No samples loaded"
    else:
      self.status = "Ready"

  def current_sample(self):
    if not self.sample_paths:
      return None

    return self.sample_paths[self.current_index]

  def stop_playback(self):
    if self.play_process is not None and self.play_process.poll() is None:
      self.play_process.terminate()

    self.play_process = None

  def play_audio_file(self, path, label):
    self.stop_playback()
    self.play_process = subprocess.Popen(["afplay", str(path)])
    self.status = f"Playing {label}: {path.name}"

  def target_sample_path(self, groovebox_type):
    destination_dir = self.destination_dir()
    if destination_dir is None:
      return None

    preferred_path = destination_dir / f"{groovebox_type}.wav"
    if preferred_path.exists():
      return preferred_path

    for extension in sorted(SUPPORTED_EXTENSIONS):
      candidate_path = destination_dir / f"{groovebox_type}{extension}"
      if candidate_path.exists():
        return candidate_path

    for candidate_path in destination_dir.iterdir() if destination_dir.exists() else []:
      if not candidate_path.is_file():
        continue
      if candidate_path.suffix.lower() not in SUPPORTED_EXTENSIONS:
        continue
      if candidate_path.stem.lower() == groovebox_type.lower():
        return candidate_path

    return preferred_path

  def target_exists_by_type(self):
    result = {}
    for groovebox_type in GROOVEBOX_TYPES:
      target_path = self.target_sample_path(groovebox_type)
      result[groovebox_type] = bool(target_path and target_path.exists())
    return result

  def target_path_by_type(self):
    result = {}
    for groovebox_type in GROOVEBOX_TYPES:
      target_path = self.target_sample_path(groovebox_type)
      result[groovebox_type] = str(target_path) if target_path else ""
    return result

  def conversion_audio_filter(self):
    filters = []

    if self.remove_silence:
      filters.append(
        "silenceremove=start_periods=1:start_threshold=-60dB:start_silence=0.005:"
        "stop_periods=1:stop_threshold=-60dB:stop_silence=0.020"
      )

    filters.extend([
      "loudnorm=I=-16:TP=-3:LRA=11",
      "volume=-3dB",
    ])

    return ",".join(filters)

  def build_ffmpeg_command(self, source_path, output_path):
    return [
      "ffmpeg",
      "-hide_banner",
      "-loglevel",
      "error",
      "-y",
      "-i",
      str(source_path),
      "-af",
      self.conversion_audio_filter(),
      "-ac",
      "1",
      "-ar",
      "44100",
      "-sample_fmt",
      "s16",
      str(output_path),
    ]

  def convert_to_path(self, source_path, output_path):
    result = subprocess.run(
      self.build_ffmpeg_command(source_path, output_path),
      capture_output=True,
      text=True
    )

    if result.returncode != 0:
      raise RuntimeError(result.stderr.strip() if result.stderr.strip() else "ffmpeg failed")

  def gain_volume_factor(self, value):
    value = max(MIN_GAIN_PERCENT, min(MAX_GAIN_PERCENT, int(value)))
    return value / 100.0

  def build_gain_preview_command(self, source_path, output_path, value, already_converted):
    gain_factor = self.gain_volume_factor(value)

    if already_converted:
      audio_filter = f"volume={gain_factor:.4f}"
    else:
      audio_filter = f"{self.conversion_audio_filter()},volume={gain_factor:.4f}"

    return [
      "ffmpeg",
      "-hide_banner",
      "-loglevel",
      "error",
      "-y",
      "-i",
      str(source_path),
      "-af",
      audio_filter,
      "-ac",
      "1",
      "-ar",
      "44100",
      "-sample_fmt",
      "s16",
      str(output_path),
    ]

  def create_gain_preview(self, groovebox_type, value):
    target_path = self.target_sample_path(groovebox_type)
    source_path = None
    already_converted = False

    if target_path is not None and target_path.exists():
      source_path = target_path
      already_converted = True
    else:
      source_path = self.current_sample()
      already_converted = False

    if source_path is None:
      raise RuntimeError("No sample selected")

    self.stop_playback()

    if self.gain_preview_path.exists():
      self.gain_preview_path.unlink()

    result = subprocess.run(
      self.build_gain_preview_command(source_path, self.gain_preview_path, value, already_converted),
      capture_output=True,
      text=True
    )

    if result.returncode != 0:
      raise RuntimeError(result.stderr.strip() if result.stderr.strip() else "ffmpeg failed")

    return self.gain_preview_path, already_converted

  def gain_file_path(self):
    destination_dir = self.destination_dir()
    if destination_dir is None:
      return None
    return destination_dir / "setGain.json"

  def default_gain_payload(self):
    return {
      "sampleGainPercent": {
        name: DEFAULT_GAIN_PERCENT
        for name in GROOVEBOX_TYPES
      },
      "sampleSource": {
        name: ""
        for name in GROOVEBOX_TYPES
      }
    }

  def normalized_gain_payload(self):
    payload = self.default_gain_payload()
    gain_file = self.gain_file_path()

    if gain_file is None or not gain_file.exists():
      return payload

    try:
      loaded = json.loads(gain_file.read_text(encoding="utf-8"))
    except Exception:
      return payload

    gain_values = loaded.get("sampleGainPercent", loaded.get("setGain", {}))
    source_values = loaded.get("sampleSource", {})

    if isinstance(gain_values, dict):
      for name in GROOVEBOX_TYPES:
        try:
          value = int(gain_values.get(name, DEFAULT_GAIN_PERCENT))
        except (TypeError, ValueError):
          value = DEFAULT_GAIN_PERCENT
        payload["sampleGainPercent"][name] = max(MIN_GAIN_PERCENT, min(MAX_GAIN_PERCENT, value))

    if isinstance(source_values, dict):
      for name in GROOVEBOX_TYPES:
        value = source_values.get(name, "")
        payload["sampleSource"][name] = str(value) if value is not None else ""

    return payload

  def save_gain_payload(self, payload):
    gain_file = self.gain_file_path()
    if gain_file is None:
      raise RuntimeError("No destination directory selected")

    gain_file.parent.mkdir(parents=True, exist_ok=True)
    gain_file.write_text(json.dumps(payload, indent=2) + "\n", encoding="utf-8")

  def ensure_gain_file(self):
    gain_file = self.gain_file_path()
    if gain_file is None:
      return

    payload = self.normalized_gain_payload()
    self.save_gain_payload(payload)

  def load_gain_values(self):
    return self.normalized_gain_payload()["sampleGainPercent"]

  def load_source_values(self):
    return self.normalized_gain_payload()["sampleSource"]

  def save_gain_value(self, name, value):
    value = max(MIN_GAIN_PERCENT, min(MAX_GAIN_PERCENT, int(value)))
    payload = self.normalized_gain_payload()
    payload["sampleGainPercent"][name] = value
    self.save_gain_payload(payload)
    self.status = f"Gain saved: {name} = {value}%"

  def save_source_value(self, name, value):
    payload = self.normalized_gain_payload()
    payload["sampleSource"][name] = str(value)
    self.save_gain_payload(payload)
    self.status = f"Source saved: {name}"

  def sample_source_text(self, sample_path):
    documents_dir = Path.home() / "Documents"

    try:
      return str(sample_path.resolve().relative_to(documents_dir)).replace("\\", "/")
    except ValueError:
      pass

    return str(sample_path).replace("\\", "/")

  def jump_to_letter(self, letter):
    if not self.sample_paths:
      return

    needle = letter.lower()

    for index, path in enumerate(self.sample_paths):
      if path.name.lower().startswith(needle):
        self.current_index = index
        self.status = f"Jumped to {letter.upper()}"
        return

    self.status = f"No sample starting with {letter.upper()}"

  def as_dict(self):
    current = self.current_sample()
    destination_dir = self.destination_dir()
    return {
      "ok": True,
      "sourceDir": str(self.source_dir) if self.source_dir else "",
      "destinationRoot": str(self.destination_root) if self.destination_root else "",
      "destinationDir": str(destination_dir) if destination_dir else "",
      "sampleSet": self.sample_set,
      "removeSilence": self.remove_silence,
      "sampleCount": len(self.sample_paths),
      "currentIndex": self.current_index,
      "currentSampleName": current.name if current else "",
      "currentSamplePath": str(current) if current else "",
      "targetExistsByType": self.target_exists_by_type(),
      "targetPathByType": self.target_path_by_type(),
      "gainValues": self.load_gain_values(),
      "sourceValues": self.load_source_values(),
      "status": self.status,
    }


STATE = None


def choose_folder_with_applescript(prompt):
  script = f'POSIX path of (choose folder with prompt "{prompt}")'
  result = subprocess.run(
    ["osascript", "-e", script],
    capture_output=True,
    text=True
  )

  if result.returncode != 0:
    raise RuntimeError("Folder selection cancelled")

  return Path(result.stdout.strip()).expanduser().resolve()


class Handler(BaseHTTPRequestHandler):
  def do_GET(self):
    if self.path == "/" or self.path.startswith("/index.html"):
      self.send_response(200)
      self.send_header("Content-Type", "text/html; charset=utf-8")
      self.end_headers()
      self.wfile.write(HTML.encode("utf-8"))
      return

    if self.path == "/api/state":
      self.send_json(STATE.as_dict())
      return

    self.send_error(404)

  def do_POST(self):
    try:
      length = int(self.headers.get("Content-Length", "0"))
      raw = self.rfile.read(length).decode("utf-8") if length > 0 else "{}"
      payload = json.loads(raw) if raw else {}

      if self.path == "/api/reload":
        source = payload.get("sourceDir", "").strip()
        destination = payload.get("destinationRoot", "").strip()
        sample_set = payload.get("sampleSet", "S1")

        if source:
          STATE.source_dir = Path(source).expanduser().resolve()
        if destination:
          STATE.destination_root = Path(destination).expanduser().resolve()
        if sample_set in SAMPLE_SETS:
          STATE.sample_set = sample_set

        STATE.reload()
        self.send_json(STATE.as_dict())
        return

      if self.path == "/api/choose-source":
        STATE.source_dir = choose_folder_with_applescript("Choose source sample directory")
        STATE.reload()
        self.send_json(STATE.as_dict())
        return

      if self.path == "/api/choose-destination":
        STATE.destination_root = choose_folder_with_applescript("Choose destination root directory")
        STATE.reload()
        self.send_json(STATE.as_dict())
        return

      if self.path == "/api/remove-silence":
        STATE.remove_silence = bool(payload.get("removeSilence", True))
        STATE.status = "Remove silence: ON" if STATE.remove_silence else "Remove silence: OFF"
        self.send_json(STATE.as_dict())
        return

      if self.path == "/api/back":
        if STATE.sample_paths:
          STATE.stop_playback()
          STATE.current_index = (STATE.current_index - 1) % len(STATE.sample_paths)
          STATE.status = "Ready"
        self.send_json(STATE.as_dict())
        return

      if self.path == "/api/forward":
        if STATE.sample_paths:
          STATE.stop_playback()
          STATE.current_index = (STATE.current_index + 1) % len(STATE.sample_paths)
          STATE.status = "Ready"
        self.send_json(STATE.as_dict())
        return

      if self.path == "/api/jump-letter":
        letter = str(payload.get("letter", ""))[:1]
        if not letter:
          raise RuntimeError("Missing letter")
        STATE.stop_playback()
        STATE.jump_to_letter(letter)
        self.send_json(STATE.as_dict())
        return

      if self.path == "/api/play-original":
        sample = STATE.current_sample()
        if sample is None:
          raise RuntimeError("No sample selected")
        STATE.play_audio_file(sample, "original")
        self.send_json(STATE.as_dict())
        return

      if self.path == "/api/play-groovebox":
        groovebox_type = payload.get("grooveboxType", "kick")
        if groovebox_type not in GROOVEBOX_TYPES:
          raise RuntimeError("Invalid groovebox type")

        target_path = STATE.target_sample_path(groovebox_type)

        if target_path is not None and target_path.exists():
          STATE.play_audio_file(target_path, f"Groovebox {groovebox_type}")
          self.send_json(STATE.as_dict())
          return

        sample = STATE.current_sample()
        if sample is None:
          raise RuntimeError("No sample selected")

        STATE.stop_playback()

        if STATE.preview_path.exists():
          STATE.preview_path.unlink()

        STATE.convert_to_path(sample, STATE.preview_path)
        STATE.play_audio_file(STATE.preview_path, "Groovebox preview from source")
        self.send_json(STATE.as_dict())
        return

      if self.path == "/api/convert":
        sample = STATE.current_sample()
        destination_dir = STATE.destination_dir()
        groovebox_type = payload.get("grooveboxType", "")

        if sample is None:
          raise RuntimeError("No sample selected")
        if destination_dir is None:
          raise RuntimeError("No destination selected")
        if groovebox_type not in GROOVEBOX_TYPES:
          raise RuntimeError("Invalid groovebox type")

        destination_dir.mkdir(parents=True, exist_ok=True)
        STATE.ensure_gain_file()

        output_path = destination_dir / f"{groovebox_type}.wav"
        temporary_path = destination_dir / f"{groovebox_type}.tmp.wav"

        if temporary_path.exists():
          temporary_path.unlink()

        STATE.convert_to_path(sample, temporary_path)

        if output_path.exists():
          output_path.unlink()

        temporary_path.rename(output_path)
        STATE.save_source_value(groovebox_type, STATE.sample_source_text(sample))
        STATE.status = f"Saved: {output_path}"
        self.send_json(STATE.as_dict())
        return

      if self.path == "/api/play-gain-preview":
        name = payload.get("name", "")
        value = payload.get("value", DEFAULT_GAIN_PERCENT)
        if name not in GROOVEBOX_TYPES:
          raise RuntimeError("Invalid gain name")

        STATE.save_gain_value(name, value)
        preview_path, already_converted = STATE.create_gain_preview(name, value)
        source_text = "target sample" if already_converted else "current source sample"
        STATE.play_audio_file(preview_path, f"{name} gain preview from {source_text} at {int(value)}%")
        self.send_json(STATE.as_dict())
        return

      if self.path == "/api/gain":
        name = payload.get("name", "")
        value = payload.get("value", DEFAULT_GAIN_PERCENT)
        if name not in GROOVEBOX_TYPES:
          raise RuntimeError("Invalid gain name")
        STATE.save_gain_value(name, value)
        self.send_json(STATE.as_dict())
        return

      if self.path == "/api/source":
        name = payload.get("name", "")
        value = payload.get("value", "")
        if name not in GROOVEBOX_TYPES:
          raise RuntimeError("Invalid source name")
        STATE.save_source_value(name, value)
        self.send_json(STATE.as_dict())
        return

      self.send_error(404)

    except Exception as error:
      self.send_json({"ok": False, "error": str(error)}, status=400)

  def send_json(self, data, status=200):
    body = json.dumps(data).encode("utf-8")
    self.send_response(status)
    self.send_header("Content-Type", "application/json")
    self.send_header("Content-Length", str(len(body)))
    self.end_headers()
    self.wfile.write(body)

  def log_message(self, format, *args):
    return


def parse_args():
  parser = argparse.ArgumentParser(description="Browser based sample picker for ESP32 MicroCycles Groovebox.")
  parser.add_argument("source_dir", nargs="?", help="Optional source directory")
  parser.add_argument("destination_dir", nargs="?", help="Optional destination root directory")
  parser.add_argument("--port", type=int, default=8765)
  return parser.parse_args()


def main():
  global STATE

  args = parse_args()
  source_dir = Path(args.source_dir).expanduser().resolve() if args.source_dir else None
  destination_root = Path(args.destination_dir).expanduser().resolve() if args.destination_dir else None

  STATE = AppState(source_dir, destination_root)
  STATE.validate_tools()
  STATE.reload()

  server = ThreadingHTTPServer(("127.0.0.1", args.port), Handler)
  url = f"http://127.0.0.1:{args.port}/"

  print(f"Groovebox Sample Picker running at {url}")
  print("Press Ctrl+C to stop.")

  webbrowser.open(url)

  try:
    server.serve_forever()
  except KeyboardInterrupt:
    pass
  finally:
    STATE.stop_playback()
    server.shutdown()
    server.server_close()


if __name__ == "__main__":
  main()
