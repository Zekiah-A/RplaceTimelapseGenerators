import { LitElement, html } from "lit";
import { customElement, property } from "lit/decorators.js";

export type Worker = {
	workerId: number;
	type: string;
	status:"waiting"|"active";
};

@customElement("worker-instance")
export class WorkerInstance extends LitElement implements Worker {
	@property({ type: String })
	type:string = "";
	@property({ type: Number })
	workerId:number = -1;
	@property({ type: String })
	status: "waiting" | "active" = "waiting";

	createRenderRoot() {
		return this
	}

	render() {
		return html`
			<table style="width:100%">
				<tr>
					<th>Type:</th>
					<td> ${this.type}</td>
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
	workerType:"download"|"render"|"save"|"" = "";
	@property({ type: Array })
	workers:Array<Worker> = [];

	createRenderRoot() {
		return this
	}

	render() {
		return html`
			<p>
				<span>${this.workerType} workers: ${this.workers.length}</span>
				<ol>
				${this.workers.map(worker => html`
					<li>
						<worker-instance .status=${worker.status} .workerId=${worker.workerId} .type=${worker.type}></worker-instance>
					</li>
				`)}
				</ol>
				<button type="button" style="background-color: green;">Add Worker</button>
				<button type="button" style="background-color: red;">Remove Worker</button>
			</p>
		`
	}

}

@customElement("worker-manager-view")
export class WorkerManagerView extends LitElement {
	createRenderRoot() {
		return this
	}

	render() {
		return html`
			<h2>Workers:</h2>
			<worker-manager .workerType=${"download"} .workers=${[]}></worker-manager>
			<worker-manager .workerType=${"render"} .workers=${[]}></worker-manager>
			<worker-manager .workerType=${"save"} .workers=${[]}></worker-manager>
		`;
	}
}
