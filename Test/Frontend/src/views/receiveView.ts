import type { ReceivedResponse } from "../api/client";

export function receiveView(received: ReceivedResponse | null): string {
  const rows = (received?.entries ?? [])
    .slice(-80)
    .map(
      (entry) => `
        <tr>
          <td>${entry.index}</td>
          <td>${entry.timestampMs}</td>
          <td class="mono">${entry.hex}</td>
          <td>${entry.ascii}</td>
        </tr>
      `
    )
    .join("");

  return `
    <section class="panel wide">
      <div class="panel-header">
        <h2>Received Data</h2>
        <span class="status idle">${received?.buffered ?? 0} buffered</span>
      </div>
      <div class="button-row">
        <button id="refreshReceived" class="secondary">Refresh</button>
        <button id="clearReceived" class="secondary">Clear</button>
        <button id="exportReceived">Export TXT</button>
      </div>
      <div class="table-wrap">
        <table>
          <thead>
            <tr><th>Index</th><th>Timestamp</th><th>Hex</th><th>ASCII</th></tr>
          </thead>
          <tbody>${rows}</tbody>
        </table>
      </div>
    </section>
  `;
}
