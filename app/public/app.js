async function fetchJson(path, options = {}) {
  const response = await fetch(path, {
    headers: { "Content-Type": "application/json" },
    ...options,
  });
  const text = await response.text();
  try {
    return JSON.parse(text);
  } catch {
    return { raw: text, ok: response.ok, status: response.status };
  }
}

function setHint(id, message, isError = false) {
  const el = document.getElementById(id);
  if (!el) return;
  el.textContent = message;
  el.classList.toggle("error", isError);
}

function formatSequenceStatus(status) {
  if (!status || status.state === "idle") {
    return "No sequence loaded.";
  }
  const index = Math.max(0, status.current_index);
  const total = status.total_tiles || 0;
  return [
    `State: ${status.state}`,
    status.source_path ? `Source: ${status.source_path}` : null,
    total ? `Tile: ${index + 1} / ${total} (${status.columns}×${status.rows})` : null,
    status.tile_width ? `Tile size: ${status.tile_width}×${status.tile_height}` : null,
    status.error ? `Error: ${status.error}` : null,
  ]
    .filter(Boolean)
    .join(" · ");
}

async function refreshNetwork() {
  const network = await fetchJson("/api/network/status");
  const enabled = !!network.networking_enabled;
  const dot = document.getElementById("network-dot");
  const label = document.getElementById("network-label");
  const detail = document.getElementById("network-detail");
  const toggle = document.getElementById("network-toggle");

  dot.classList.toggle("online", enabled);
  dot.classList.toggle("offline", !enabled);
  label.textContent = enabled ? "Networking Engine ON" : "Networking Engine OFF";
  detail.textContent = `${network.target_count || 0} target(s) · ${network.signal_log_count || 0} log entries`;
  toggle.classList.toggle("active", enabled);
  toggle.textContent = enabled ? "Disable Networking" : "Enable Networking";
}

async function refreshRuntimes() {
  const payload = await fetchJson("/api/runtimes");
  const list = document.getElementById("runtime-list");
  list.innerHTML = "";
  const runtimes = payload.runtimes || [];
  if (!runtimes.length) {
    list.innerHTML = "<li class='runtime-item muted'>No runtimes reported.</li>";
    return;
  }
  for (const runtime of runtimes) {
    const item = document.createElement("li");
    item.className = "runtime-item";
    item.innerHTML = `
      <div class="runtime-title">
        <span>${runtime.title}</span>
        <span class="runtime-state ${runtime.active_in_session ? "active" : "inactive"}">
          ${runtime.active_in_session ? "active" : "inactive"}
        </span>
      </div>
      <div class="runtime-meta">${runtime.host_scope}</div>
      <div class="runtime-summary">${runtime.summary}</div>
    `;
    list.appendChild(item);
  }
}

async function refreshTargets() {
  const payload = await fetchJson("/api/targets");
  const targets = payload.targets || [];
  const list = document.getElementById("target-list");
  const select = document.getElementById("signal-target");
  list.innerHTML = "";
  select.innerHTML = "";

  if (!targets.length) {
    list.innerHTML = "<li class='target-item muted'>No targets registered.</li>";
    const option = document.createElement("option");
    option.value = "";
    option.textContent = "No targets";
    select.appendChild(option);
    return;
  }

  for (const target of targets) {
    const item = document.createElement("li");
    item.className = "target-item";
    item.innerHTML = `
      <div class="target-head">
        <strong>${target.id}</strong>
        <span class="target-state ${target.enabled ? "enabled" : "disabled"}">
          ${target.enabled ? "enabled" : "disabled"}
        </span>
      </div>
      <div class="target-url">${target.control_url}</div>
    `;
    list.appendChild(item);

    const option = document.createElement("option");
    option.value = target.id;
    option.textContent = target.id;
    select.appendChild(option);
  }
}

async function refreshCommsLog() {
  const [signals, notify] = await Promise.all([
    fetchJson("/api/signals/log"),
    fetchJson("/api/notify/log"),
  ]);

  const entries = [];
  for (const entry of signals.entries || []) {
    entries.push({
      kind: "signal",
      direction: entry.direction,
      text: `[${entry.direction}] ${entry.type} → ${entry.target || "—"} · ${entry.summary}`,
      delivered: entry.delivered,
    });
  }
  for (const entry of notify.entries || []) {
    entries.push({
      kind: "notify",
      direction: "inbound",
      text: `[notify] ${entry.message || JSON.stringify(entry)}`,
      delivered: true,
    });
  }

  const list = document.getElementById("comms-log");
  list.innerHTML = "";
  if (!entries.length) {
    list.innerHTML = "<li class='log-item muted'>No comms activity yet.</li>";
    return;
  }

  for (const entry of entries.slice().reverse()) {
    const item = document.createElement("li");
    item.className = `log-item ${entry.delivered ? "ok" : "fail"}`;
    item.textContent = entry.text;
    list.appendChild(item);
  }
}

