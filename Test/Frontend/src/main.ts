import {
  apiClient,
  getApiBase,
  getEventStreamUrl,
  type MessageDescription,
  type ParsedMessage,
  type PortStatus,
  type ReceivedResponse,
  type TestInfo,
  type TestSource,
  type TestStatus
} from "./api/client";

import { connectionView, readConnectionForm } from "./views/connectionView";
import { messageView, readMessageForm } from "./views/messageView";
import { receiveView } from "./views/receiveView";
import { readTestRunForm, testView, type TestCardsState } from "./views/testView";
import "./styles.css";

const app = document.querySelector<HTMLDivElement>("#app");

const MAX_LOCAL_PARSED_MESSAGES = 60;
const MAX_LOCAL_LOGS = 80;

interface AppState {
  status: PortStatus | null;
  messages: MessageDescription[];
  received: ReceivedResponse | null;
  tests: TestInfo[];
  testStatus: TestStatus | null;
  logs: string[];
  notice: string;
  selectedMessageType: string;
  selectedTestIds: string[];
  expandedTestParamsById: Record<string, boolean>;
  testParamsById: Record<string, Record<string, number>>;
  testSourceById: Record<string, TestSource>;
  testStatusesById: Record<string, TestStatus>;
  batchRunning: boolean;
  batchStopRequested: boolean;
}

const state: AppState = {
  status: null,
  messages: [],
  received: null,
  tests: [],
  testStatus: null,
  logs: [],
  notice: "",
  selectedMessageType: "pwm",
  selectedTestIds: [],
  expandedTestParamsById: {},
  testParamsById: {},
  testSourceById: {},
  testStatusesById: {},
  batchRunning: false,
  batchStopRequested: false
};

let liveRefreshRunning = false;
let eventSource: EventSource | null = null;

function renderShell(): void {
  if (!app) return;

  app.innerHTML = `
    <header class="topbar">
      <div>
        <h1>USART Test Console</h1>
        <p>C++ backend API üzerinden paket gönderme, canlı okuma, kayıt ve test yönetimi.</p>
      </div>
      <span id="notice" class="notice">${state.notice}</span>
    </header>

    <div class="layout">
      <div id="connectionPanel"></div>
      <div id="messagePanel"></div>
      <div id="testPanel"></div>
      <div id="receivedPanel"></div>
      <div id="logsPanel"></div>
    </div>
  `;
}

function setNotice(message: string): void {
  state.notice = message;

  const notice = document.querySelector<HTMLSpanElement>("#notice");
  if (notice) {
    notice.textContent = message;
  }
}

function setPanelHtml(panelId: string, html: string): void {
  const panel = document.querySelector<HTMLDivElement>(`#${panelId}`);
  if (panel) {
    panel.innerHTML = html;
  }
}

function commandMessages(): MessageDescription[] {
  return state.messages.filter((message) => message.direction !== "telemetry");
}

function testCardsState(): TestCardsState {
  return {
    selectedIds: state.selectedTestIds,
    expandedParamsById: state.expandedTestParamsById,
    paramsById: state.testParamsById,
    sourceById: state.testSourceById,
    statusesById: state.testStatusesById
  };
}

function defaultTestParams(test: TestInfo): Record<string, number> {
  const params: Record<string, number> = {};
  for (const parameter of test.parameters ?? []) {
    params[parameter.name] = parameter.defaultValue;
  }
  return params;
}

