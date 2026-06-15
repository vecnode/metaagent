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

function setCommandOutput(message, isError = false) {
  const output = document.getElementById("command-output");
  output.textContent = message;
  output.style.color = isError ? "#fca5a5" : "";
}

async function refreshStatus() {
  const status = await fetchJson("/api/status");
  document.getElementById("status-host").textContent = status.host || "metaagent_app";
  document.getElementById("status-pattern").textContent =
    status.pattern_state ? `${status.pattern_state} · ${status.pattern_preset || ""}` : "…";
  document.getElementById("status-particles").textContent =
    typeof status.particle_count === "number" ? String(status.particle_count) : "…";
}

async function refreshNotifyLog() {
  const payload = await fetchJson("/api/notify/log");
  const list = document.getElementById("notify-log");
  list.innerHTML = "";
  for (const entry of payload.entries || []) {
    const item = document.createElement("li");
    item.textContent = entry.message || JSON.stringify(entry);
    list.appendChild(item);
  }
}

async function dispatchCommand(command) {
  setCommandOutput("Running…");
  const result = await fetchJson("/api/command", {
    method: "POST",
    body: JSON.stringify({ command }),
  });
  if (result.success) {
    setCommandOutput(result.message || `OK · ${result.pattern_state || command}`);
  } else {
    setCommandOutput(result.message || "Command rejected", true);
  }
  await refreshStatus();
}

document.querySelectorAll("[data-command]").forEach((button) => {
  button.addEventListener("click", async () => {
    button.disabled = true;
    try {
      await dispatchCommand(button.dataset.command);
    } finally {
      button.disabled = false;
    }
  });
});

document.getElementById("notify-form").addEventListener("submit", async (event) => {
  event.preventDefault();
  const input = document.getElementById("notify-input");
  const message = input.value.trim();
  if (!message) {
    return;
  }

  const body = message.startsWith("{") ? message : JSON.stringify({ message });
  await fetch("/notify", { method: "POST", body });
  input.value = "";
  await refreshNotifyLog();
  await refreshStatus();
});

document.getElementById("chat-form").addEventListener("submit", async (event) => {
  event.preventDefault();
  const input = document.getElementById("chat-input");
  const prompt = input.value.trim();
  if (!prompt) {
    return;
  }

  const output = document.getElementById("chat-output");
  output.textContent = "Thinking…";

  const result = await fetchJson("/ai/chat", {
    method: "POST",
    body: JSON.stringify({ prompt }),
  });

  output.textContent = result.assistant || result.reply || result.message || result.error || JSON.stringify(result, null, 2);
});

refreshStatus();
refreshNotifyLog();
setInterval(refreshStatus, 2000);
setInterval(refreshNotifyLog, 3000);
