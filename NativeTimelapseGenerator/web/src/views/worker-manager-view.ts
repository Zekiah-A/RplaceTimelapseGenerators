import { LitElement, html } from "lit";
import { customElement, property } from "lit/decorators.js";
import { ControlPanel, EventPacket } from "./control-panel";
import { BufReader } from "nanobuf";

type WorkerType = "download"|"render"|"save";
type WorkerStatus = "waiting"|"active";

export type Worker = {
	workerId: number;
	workerType: string;
	status:WorkerStatus;
};

const workerTypesMap:Map<number, WorkerType> = new Map<number, WorkerType>([
	[ 0, "download" ],
	[ 1, "render" ],
	[ 2, "save" ],
]);

const workerStatusesMap: Map<number, WorkerStatus> = new Map<number, WorkerStatus>([
    [0, "waiting"],
    [1, "active"],
]);

@customElement("worker-instance")
export class WorkerInstance extends LitElement {
	@property({ type: String })
	workerType:WorkerType = null!;
	@property({ type: Number })
	workerId:number = null!;
	@property({ type: String })
	status: "waiting" | "active" = null!;

	createRenderRoot() {
		return this
	}

	render() {
		return html`
			<table style="width:100%">
				<tr>
					<th>Type:</th>
					<td> ${this.workerType}</td>
				</tr>
				<tr>
					<th>Worker ID:</th>
					<td>${this.workerId}</td>
				</tr>
				<tr>
					<th>Status:</th>
					<td>${this.status}</td>
				</tr>
			</table>
		`;
	}
}

@customElement("worker-manager")
export class WorkerManager extends LitElement {
	@property({ type: String })
	workerType: WorkerType = null!;

	@property({ type: Array })
	workers: Worker[] = [];

	createRenderRoot() {
		return this;
	}

	connectedCallback(): void {
		super.connectedCallback();

		const managerView = this.parentElement?.parentElement as WorkerManagerView;
		managerView.addWorkerHandler(this.workerType, (packet: BufReader) => {
			this.handleWorkerPacket(packet);
		});
	}

	handleWorkerPacket(packet: BufReader) {
		const workers: Worker[] = [];
		const workerCount = packet.u8();
		for (let i = 0; i < workerCount; i++) {
			const workerId = packet.u8();
			const status = workerStatusesMap.get(packet.u8())!;
			workers.push({ workerId, workerType: this.workerType, status });
		}
		this.workers = workers;
	}

	render() {
		return html`
			<h4>${this.workerType} workers: ${this.workers.length}</h4>
			<ul>
				${this.workers.map(
					(worker) => html`
						<li>
							<worker-instance
								.status=${worker.status}
								.workerId=${worker.workerId}
								.type=${worker.workerType}
							></worker-instance>
						</li>
					`
				)}
			</ul>
			<button type="button" style="background-color: green;" @click=${this.handleAddWorker}>Add Worker</button>
			<button type="button" style="background-color: red;" @click=${this.handleRemoveWorker}>Remove Worker</button>
		`;
	}

	handleAddWorker() {

	}

	handleRemoveWorker() {

	}
}

@customElement("worker-manager-view")
export class WorkerManagerView extends LitElement {
	@property({ type: Object })
	workerUpdates: Map<WorkerType, Worker[]> = new Map();
	private workerUpdateHandlers: Map<WorkerType, (packet: BufReader) => void> = new Map();

	addWorkerHandler(workerType: WorkerType, handler: (packet: BufReader) => void) {
		this.workerUpdateHandlers.set(workerType, handler);
	}

	createRenderRoot() {
		return this;
	}

	connectedCallback(): void {
		super.connectedCallback();

		const parent = this.parentElement as ControlPanel;
		parent.addPacketHandler(EventPacket.WorkerStatus, (packet: BufReader) => {
			const workerTypeCount = packet.u8();
			for (let i = 0; i < workerTypeCount; i++) {
				const workerType = workerTypesMap.get(packet.u8())!;
				this.workerUpdateHandlers.get(workerType)!(packet);
			}
		});
	}

	render() {
		return html`
			<h2>Workers:</h2>
			<div style="overflow: auto; height: calc(100% - 42px);">
			${[...workerTypesMap.values()].map(
				(type) => html`<worker-manager .workerType=${type}></worker-manager>`
			)}
			</div>
		`;
	}
}
