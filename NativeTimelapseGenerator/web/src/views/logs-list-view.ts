import { LitElement, html } from "lit";
import { customElement, property } from "lit/decorators.js";

export type LogEntry = {
	type: string;
	time: Date;
	message: string;
};

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
				(!this.filterType || log.type === this.filterType) &&
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

	render() {
		return html`
			<h2>Logs:</h2>
			<div class="controls">
				<label>
					Filter by Type:
					<select @change="${this.updateFilterType}">
						<option value="">All</option>
						${[...new Set(this.logs.map(log => log.type))].map(
							type => html`<option value="${type}" ?selected="${type === this.filterType}">${type}</option>`
						)}
					</select>
				</label>
				<label>
					Search:
					<input type="text" @input="${this.updateSearchQuery}" placeholder="Search logs..." />
				</label>
			</div>
			<table>
				<thead>
					<tr>
						<th @click="${() => this.updateSort("type")}">Type</th>
						<th @click="${() => this.updateSort("time")}">Time</th>
						<th @click="${() => this.updateSort("message")}">Message</th>
					</tr>
				</thead>
				<tbody>
					${this.filteredLogs.map(log => html`
					<tr>
						<td>${log.type}</td>
						<td>${log.time.toLocaleString()}</td>
						<td>${log.message}</td>
					</tr>
					`
					)}
				</tbody>
			</table>
	`
	}
}
