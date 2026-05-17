import { apiClient, type MessageDescription, type PortStatus, type ReceivedResponse, type TestInfo, type TestStatus } from "./api/client";
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
}

const state: AppState = {
  status: null,
  messages: [],
  received: null,
  tests: [],
  testStatus: null,
  logs: [],
  notice: ""
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
      ${messageView(state.messages)}
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

async function refreshSoft(): Promise<void> {
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
  render();
}

async function loadInitial(): Promise<void> {
  try {
    const [messages, tests] = await Promise.all([apiClient.messages(), apiClient.tests()]);
    state.messages = messages.messages;
    state.tests = tests.tests;
    await refreshSoft();
    state.notice = "Backend connected";
    render();
  } catch (error) {
    state.notice = error instanceof Error ? error.message : "Backend is not reachable";
    render();
  }
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
      const result = await apiClient.sendMessage(readMessageForm());
      const frame = document.querySelector<HTMLParagraphElement>("#lastFrame");
      if (frame) frame.textContent = result.frameHex;
      await refreshSoft();
      setNotice("Frame sent");
    } catch (error) {
      setNotice(error instanceof Error ? error.message : "Send failed");
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
  refreshSoft().catch(() => {
    state.notice = "Backend is not reachable";
    render();
  });
}, 1500);
