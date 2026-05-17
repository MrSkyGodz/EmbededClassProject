export type TransportMode = "mock" | "serial";
export type TestSource = "received_snapshot" | "received_live";

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

export interface ParsedMessage {
  index: number;
  timestampMs: number;
  timetagMs: number;
  counter: number;
  icdType: number;
  type: string;
  fields: Record<string, number>;
}

export interface ReceivedResponse {
  totalReceived: number;
  buffered: number;
  entries: ReceivedEntry[];
  parsedTotal: number;
  parsedMessages: ParsedMessage[];
}

export interface TestInfo {
  id: string;
  name: string;
  source: TestSource;
  allowedSources?: TestSource[];
  parameters?: TestParameter[];
}

export interface TestParameter {
  name: string;
  label: string;
  type: string;
  defaultValue: number;
  min: number;
  max: number;
}

export interface TestStatus {
  id: string;
  state: string;
  passed: boolean;
  message: string;
}

const CONFIGURED_API_BASE = import.meta.env.VITE_API_BASE;

const API_BASE_CANDIDATES = Array.from(
  new Set(
    [CONFIGURED_API_BASE, "http://localhost:8081", "http://localhost:8080"].filter(
      (base): base is string => Boolean(base)
    )
  )
);

let activeApiBase = API_BASE_CANDIDATES[0];

export function getApiBase(): string {
  return activeApiBase;
}

export function getEventStreamUrl(): string {
  return `${activeApiBase}/api/events`;
}

async function request<T>(path: string, init?: RequestInit): Promise<T> {
  let lastNetworkError: unknown = null;
  const bases = [
    activeApiBase,
    ...API_BASE_CANDIDATES.filter((base) => base !== activeApiBase)
  ];

  for (const base of bases) {
    try {
      const response = await fetch(`${base}${path}`, {
        headers: { "Content-Type": "application/json" },
        ...init
      });

      const data = (await response.json()) as T & { error?: string };

      if (!response.ok) {
        throw new Error(data.error ?? `Request failed: ${response.status}`);
      }

      activeApiBase = base;
      return data;
    } catch (error) {
      lastNetworkError = error;

      if (error instanceof TypeError) {
        continue;
      }

      throw error;
    }
  }

  throw lastNetworkError instanceof Error
    ? lastNetworkError
    : new Error("Backend is not reachable");
}

export const apiClient = {
  health: () =>
    request<{ ok: boolean; service: string }>("/api/health"),

  messages: () =>
    request<{ messages: MessageDescription[] }>("/api/messages"),

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

  portStatus: () =>
    request<PortStatus>("/api/ports/status"),

  sendMessage: (payload: { type: string; payload: Record<string, number> }) =>
    request<{ ok: boolean; frameHex: string }>("/api/messages/send", {
      method: "POST",
      body: JSON.stringify(payload)
    }),

  received: (entryLimit = 0, parsedLimit = 60) =>
    request<ReceivedResponse>(
      `/api/received?entryLimit=${entryLimit}&parsedLimit=${parsedLimit}`
    ),

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

  logs: (limit = 80) =>
    request<{ logs: string[] }>(
      `/api/logs?limit=${limit}`
    ),

  tests: () =>
    request<{ tests: TestInfo[] }>("/api/tests"),

  runTest: (payload: { testId: string; source: TestSource; params: Record<string, number> }) =>
    request<{ ok: boolean; id: string; state: string; message: string }>("/api/tests/run", {
      method: "POST",
      body: JSON.stringify(payload)
    }),

  stopTest: () =>
    request<{ ok: boolean; id: string; state: string; message: string }>("/api/tests/stop", {
      method: "POST",
      body: "{}"
    }),

  testStatus: () =>
    request<TestStatus>("/api/tests/status")
};
