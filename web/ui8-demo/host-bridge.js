const STORAGE_KEY = "sil-ui8-demo:inputs";

function appendLogLine(container, text) {
  const line = document.createElement("div");
  line.className = "host-line";
  line.textContent = text;
  container.prepend(line);
}

export function createBrowserHost(logContainer) {
  return {
    monotonicUsec() {
      return Math.floor(performance.now() * 1000);
    },

    wallUsec() {
      return Math.floor(Date.now() * 1000);
    },

    log(level, subsystem, message) {
      const stamp = new Date().toLocaleTimeString();
      const text = `[${stamp}] ${level.toUpperCase()} ${subsystem}: ${message}`;
      appendLogLine(logContainer, text);
      if (level === "error") {
        console.error(text);
      } else if (level === "warn") {
        console.warn(text);
      } else {
        console.log(text);
      }
    },

    async loadBinaryResource(name) {
      this.log("info", "host", `fetch ${name}`);
      const response = await fetch(`./samples/${name}`);
      if (!response.ok) {
        throw new Error(`Unable to load ${name}: ${response.status}`);
      }
      return new Uint8Array(await response.arrayBuffer());
    },

    async loadJsonResource(name) {
      this.log("info", "host", `fetch ${name}`);
      const response = await fetch(`./samples/${name}`);
      if (!response.ok) {
        throw new Error(`Unable to load ${name}: ${response.status}`);
      }
      return response.json();
    },

    loadPersistedInputs() {
      const raw = localStorage.getItem(STORAGE_KEY);
      if (!raw) {
        return [];
      }
      try {
        return JSON.parse(raw);
      } catch (error) {
        this.log("warn", "host", "Invalid persisted input history; clearing.");
        localStorage.removeItem(STORAGE_KEY);
        return [];
      }
    },

    storePersistedInputs(entries) {
      localStorage.setItem(STORAGE_KEY, JSON.stringify(entries.slice(0, 8)));
      this.log("debug", "host", `stored ${Math.min(entries.length, 8)} input packets`);
    },
  };
}
