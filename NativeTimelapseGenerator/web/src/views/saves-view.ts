import { LitElement, html } from "lit";
import { customElement, property, state } from "lit/decorators.js";
import { ControlPanel, EventPacket } from "./control-panel";
import { BufReader } from "nanobuf";

export enum SaveType {
	CanvasDownload = 1,
	CanvasRender = 2,
	DateRender = 3,
	PlacersDownload = 4,
	TopPlacersRender = 5,
	CanvasControlRender = 6,
}

type Save = {
	commitId: number;
	commitHash: string;
	date: Date;
	path: string;
};

@customElement("saves-view")
export class SavesView extends LitElement {
	@property({ type: String })
	currentRenderSave: string = "";

	@property({ type: Object })
	saves: Map<SaveType, Save[]> = new Map<SaveType, Save[]>([
		[SaveType.CanvasDownload, []],
		[SaveType.CanvasRender, []],
		[SaveType.DateRender, []],
		[SaveType.TopPlacersRender, []],
		[SaveType.CanvasControlRender, []],
	]);

	@property({ type: Number })
	savesPerSecond: number = 0;

	@property({ type: Number })
	completedSaves: number = 0;

	@state()
	showAllSaves: boolean = false;

	createRenderRoot() {
		return this;
	}

	connectedCallback(): void {
		super.connectedCallback();

		const parent = this.parentElement as ControlPanel;
		parent.addPacketHandler(EventPacket.SaveStatus, (packet: BufReader) => {
			this.completedSaves = packet.u32();
			this.savesPerSecond = packet.f32();
			const saveTypesCount = packet.u8();
			for (let i = 0; i < saveTypesCount; i++) {
				const saveType = packet.u8();
				const saveCount = packet.u32();
				for (let j = 0; j < saveCount; j++) {
					const commitId = packet.u32();
					const commitHash = packet.str();
					const date = new Date(packet.u64());
					const path = packet.str();
					const save = { commitId, commitHash, date, path };
					const saveList = this.saves.get(saveType);
					if (saveList) {
						saveList.push(save);
					}
				}
			}
		});
	}

	private renderSaveType(type: SaveType): string {
		switch (type) {
			case SaveType.CanvasDownload:
				return "Canvas Download";
			case SaveType.CanvasRender:
				return "Canvas Render";
			case SaveType.DateRender:
				return "Date Render";
			case SaveType.PlacersDownload:
				return "Placers Download";
			case SaveType.TopPlacersRender:
				return "Top Placers Render";
			case SaveType.CanvasControlRender:
				return "Canvas Control Render";
			default:
				return "Unknown Type";
		}
	}

	private getMostRecentSave(saveList: Save[]): Save | null {
		return saveList.length > 0 ? saveList[saveList.length - 1] : null;
	}

	private toggleShowAllSaves() {
		this.showAllSaves = !this.showAllSaves;
	}

	render() {
		return html`
			<h2>Saves:</h2>
			<div style="overflow-y: scroll; max-height: calc(100% - 48px);">
				<div>
					${Array.from(this.saves.entries()).map(([type, saveList]) => {
						const mostRecent = this.getMostRecentSave(saveList);
						return html`
							<h3>${this.renderSaveType(type)}</h3>
							${mostRecent
								? html`
									<p>
										<strong>Date:</strong> ${mostRecent.date}
										<br />
										<strong>Commit ID:</strong> ${mostRecent.commitId}
										<br />
										<strong>Commit Hash:</strong> ${mostRecent.commitHash}
									</p>
								`
								: html`<p>No saves available</p>`}
						`;
					})}
				</div>
				<button @click=${this.toggleShowAllSaves}>
					${this.showAllSaves ? "Hide All Saves" : "Show All Saves"}
				</button>
				${this.showAllSaves
					? html`
						<h2>All Saves:</h2>
						<div>
							${Array.from(this.saves.entries()).map(
								([type, saveList]) => html`
									<h3>${this.renderSaveType(type)}</h3>
									<ul>
										${saveList.map(
											(save) => html`
												<li>
													<strong>Date:</strong> ${save.date}
													<br>
													${save.path.endsWith(".png") ? html`<img src="${save.path}">` : ""}
													<strong>Commit ID:</strong> ${save.commitId} * 
													<strong>Commit Hash:</strong> ${save.commitHash} *
													<strong>Path:</strong> ${save.path}
												</li>
											`
										)}
									</ul>
								`
							)}
						</div>
					`
					: ""}
				<p>Saves per Second: ${this.savesPerSecond.toFixed(2)}</p>
				<p>Completed Saves: ${this.completedSaves}</p>
			</div>
		`;
	}
}
