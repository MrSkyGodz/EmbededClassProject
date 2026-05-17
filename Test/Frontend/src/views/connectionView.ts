import type { PortStatus, TransportMode } from "../api/client";

export function connectionView(status: PortStatus | null): string {
  const isOpen = status?.open ?? false;
  const mode = status?.mode ?? "mock";
  const port = status?.port || "mock";
  const baud = status?.baud || 115200;

  return `
    <section class="panel">
      <div class="panel-header">
        <h2>Connection</h2>
        <span class="status ${isOpen ? "ok" : "idle"}">${isOpen ? "Open" : "Closed"}</span>
      </div>

      <div class="form-grid">
        <label>
          Mode
          <select id="mode">
            <option value="mock" ${mode === "mock" ? "selected" : ""}>Mock</option>
            <option value="serial" ${mode === "serial" ? "selected" : ""}>Serial</option>
          </select>
        </label>

        <label>
          Port
          <input id="port" value="${port}" />
        </label>

        <label>
          Baud
          <input id="baud" type="number" value="${baud}" />
        </label>
      </div>

      <div class="button-row">
        <button id="openPort">Open</button>
        <button id="closePort" class="secondary">Close</button>
        <button id="refreshConnection" class="secondary">Refresh</button>
      </div>

      <dl class="metrics">
        <div><dt>Mode</dt><dd>${status?.mode ?? "none"}</dd></div>
        <div><dt>Reader</dt><dd>${status?.readerRunning ? "running" : "stopped"}</dd></div>
        <div><dt>Received</dt><dd>${status?.receivedBytes ?? 0} bytes</dd></div>
      </dl>
    </section>
  `;
}

export function readConnectionForm(): { mode: TransportMode; port: string; baud: number } {
  const mode = (document.querySelector<HTMLSelectElement>("#mode")?.value ?? "mock") as TransportMode;
  const port = document.querySelector<HTMLInputElement>("#port")?.value.trim() || "mock";
  const baud = Number(document.querySelector<HTMLInputElement>("#baud")?.value || "115200");
  return { mode, port, baud };
}