function mergeTestMetadata(tests: TestInfo[]): void {
  const ids = new Set(tests.map((test) => test.id));
  state.selectedTestIds = state.selectedTestIds.filter((id) => ids.has(id));

  if (state.selectedTestIds.length === 0) {
    state.selectedTestIds = tests.map((test) => test.id);
  }

  for (const test of tests) {
    state.testParamsById[test.id] = {
      ...defaultTestParams(test),
      ...(state.testParamsById[test.id] ?? {})
    };
    const sources = test.allowedSources?.length ? test.allowedSources : [test.source];
    if (!sources.includes(state.testSourceById[test.id])) {
      state.testSourceById[test.id] = test.source;
    }
  }

  for (const id of Object.keys(state.testParamsById)) {
    if (!ids.has(id)) delete state.testParamsById[id];
  }
  for (const id of Object.keys(state.expandedTestParamsById)) {
    if (!ids.has(id)) delete state.expandedTestParamsById[id];
  }
  for (const id of Object.keys(state.testSourceById)) {
    if (!ids.has(id)) delete state.testSourceById[id];
  }
  for (const id of Object.keys(state.testStatusesById)) {
    if (!ids.has(id)) delete state.testStatusesById[id];
  }
}

function renderConnectionPanel(): void {
  setPanelHtml("connectionPanel", connectionView(state.status));
  bindConnectionEvents();
}

function renderMessagePanel(): void {
  setPanelHtml("messagePanel", messageView(state.messages, state.selectedMessageType));
  bindMessageEvents();
}

function renderTestPanel(): void {
  setPanelHtml("testPanel", testView(state.tests, state.testStatus, testCardsState()));
  bindTestEvents();
}

function renderReceivedPanel(): void {
  setPanelHtml("receivedPanel", receiveView(state.received));

  const tableWrap = document.querySelector<HTMLDivElement>("#receivedPanel .table-wrap");
  if (tableWrap) {
    tableWrap.scrollTop = tableWrap.scrollHeight;
  }

  bindReceivedEvents();
}

function renderLogsPanel(): void {
  setPanelHtml(
    "logsPanel",
    `
      <section class="panel wide">
        <div class="panel-header">
          <h2>Logs</h2>
          <span class="status idle">${state.logs.length} entries</span>
        </div>
        <pre id="logBox" class="log">${state.logs.join("\n")}</pre>
      </section>
    `
  );

  const logBox = document.querySelector<HTMLPreElement>("#logBox");
  if (logBox) {
    logBox.scrollTop = logBox.scrollHeight;
  }
}

async function loadMessages(): Promise<void> {
  const messages = await apiClient.messages();
  state.messages = messages.messages;

  const commands = commandMessages();

  if (!commands.some((message) => message.type === state.selectedMessageType)) {
    state.selectedMessageType = commands[0]?.type ?? "";
  }
}

async function loadTests(): Promise<void> {
  const tests = await apiClient.tests();
  state.tests = tests.tests;
  mergeTestMetadata(state.tests);
}

async function refreshConnectionOnly(): Promise<void> {
  state.status = await apiClient.portStatus();
  renderConnectionPanel();
}

async function refreshTestStatusOnly(): Promise<void> {
  state.testStatus = await apiClient.testStatus();
  if (state.testStatus.id !== "none") {
    state.testStatusesById[state.testStatus.id] = state.testStatus;
  }
  renderTestPanel();
}

async function refreshReceivedAndLogsOnly(): Promise<void> {
  if (liveRefreshRunning) {
    return;
  }

  liveRefreshRunning = true;

  try {
    const [received, logs] = await Promise.all([
      apiClient.received(0, MAX_LOCAL_PARSED_MESSAGES),
      apiClient.logs(MAX_LOCAL_LOGS)
    ]);

    state.received = {
      ...received,
      entries: [],
      parsedMessages: received.parsedMessages.slice(-MAX_LOCAL_PARSED_MESSAGES)
    };

    state.logs = logs.logs.slice(-MAX_LOCAL_LOGS);

    renderReceivedPanel();
    renderLogsPanel();
  } finally {
    liveRefreshRunning = false;
  }
}

