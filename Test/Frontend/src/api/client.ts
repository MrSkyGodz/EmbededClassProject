export type TransportMode = "mock" | "serial";
export type TestSource = "mock" | "serial" | "received_snapshot" | "received_live";

export interface PortStatus {
  open: boolean;
  mode: string;
  port: string;
  baud: number;
  readerRunning: boolean;
  receivedBytes: number;
  parsedMessages: number;
}

export interface MessageField {
  name: string;
  type: string;
  min: number;
  max: number;
}

export interface MessageDescription {
  type: string;
  icdType: number;
  direction: "command" | "telemetry";
  fields: MessageField[];
}

export interface ReceivedEntry {
  index: number;
  timestampMs: number;
  hex: string;
  ascii: string;
}

export interface ReceivedResponse {
  totalReceived: number;
  buffered: number;
  entries: ReceivedEntry[];
  parsedTotal: number;
  parsedMessages: ParsedMessage[];
}

export interface ParsedMessage {
  index: number;
  timestampMs: number;
  timetagMs: number;
  counter: number;
  icdType: number;
  type: string;
  fields: Record<string, number>;
}

export interface TestInfo {
  id: string;
  name: string;
  source: TestSource;
}

export interface TestStatus {
  id: string;
  state: string;
  passed: boolean;
  message: string;
}

const API_BASE = "http://localhost:8081";

async function request<T>(path: string, init?: RequestInit): Promise<T> {
  const response = await fetch(`${API_BASE}${path}`, {
    headers: { "Content-Type": "application/json" },
    ...init
  });
  const data = (await response.json()) as T & { error?: string };
  if (!response.ok) {
    throw new Error(data.error ?? `Request failed: ${response.status}`);
  }
  return data;
}

export const apiClient = {
  health: () => request<{ ok: boolean; service: string }>("/api/health"),
  messages: () => request<{ messages: MessageDescription[] }>("/api/messages"),
  openPort: (payload: { mode: TransportMode; port: string; baud: number }) =>
    request<{ ok: boolean } & PortStatus>("/api/ports/open", {
      method: "POST",
      body: JSON.stringify(payload)
    }),
  closePort: () =>
    request<{ ok: boolean }>("/api/ports/close", {
      method: "POST",
      body: "{}"
    }),
  portStatus: () => request<PortStatus>("/api/ports/status"),
  sendMessage: (payload: { type: string; payload: Record<string, number> }) =>
    request<{ ok: boolean; frameHex: string }>("/api/messages/send", {
      method: "POST",
      body: JSON.stringify(payload)
    }),
  received: () => request<ReceivedResponse>("/api/received"),
  clearReceived: () =>
    request<{ ok: boolean }>("/api/received/clear", {
      method: "POST",
      body: "{}"
    }),
  exportReceived: () =>
    request<{ ok: boolean; path: string }>("/api/received/export", {
      method: "POST",
      body: "{}"
    }),
  logs: () => request<{ logs: string[] }>("/api/logs"),
  tests: () => request<{ tests: TestInfo[] }>("/api/tests"),
  runTest: (payload: { testId: string; source: TestSource }) =>
    request<{ ok: boolean; state: string; message: string }>("/api/tests/run", {
      method: "POST",
      body: JSON.stringify(payload)
    }),
  stopTest: () =>
    request<{ ok: boolean; state: string; message: string }>("/api/tests/stop", {
      method: "POST",
      body: "{}"
    }),
  testStatus: () => request<TestStatus>("/api/tests/status")
};
