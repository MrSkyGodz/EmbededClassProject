import type { MessageDescription } from "../api/client";

export function messageView(messages: MessageDescription[]): string {
  const options = messages.map((message) => `<option value="${message.type}">${message.type}</option>`).join("");
  return `
    <section class="panel">
      <div class="panel-header">
        <h2>Send Message</h2>
        <span class="status idle">USART frame</span>
      </div>
      <div class="form-grid">
        <label>
          Type
          <select id="messageType">${options}</select>
        </label>
        <label>
          PWM
          <input id="pwm" type="number" min="0" max="255" value="128" />
        </label>
        <label>
          Motor 1
          <input id="motor1AngleDeg" type="number" min="0" max="180" value="90" />
        </label>
        <label>
          Motor 2
          <input id="motor2AngleDeg" type="number" min="0" max="180" value="45" />
        </label>
      </div>
      <button id="sendMessage">Send</button>
      <p id="lastFrame" class="mono"></p>
    </section>
  `;
}

export function readMessageForm(): { type: string; payload: Record<string, number> } {
  const type = document.querySelector<HTMLSelectElement>("#messageType")?.value ?? "pwm";
  const payload: Record<string, number> = {};
  if (type === "pwm") {
    payload.pwm = Number(document.querySelector<HTMLInputElement>("#pwm")?.value || "0");
  } else {
    payload.motor1AngleDeg = Number(document.querySelector<HTMLInputElement>("#motor1AngleDeg")?.value || "0");
    payload.motor2AngleDeg = Number(document.querySelector<HTMLInputElement>("#motor2AngleDeg")?.value || "0");
  }
  return { type, payload };
}