async function loadInitial(): Promise<void> {
  renderShell();

  let connected = false;

  try {
    await loadMessages();
    connected = true;
  } catch (error) {
    setNotice(
      error instanceof Error
        ? `Messages not loaded from ${getApiBase()}: ${error.message}`
        : "Messages not loaded"
    );
  }

  try {
    await loadTests();
    connected = true;
  } catch {
    state.tests = [];
  }

  try {
    const [status, received, logs, testStatus] = await Promise.all([
      apiClient.portStatus(),
      apiClient.received(0, MAX_LOCAL_PARSED_MESSAGES),
      apiClient.logs(MAX_LOCAL_LOGS),
      apiClient.testStatus()
    ]);

    state.status = status;
    state.received = {
      ...received,
      entries: [],
      parsedMessages: received.parsedMessages.slice(-MAX_LOCAL_PARSED_MESSAGES)
    };
    state.logs = logs.logs.slice(-MAX_LOCAL_LOGS);
    state.testStatus = testStatus;

    connected = true;
  } catch (error) {
    if (!state.notice) {
      setNotice(error instanceof Error ? error.message : "Backend is not reachable");
    }
  }

  renderConnectionPanel();
  renderMessagePanel();
  renderTestPanel();
  renderReceivedPanel();
  renderLogsPanel();

  if (connected && state.messages.length > 0) {
    setNotice(`Backend connected: ${getApiBase()}`);
  }

  startEventStream();
}

function startEventStream(): void {
  if (eventSource) {
    eventSource.close();
    eventSource = null;
  }

  const url = getEventStreamUrl();
  eventSource = new EventSource(url);

  eventSource.addEventListener("open", () => {
    setNotice(`Live stream connected: ${getApiBase()}`);
  });

  eventSource.addEventListener("parsed", (event) => {
    try {
      const message = JSON.parse((event as MessageEvent).data) as ParsedMessage;
      appendParsedMessage(message);
    } catch {
      setNotice("Live parsed message could not be decoded");
    }
  });

  eventSource.addEventListener("log", (event) => {
    try {
      const data = JSON.parse((event as MessageEvent).data) as { message?: string };

      if (typeof data.message === "string") {
        appendLog(data.message);
      }
    } catch {
      const raw = (event as MessageEvent).data;
      if (raw) {
        appendLog(raw);
      }
    }
  });

  eventSource.addEventListener("status", (event) => {
    try {
      state.status = JSON.parse((event as MessageEvent).data) as PortStatus;
      updateConnectionStatusIndicators();
    } catch {
      setNotice("Live status message could not be decoded");
    }
  });

  eventSource.onerror = () => {
    setNotice(`Live stream disconnected. Browser will retry: ${getApiBase()}`);
  };
}

function appendParsedMessage(message: ParsedMessage): void {
  const previous = state.received?.parsedMessages ?? [];
  const merged = [...previous, message].slice(-MAX_LOCAL_PARSED_MESSAGES);

  state.received = {
    totalReceived: state.received?.totalReceived ?? 0,
    buffered: state.received?.buffered ?? 0,
    entries: [],
    parsedTotal: Math.max(state.received?.parsedTotal ?? 0, message.index + 1),
    parsedMessages: merged
  };

  renderReceivedPanel();
}

function appendLog(message: string): void {
  state.logs = [...state.logs, message].slice(-MAX_LOCAL_LOGS);
  renderLogsPanel();
}

function bindConnectionEvents(): void {
  document.querySelector("#openPort")?.addEventListener("click", async () => {
    try {
      const form = readConnectionForm();

      await apiClient.openPort(form);

      state.status = await apiClient.portStatus();

      if (state.status) {
        state.status.mode = form.mode;
        state.status.port = form.port;
        state.status.baud = form.baud;
        state.status.open = true;
      }

      updateConnectionStatusIndicators();
      setNotice("Connection opened");
    } catch (error) {
      setNotice(error instanceof Error ? error.message : "Open failed");
    }
  });

  document.querySelector("#closePort")?.addEventListener("click", async () => {
    try {
      await apiClient.closePort();

      if (state.status) {
        state.status.open = false;
        state.status.readerRunning = false;
      }

      updateConnectionStatusIndicators();
      setNotice("Connection closed");
    } catch (error) {
      setNotice(error instanceof Error ? error.message : "Close failed");
    }
  });

  document.querySelector("#refreshConnection")?.addEventListener("click", async () => {
    try {
      await refreshConnectionOnly();
      setNotice("Connection refreshed");
    } catch (error) {
      setNotice(error instanceof Error ? error.message : "Connection refresh failed");
    }
  });
}

