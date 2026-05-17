import type { ReceivedResponse } from "../api/client";

export function receiveView(received: ReceivedResponse | null): string {
  const parsedRows = (received?.parsedMessages ?? [])
    .slice(-60)
    .map(
      (message) => `
        <tr>
          <td>${message.index}</td>
          <td>${message.timestampMs}</td>
          <td>${message.timetagMs}</td>
          <td>${message.counter}</td>
          <td>${message.icdType}</td>
          <td>${message.type}</td>
          <td class="mono">${formatFields(message.fields)}</td>
        </tr>
      `
    )
    .join("");

  return `
    <section class="panel wide">
      <div class="panel-header">
        <h2>Received Data</h2>
        <span class="status idle">${received?.parsedTotal ?? 0} parsed</span>
      </div>
      <div class="button-row">
        <button id="refreshReceived" class="secondary">Refresh</button>
        <button id="clearReceived" class="secondary">Clear</button>
        <button id="exportReceived">Export TXT</button>
      </div>
      <div class="table-wrap">
        <h3>Parsed ICD Messages</h3>
        <table>
          <thead>
            <tr><th>Index</th><th>Timestamp</th><th>Timetag</th><th>Counter</th><th>ICD</th><th>Type</th><th>Fields</th></tr>
          </thead>
          <tbody>${parsedRows}</tbody>
        </table>
      </div>
    </section>
  `;
}

function formatFields(fields: Record<string, number>): string {
  return Object.entries(fields)
    .map(([name, value]) => `${name}=${Number(value).toFixed(2)}`)
    .join("; ");
}
