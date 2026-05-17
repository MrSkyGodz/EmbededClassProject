import type { TestInfo, TestSource, TestStatus } from "../api/client";

const SOURCE_LABELS: Record<TestSource, string> = {
  received_snapshot: "Received snapshot",
  received_live: "Received live"
};

export interface TestCardsState {
  selectedIds: string[];
  expandedParamsById: Record<string, boolean>;
  paramsById: Record<string, Record<string, number>>;
  sourceById: Record<string, TestSource>;
  statusesById: Record<string, TestStatus>;
}

export function testView(tests: TestInfo[], globalStatus: TestStatus | null, cardsState: TestCardsState): string {
  const selectedCount = cardsState.selectedIds.length;
  const cards = tests.map((test) => testCard(test, cardsState, globalStatus)).join("");

  return `
    <section class="panel wide tests-panel">
      <div class="panel-header">
        <h2>Tests</h2>
        <span class="status idle">${selectedCount}/${tests.length} selected</span>
      </div>
      <div class="button-row">
        <button id="runSelectedTests" ${selectedCount > 0 ? "" : "disabled"}>Run Selected</button>
        <button id="stopTest" class="secondary">Stop</button>
        <button id="reloadTests" class="secondary">Reload Tests</button>
      </div>
      <div class="test-card-grid">
        ${cards || '<p class="hint">Backend test list is empty.</p>'}
      </div>
    </section>
  `;
}

export function readTestRunForm(test: TestInfo, paramsById: Record<string, Record<string, number>>, sourceById: Record<string, TestSource>): {
  testId: string;
  source: TestSource;
  params: Record<string, number>;
} {
  const source = (document.querySelector<HTMLSelectElement>(`#testSource_${test.id}`)?.value ?? sourceById[test.id] ?? test.source) as TestSource;
  return {
    testId: test.id,
    source,
    params: paramsById[test.id] ?? defaultParams(test)
  };
}

function testCard(test: TestInfo, cardsState: TestCardsState, globalStatus: TestStatus | null): string {
  const selected = cardsState.selectedIds.includes(test.id);
  const expanded = Boolean(cardsState.expandedParamsById[test.id]);
  const status = effectiveStatus(test.id, cardsState.statusesById, globalStatus);
  const stateName = statusState(status);
  const params = cardsState.paramsById[test.id] ?? defaultParams(test);
  const sources = test.allowedSources?.length ? test.allowedSources : [test.source];
  const selectedSource = sources.includes(cardsState.sourceById[test.id]) ? cardsState.sourceById[test.id] : test.source;
  const sourceOptions = sources
    .map((source) => `<option value="${source}" ${source === selectedSource ? "selected" : ""}>${SOURCE_LABELS[source]}</option>`)
    .join("");

  return `
    <article class="test-card test-card-${stateName} ${selected ? "selected" : ""}" data-test-card="${test.id}">
      <div class="test-card-top">
        <div>
          <h3>${test.name}</h3>
          <p class="mono">${test.id}</p>
        </div>
        <span class="status ${stateName === "passed" ? "ok" : "idle"}">${stateName}</span>
      </div>
      <label>
        Source
        <select id="testSource_${test.id}" data-test-control="true">
          ${sourceOptions}
        </select>
      </label>
      <p class="result">${status.message}</p>
      <div class="button-row">
        <button data-run-test="${test.id}" data-test-control="true">Run</button>
        <button class="secondary" data-toggle-params="${test.id}" data-test-control="true">
          ${expanded ? "Hide Params" : "Params"}
        </button>
      </div>
      ${expanded ? parameterInputs(test, params) : ""}
    </article>
  `;
}

function parameterInputs(test: TestInfo, params: Record<string, number>): string {
  const inputs = (test.parameters ?? [])
    .map(
      (parameter) => `
        <label>
          ${parameter.label}
          <input
            data-test-control="true"
            data-test-param="${test.id}:${parameter.name}"
            type="number"
            min="${parameter.min}"
            max="${parameter.max}"
            value="${params[parameter.name] ?? parameter.defaultValue}"
          />
        </label>
      `
    )
    .join("");

  return `<div class="form-grid test-params">${inputs || '<p class="hint">No parameters.</p>'}</div>`;
}

function defaultParams(test: TestInfo): Record<string, number> {
  const params: Record<string, number> = {};
  for (const parameter of test.parameters ?? []) {
    params[parameter.name] = parameter.defaultValue;
  }
  return params;
}

function effectiveStatus(testId: string, statusesById: Record<string, TestStatus>, globalStatus: TestStatus | null): TestStatus {
  if (globalStatus?.id === testId && globalStatus.state === "running") {
    return globalStatus;
  }

  return statusesById[testId] ?? {
    id: testId,
    state: "idle",
    passed: false,
    message: "Not run yet."
  };
}

function statusState(status: TestStatus): string {
  if (status.state === "running") return "running";
  if (status.state === "stopped") return "stopped";
  if (status.state === "finished") return status.passed ? "passed" : "failed";
  return "idle";
}