function bindMessageEvents(): void {
  document.querySelector("#sendMessage")?.addEventListener("click", async () => {
    try {
      const form = readMessageForm(state.messages, state.selectedMessageType);
      const result = await apiClient.sendMessage(form);

      const frame = document.querySelector<HTMLParagraphElement>("#lastFrame");
      if (frame) {
        frame.textContent = result.frameHex;
      }

      setNotice("Frame sent");
    } catch (error) {
      setNotice(error instanceof Error ? error.message : "Send failed");
    }
  });

  document.querySelector("#messageType")?.addEventListener("change", (event) => {
    state.selectedMessageType = (event.target as HTMLSelectElement).value;
    renderMessagePanel();
  });

  document.querySelector("#reloadMessages")?.addEventListener("click", async () => {
    try {
      await loadMessages();
      renderMessagePanel();
      setNotice(`Messages loaded from ${getApiBase()}`);
    } catch (error) {
      setNotice(error instanceof Error ? `Messages not loaded: ${error.message}` : "Messages not loaded");
    }
  });
}

function bindReceivedEvents(): void {
  document.querySelector("#refreshReceived")?.addEventListener("click", async () => {
    try {
      await refreshReceivedAndLogsOnly();
      setNotice("Received data refreshed");
    } catch (error) {
      setNotice(error instanceof Error ? error.message : "Received refresh failed");
    }
  });

  document.querySelector("#clearReceived")?.addEventListener("click", async () => {
    try {
      await apiClient.clearReceived();

      state.received = {
        totalReceived: 0,
        buffered: 0,
        entries: [],
        parsedTotal: 0,
        parsedMessages: []
      };

      renderReceivedPanel();
      setNotice("Received buffer cleared");
    } catch (error) {
      setNotice(error instanceof Error ? error.message : "Clear received failed");
    }
  });

  document.querySelector("#exportReceived")?.addEventListener("click", async () => {
    try {
      const result = await apiClient.exportReceived();
      setNotice(`Exported: ${result.path}`);
    } catch (error) {
      setNotice(error instanceof Error ? error.message : "Export failed");
    }
  });
}

async function waitForTestFinish(testId: string): Promise<TestStatus> {
  while (true) {
    const status = await apiClient.testStatus();
    state.testStatus = status;
    if (status.id === testId) {
      state.testStatusesById[testId] = status;
      renderTestPanel();

      if (status.state !== "running") {
        return status;
      }
    }

    await new Promise((resolve) => window.setTimeout(resolve, 250));
  }
}

async function runSingleTest(test: TestInfo): Promise<TestStatus> {
  const result = await apiClient.runTest(readTestRunForm(test, state.testParamsById, state.testSourceById));
  state.testStatus = {
    id: result.id,
    state: result.state,
    passed: result.ok && result.state !== "running",
    message: result.message
  };
  state.testStatusesById[test.id] = state.testStatus;
  renderTestPanel();

  if (result.state === "running") {
    return waitForTestFinish(test.id);
  }

  return state.testStatus;
}

async function runSelectedTests(): Promise<void> {
  if (state.batchRunning) return;

  state.batchRunning = true;
  state.batchStopRequested = false;
  const selected = state.tests.filter((test) => state.selectedTestIds.includes(test.id));

  try {
    for (const test of selected) {
      if (state.batchStopRequested) {
        break;
      }

      setNotice(`Running ${test.name}`);
      const status = await runSingleTest(test);
      if (state.batchStopRequested || status.state === "stopped") {
        break;
      }

    }
    setNotice(state.batchStopRequested ? "Selected test run stopped" : "Selected tests finished");
  } catch (error) {
    setNotice(error instanceof Error ? error.message : "Selected test run failed");
  } finally {
    state.batchRunning = false;
    state.batchStopRequested = false;
    renderTestPanel();
  }
}

