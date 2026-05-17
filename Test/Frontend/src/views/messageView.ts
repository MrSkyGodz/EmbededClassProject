import type { MessageDescription } from "../api/client";

export function messageView(messages: MessageDescription[], selectedType: string): string {
  const commandMessages = messages.filter((message) => message.direction !== "telemetry");
  const selectedMessage = commandMessages.find((message) => message.type === selectedType) ?? commandMessages[0];
  const options = commandMessages
    .map((message) => `<option value="${message.type}" ${message.type === selectedMessage?.type ? "selected" : ""}>${message.type}</option>`)
    .join("");
  const fieldInputs = selectedMessage
    ? selectedMessage.fields
        .map(
          (field) => `
            <label>
              ${field.name}
              <input
                id="messageField_${field.name}"
                type="number"
                min="${field.min}"
                max="${field.max}"
                value="${defaultValue(field.name, field.min, field.max)}"
              />
            </label>
          `
        )
        .join("")
    : "";

  return `
    <section class="panel">
      <div class="panel-header">
        <h2>Send Message</h2>
        <span class="status idle">ICD v1</span>
      </div>
      <div class="form-grid">
        <label>
          Type
          <select id="messageType">${options}</select>
        </label>
        ${fieldInputs}
      </div>
      <button id="sendMessage">Send</button>
      <p id="lastFrame" class="mono"></p>
    </section>
  `;
}

export function readMessageForm(messages: MessageDescription[], selectedType: string): { type: string; payload: Record<string, number> } {
  const type = document.querySelector<HTMLSelectElement>("#messageType")?.value ?? selectedType;
  const selectedMessage = messages.find((message) => message.type === type && message.direction !== "telemetry");
  const payload: Record<string, number> = {};
  for (const field of selectedMessage?.fields ?? []) {
    payload[field.name] = Number(document.querySelector<HTMLInputElement>(`#messageField_${field.name}`)?.value || "0");
  }
  return { type, payload };
}

function defaultValue(name: string, min: number, max: number): number {
  if (name === "pwm") return 128;
  if (name === "motor1AngleDeg") return 90;
  if (name === "motor2AngleDeg") return 45;
  return Math.round((min + max) / 2);
}