async function refreshSequence() {
  const status = await fetchJson("/api/sequence/status");
  document.getElementById("sequence-status").textContent = formatSequenceStatus(status);
}

async function toggleNetworking() {
  const result = await fetchJson("/api/command", {
    method: "POST",
    body: JSON.stringify({ command: "toggle_networking_runtime" }),
  });
  setHint("signal-output", result.message || (result.success ? "Networking updated." : "Rejected."), !result.success);
  await refreshNetwork();
  await refreshRuntimes();
}

document.getElementById("network-toggle").addEventListener("click", toggleNetworking);

document.getElementById("target-form").addEventListener("submit", async (event) => {
  event.preventDefault();
  const id = document.getElementById("target-id").value.trim();
  const control_url = document.getElementById("target-url").value.trim();
  if (!id || !control_url) return;

  const result = await fetchJson("/api/targets/register", {
    method: "POST",
    body: JSON.stringify({ id, control_url, enabled: true }),
  });
  setHint("signal-output", result.success ? `Registered ${id}.` : result.message || "Registration failed.", !result.success);
  if (result.success) {
    document.getElementById("target-id").value = "";
    document.getElementById("target-url").value = "";
    await refreshTargets();
    await refreshNetwork();
  }
});

document.getElementById("signal-form").addEventListener("submit", async (event) => {
  event.preventDefault();
  const type = document.getElementById("signal-type").value;
  const target = document.getElementById("signal-target").value;
  const payloadText = document.getElementById("signal-payload").value.trim() || "{}";

  let payload;
  try {
    payload = JSON.parse(payloadText);
  } catch {
    setHint("signal-output", "Payload must be valid JSON.", true);
    return;
  }

  const body = {
    type,
    target,
    payload,
  };

  const result = await fetchJson("/api/signal", {
    method: "POST",
    body: JSON.stringify(body),
  });

  setHint(
    "signal-output",
    result.success ? `Signal ${result.signal_id || ""} delivered.` : result.message || "Delivery failed.",
    !result.success
  );
  await refreshCommsLog();
  await refreshNetwork();
});

document.getElementById("sequence-load-form").addEventListener("submit", async (event) => {
  event.preventDefault();
  const path = document.getElementById("sequence-path").value.trim();
  const columns = Number(document.getElementById("sequence-cols").value) || 4;
  const rows = Number(document.getElementById("sequence-rows").value) || 4;
  const target = document.getElementById("sequence-target").value.trim();

  const body = { path, columns, rows };
  if (target) body.target = target;

  const result = await fetchJson("/api/sequence/load", {
    method: "POST",
    body: JSON.stringify(body),
  });

  setHint("signal-output", result.message || (result.success ? "Sequence loaded." : "Load failed."), !result.success);
  await refreshSequence();
});

document.querySelectorAll("[data-seq]").forEach((button) => {
  button.addEventListener("click", async () => {
    const result = await fetchJson("/api/sequence/control", {
      method: "POST",
      body: JSON.stringify({ action: button.dataset.seq }),
    });
    setHint("signal-output", result.message || "Sequence updated.", !result.success);
    await refreshSequence();
    await refreshCommsLog();
  });
});

document.getElementById("chat-form").addEventListener("submit", async (event) => {
  event.preventDefault();
  const input = document.getElementById("chat-input");
  const prompt = input.value.trim();
  if (!prompt) return;

  const output = document.getElementById("chat-output");
  output.textContent = "Thinking…";

  const result = await fetchJson("/ai/chat", {
    method: "POST",
    body: JSON.stringify({ prompt }),
  });

  output.textContent =
    result.assistant || result.message || result.error || JSON.stringify(result, null, 2);
});

async function refreshAll() {
  await Promise.all([
    refreshNetwork(),
    refreshRuntimes(),
    refreshTargets(),
    refreshCommsLog(),
    refreshSequence(),
  ]);
}

refreshAll();
setInterval(refreshNetwork, 3000);
setInterval(refreshCommsLog, 2500);
setInterval(refreshSequence, 2000);
setInterval(refreshRuntimes, 10000);