function bindTestEvents(): void {
  document.querySelectorAll<HTMLElement>("[data-test-card]").forEach((card) => {
    card.addEventListener("click", (event) => {
      if ((event.target as HTMLElement).closest("[data-test-control]")) {
        return;
      }

      const testId = card.dataset.testCard;
      if (!testId) return;

      state.selectedTestIds = state.selectedTestIds.includes(testId)
        ? state.selectedTestIds.filter((id) => id !== testId)
        : [...state.selectedTestIds, testId];
      renderTestPanel();
    });
  });

  document.querySelectorAll<HTMLInputElement>("[data-test-param]").forEach((input) => {
    input.addEventListener("input", () => {
      const [testId, name] = (input.dataset.testParam ?? "").split(":");
      if (!testId || !name) return;

      state.testParamsById[testId] = {
        ...(state.testParamsById[testId] ?? {}),
        [name]: Number(input.value)
      };
    });
  });

  document.querySelectorAll<HTMLSelectElement>("[id^='testSource_']").forEach((select) => {
    select.addEventListener("change", () => {
      const testId = select.id.replace("testSource_", "");
      state.testSourceById[testId] = select.value as TestSource;
    });
  });

  document.querySelectorAll<HTMLButtonElement>("[data-toggle-params]").forEach((button) => {
    button.addEventListener("click", () => {
      const testId = button.dataset.toggleParams;
      if (!testId) return;

      state.expandedTestParamsById[testId] = !state.expandedTestParamsById[testId];
      renderTestPanel();
    });
  });

  document.querySelectorAll<HTMLButtonElement>("[data-run-test]").forEach((button) => {
    button.addEventListener("click", async () => {
      const test = state.tests.find((item) => item.id === button.dataset.runTest);
      if (!test) return;

      await runSingleTest(test);
    });
  });

  document.querySelector("#runSelectedTests")?.addEventListener("click", async () => {
    await runSelectedTests();
  });

  document.querySelector("#stopTest")?.addEventListener("click", async () => {
    try {
      state.batchStopRequested = true;
      const result = await apiClient.stopTest();
      await refreshTestStatusOnly();
      if (result.id) {
        state.testStatusesById[result.id] = result;
      }
      setNotice(result.message);
    } catch (error) {
      setNotice(error instanceof Error ? error.message : "Stop test failed");
    }
  });

  document.querySelector("#reloadTests")?.addEventListener("click", async () => {
    try {
      await loadTests();
      renderTestPanel();
      setNotice(`Tests loaded from ${getApiBase()}`);
    } catch (error) {
      setNotice(error instanceof Error ? `Tests not loaded: ${error.message}` : "Tests not loaded");
    }
  });
}

function updateConnectionStatusIndicators(): void {
  const isOpen = state.status?.open ?? false;

  const statusBadge = document.querySelector<HTMLSpanElement>(
    "#connectionPanel .panel-header .status"
  );

  if (statusBadge) {
    statusBadge.textContent = isOpen ? "Open" : "Closed";
    statusBadge.className = `status ${isOpen ? "ok" : "idle"}`;
  }

  const metricValues = document.querySelectorAll<HTMLElement>(
    "#connectionPanel .metrics dd"
  );

  if (metricValues.length >= 3) {
    metricValues[0].textContent = state.status?.mode ?? "none";
    metricValues[1].textContent = state.status?.readerRunning ? "running" : "stopped";
    metricValues[2].textContent = `${state.status?.receivedBytes ?? 0} bytes`;
  }
}

loadInitial();
window.setInterval(() => {
  if (state.testStatus?.state === "running") {
    refreshTestStatusOnly().catch(() => {
      setNotice("Test status refresh failed");
    });
  }
}, 1000);
