import type { TestInfo, TestSource, TestStatus } from "../api/client";

export function testView(tests: TestInfo[], status: TestStatus | null): string {
  const options = tests.map((test) => `<option value="${test.id}" data-source="${test.source}">${test.name}</option>`).join("");
  return `
    <section class="panel">
      <div class="panel-header">
        <h2>Tests</h2>
        <span class="status ${status?.passed ? "ok" : "idle"}">${status?.state ?? "idle"}</span>
      </div>
      <label>
        Test
        <select id="testId">${options}</select>
      </label>
      <label>
        Source
        <select id="testSource">
          <option value="mock">Mock</option>
          <option value="serial">Serial</option>
          <option value="received_snapshot">Received snapshot</option>
          <option value="received_live">Received live</option>
        </select>
      </label>
      <div class="button-row">
        <button id="runTest">Run</button>
        <button id="stopTest" class="secondary">Stop live</button>
      </div>
      <p class="result">${status?.message ?? "No test has run yet."}</p>
    </section>
  `;
}

export function readTestForm(): { testId: string; source: TestSource } {
  const testId = document.querySelector<HTMLSelectElement>("#testId")?.value ?? "pwm_frame_mock";
  const source = (document.querySelector<HTMLSelectElement>("#testSource")?.value ?? "mock") as TestSource;
  return { testId, source };
}
