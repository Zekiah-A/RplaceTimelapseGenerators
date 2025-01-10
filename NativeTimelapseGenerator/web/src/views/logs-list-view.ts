import { LitElement, html } from "lit";
import { customElement, property } from "lit/decorators.js";
import { ControlPanel, EventPacket } from "./control-panel";
import { BufReader } from "nanobuf";

type LogType = "info"|"warning"|"error";

export type LogEntry = {
	logType:LogType;
	date: Date;
	message: String;
};

export const logTypesMap:Map<number, LogType> = new Map<number, LogType>([
	[ 0, "info" ],
	[ 1, "warning" ],
	[ 2, "error" ]
]);

@customElement("logs-list-view")
export class LogsListView extends LitElement {
	@property({ type: Array }) logs: LogEntry[] = [];
	@property({ type: String }) filterType: string = "";
	@property({ type: String }) searchQuery: string = "";
	@property({ type: String }) sortField: string = "time";
	@property({ type: String }) sortOrder: "asc" | "desc" = "asc";

	get filteredLogs() {
		return this.logs
			.filter(log =>
				(!this.filterType || log.logType === this.filterType) &&
				(!this.searchQuery || log.message.toLowerCase().includes(this.searchQuery.toLowerCase()))
			)
			.sort((a, b) => {
				const fieldA = a[this.sortField as keyof LogEntry]
				const fieldB = b[this.sortField as keyof LogEntry]

				if (fieldA instanceof Date && fieldB instanceof Date) {
					return this.sortOrder === "asc"
						? fieldA.getTime() - fieldB.getTime()
						: fieldB.getTime() - fieldA.getTime()
				}

				if (typeof fieldA === "string" && typeof fieldB === "string") {
					return this.sortOrder === "asc"
						? fieldA.localeCompare(fieldB)
						: fieldB.localeCompare(fieldA)
				}

				return 0
			});
	}

	updateFilterType(e: Event) {
		this.filterType = (e.target as HTMLSelectElement).value
	}

	updateSearchQuery(e: Event) {
		this.searchQuery = (e.target as HTMLInputElement).value
	}

	updateSort(field: string) {
		if (this.sortField === field) {
			this.sortOrder = this.sortOrder === "asc" ? "desc" : "asc"
		}
		else {
			this.sortField = field;
			this.sortOrder = "asc"
		}
	}

	createRenderRoot() {
		return this
	}

	connectedCallback(): void {
		super.connectedCallback()

		const parent = this.parentElement as ControlPanel;
		parent.addPacketHandler(EventPacket.LogMessage, (packet:BufReader) =>  {
			const type = logTypesMap.get(packet.u8())!;
			const logDate = new Date(packet.u64() * 1000);
			const msg = packet.str();
			this.logs = [...this.logs, { logType: type, date: logDate, message: msg }];
		})
	}

	render() {
		return html`
			<h2>Logs:</h2>
			<div class="controls">
				<label>
					Filter by Type:
					<select @change="${this.updateFilterType}">
						<option value="">All</option>
						${[...new Set(this.logs.map(log => log.logType))].map(
							type => html`<option value="${type}" ?selected="${type === this.filterType}">${type}</option>`
						)}
					</select>
				</label>
				<label>
					Search:
					<input type="text" @input="${this.updateSearchQuery}" placeholder="Search logs..." />
				</label>
			</div>
			<div style="overflow: auto; height: calc(100% - 96px); border: 1px solid #ccc;">
				<table style="width: 100%; border-collapse: collapse;">
					<thead style="position: sticky;top: 0px;z-index: 1;background-color: #80808030;">
						<tr>
							<th @click="${() => this.updateSort("type")}">Type</th>
							<th @click="${() => this.updateSort("time")}">Time</th>
							<th @click="${() => this.updateSort("message")}">Message</th>
						</tr>
					</thead>
					<tbody>
						${this.filteredLogs.map(log => html`
						<tr>
							<td>${log.logType}</td>
							<td>${log.date.toLocaleString()}</td>
							<td>${log.message}</td>
						</tr>
						`
						)}
					</tbody>
				</table>
			</div>`
	}
}
