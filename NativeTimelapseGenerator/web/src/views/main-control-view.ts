import { LitElement, html, css } from "lit";
import { customElement, property } from "lit/decorators.js";
import { ControlPacket, ControlPanel } from "./control-panel";
import { BufWriter } from "nanobuf";

@customElement("main-control-view")
export class MainControlView extends LitElement {
	@property({ type: Boolean })
	connected = false;
	@property({ type: Boolean })
	started = false;
	@property({ type: Boolean })
	localServer = true;
	@property({ type: Boolean })
	eventSocketHttps = false;
	@property({ type: String })
	eventSocketPort = "5555";

	createRenderRoot() {
		return this
	}

	render() {
		return html`
			<h2>Main control:</h2>
			<fieldset>
				<legend>Status:</legend>
					<p class="connected-label">
					<span class="status-label">Connected:</span>
					<span>${this.connected ? "Yes" : "No"}</span>
				</p>
				<dialog id="formDialog">
					<form id="configForm" title="Config">
						<fieldset>
							<legend>Config</legend>
							<label title="repo_url">
								<span>Repo URL: </span>
								<input type="text" name="repoUrl" value="https://github.com/rplacetk/canvas1">
							</label>
							<label title="download_base_url">
								<span>Download base URL: </span>
								<input type="text" name="downloadBaseUrl" value="https://raw.githubusercontent.com/rplacetk/canvas1">
							</label>
							<label title="game_server_base_url">
								<span>Game server base URL: </span>
								<input type="text" name="gameServerBaseUrl" value="https://server.rplace.live">
							</label>
							<label title="commit_hashes_file_name">
								<span>Commit hashes file name: </span>
								<input type="text" name="commitHashesFileName" value="commit_hashes.txt">
							</label>
							<label title="max_top_placers">
								<span>Max top placers:</span>
								<input type="number" name="maxTopPlacers" value="10">
							</label>
						</fieldset>
						<button type="submit" @click=${this.handleStartSubmitPressed}>Start</button>
					</form>
				</dialog>				
				${this.connected
					? html`
						<p>
							<span class="status-label">Status:</span>
							<span class="${this.started ? 'status-enabled' : 'status-disabled'}">${this.started ? 'Started' : 'Stopped'}</span>
						</p>
						<div>
							<button @click=${this.handleStartPressed}>Start</button>
							<button @click=${this.handleShutdownPressed}>Shutdown</button>
							<button @click=${this.handleDisconnectPressed}>Disconnect</button>
						</div>`
					: html``
				}
			</fieldset>
			${this.connected
				? html``
				: html`
					<form>
						<fieldset>
							<legend>Connect to generator instance:</legend>
							<label>
								<span>Event socket port:</span>
								<input style="display: block;" type="number" .value=${this.eventSocketPort}>
							</label>
							<label>
								<span>Local server:</span>
								<input type="checkbox" ?checked=${this.localServer} @click=${(e:any) => this.localServer = e.target.checked}>
							</label>
							${this.localServer
								? html``
								: html`
									<label>
										<span>Event socket IP/Domain:</span>
										<input style="display: block;" type="text">
									</label>
									<label>
										<span>Event socket uses HTTPs:</span>
										<input type="checkbox" ?checked=${this.eventSocketHttps}>
									</label>
								`
							}
							<hr>
							<button type="submit" @click=${this.handleConnectPressed}>Connect</button>
						</fieldset>
					</form>
				`
			}
		`;
	}

	handleConnectPressed(e:Event) {
		e.preventDefault()
		const parent = this.parentElement as ControlPanel;
		parent.connect(this.eventSocketHttps, "localhost", this.eventSocketPort);
	}

	handleDisconnectPressed(e:Event) {
		e.preventDefault()
		const parent = this.parentElement as ControlPanel;
		parent.disconnect();
	}

	handleStartPressed(e:Event) {
		e.preventDefault()
		const formDialog:HTMLDialogElement = this.querySelector("#formDialog")!;
		formDialog.showModal()


	}

	handleStartSubmitPressed(e:Event) {
		e.preventDefault()
		const configForm:HTMLFormElement = this.querySelector("#configForm")!;
		//@ts-ignore
		const { repoUrl, downloadBaseUrl, gameServerBaseUrl, commitHashesFileName, maxTopPlacers } = configForm.elements
		const packet = new BufWriter()
		packet.u8(ControlPacket.Start);
		packet.str(repoUrl.value);
		packet.str(downloadBaseUrl.value);
		packet.str(gameServerBaseUrl.value);
		packet.str(commitHashesFileName.value);
		packet.u32(maxTopPlacers.value);
		const controlPanel:ControlPanel = this.parentElement as ControlPanel;
		controlPanel.websocket?.send(packet);		
	}

	handleShutdownPressed() {

	}
}
