import { apiClient, getApiBase, type MessageDescription, type PortStatus, type ReceivedResponse, type TestInfo, type TestStatus } from "./api/client";
import { connectionView, readConnectionForm } from "./views/connectionView";
import { messageView, readMessageForm } from "./views/messageView";
import { receiveView } from "./views/receiveView";
import { readTestForm, testView } from "./views/testView";
import "./styles.css";

const app = document.querySelector<HTMLDivElement>("#app");

interface AppState {
  status: PortStatus | null;
  messages: MessageDescription[];
  received: ReceivedResponse | null;
  tests: TestInfo[];
  testStatus: TestStatus | null;
  logs: string[];
  notice: string;
  selectedMessageType: string;
}

const state: AppState = {
  status: null,
  messages: [],
  received: null,
  tests: [],
  testStatus: null,
  logs: [],
  notice: "",
  selectedMessageType: "pwm"
};

function render(): void {
  if (!app) return;
  app.innerHTML = `
    <header class="topbar">
      <div>
        <h1>USART Test Console</h1>
        <p>C++ backend API üzerinden paket gönderme, canlı okuma, kayıt ve test yönetimi.</p>
      </div>
      <span class="notice">${state.notice}</span>
    </header>
    <div class="layout">
      ${connectionView(state.status)}
      ${messageView(state.messages, state.selectedMessageType)}
      ${testView(state.tests, state.testStatus)}
      ${receiveView(state.received)}
      <section class="panel wide">
        <div class="panel-header">
          <h2>Logs</h2>
          <span class="status idle">${state.logs.length} entries</span>
        </div>
        <pre class="log">${state.logs.join("\n")}</pre>
      </section>
    </div>
  `;
  bindEvents();
}

function setNotice(message: string): void {
  state.notice = message;
  render();
}

function canAutoRender(): boolean {
  const active = document.activeElement;
  if (!(active instanceof HTMLElement)) {
    return true;
  }

  return !active.closest("select, input, textarea");
}

function commandMessages(): MessageDescription[] {
  return state.messages.filter((message) => message.direction !== "telemetry");
}

async function loadMessages(): Promise<void> {
  const messages = await apiClient.messages();
  state.messages = messages.messages;

  const commands = commandMessages();
  if (!commands.some((message) => message.type === state.selectedMessageType)) {
    state.selectedMessageType = commands[0]?.type ?? "";
  }
}

async function refreshSoft(options: { render?: boolean } = {}): Promise<void> {
  const [status, received, logs, testStatus] = await Promise.all([
    apiClient.portStatus(),
    apiClient.received(),
    apiClient.logs(),
    apiClient.testStatus()
  ]);
  state.status = status;
  state.received = received;
  state.logs = logs.logs;
  state.testStatus = testStatus;
  if (options.render ?? true) {
    render();
  }
}

async function loadInitial(): Promise<void> {
  let connected = false;

  try {
    await loadMessages();
    connected = true;
  } catch (error) {
    state.notice = error instanceof Error ? `Messages not loaded from ${getApiBase()}: ${error.message}` : "Messages not loaded";
  }

  try {
    const tests = await apiClient.tests();
    state.tests = tests.tests;
    connected = true;
  } catch {
    state.tests = [];
  }

  try {
    await refreshSoft();
    connected = true;
  } catch (error) {
    if (!state.notice) {
      state.notice = error instanceof Error ? error.message : "Backend is not reachable";
    }
  }

  if (connected && state.messages.length > 0) {
    state.notice = `Backend connected: ${getApiBase()}`;
  }
  render();
}

function bindEvents(): void {
  document.querySelector("#openPort")?.addEventListener("click", async () => {
    try {
      await apiClient.openPort(readConnectionForm());
      await refreshSoft();
      setNotice("Connection opened");
    } catch (error) {
      setNotice(error instanceof Error ? error.message : "Open failed");
    }
  });

  document.querySelector("#closePort")?.addEventListener("click", async () => {
    await apiClient.closePort();
    await refreshSoft();
    setNotice("Connection closed");
  });

  document.querySelector("#sendMessage")?.addEventListener("click", async () => {
    try {
      const result = await apiClient.sendMessage(readMessageForm(state.messages, state.selectedMessageType));
      const frame = document.querySelector<HTMLParagraphElement>("#lastFrame");
      if (frame) frame.textContent = result.frameHex;
      await refreshSoft();
      setNotice("Frame sent");
    } catch (error) {
      setNotice(error instanceof Error ? error.message : "Send failed");
    }
  });

  document.querySelector("#messageType")?.addEventListener("change", (event) => {
    state.selectedMessageType = (event.target as HTMLSelectElement).value;
    render();
  });

  document.querySelector("#reloadMessages")?.addEventListener("click", async () => {
    try {
      await loadMessages();
      setNotice(`Messages loaded from ${getApiBase()}`);
    } catch (error) {
      setNotice(error instanceof Error ? `Messages not loaded: ${error.message}` : "Messages not loaded");
    }
  });

  document.querySelector("#refreshReceived")?.addEventListener("click", refreshSoft);

  document.querySelector("#clearReceived")?.addEventListener("click", async () => {
    await apiClient.clearReceived();
    await refreshSoft();
    setNotice("Received buffer cleared");
  });

  document.querySelector("#exportReceived")?.addEventListener("click", async () => {
    const result = await apiClient.exportReceived();
    await refreshSoft();
    setNotice(`Exported: ${result.path}`);
  });

  document.querySelector("#runTest")?.addEventListener("click", async () => {
    const result = await apiClient.runTest(readTestForm());
    await refreshSoft();
    setNotice(result.message);
  });

  document.querySelector("#stopTest")?.addEventListener("click", async () => {
    const result = await apiClient.stopTest();
    await refreshSoft();
    setNotice(result.message);
  });
}

loadInitial();
window.setInterval(() => {
  refreshSoft({ render: canAutoRender() }).catch(() => {
    state.notice = `Backend is not reachable: ${getApiBase()}`;
    if (canAutoRender()) {
      render();
    }
  });
}, 1500);
