import { LitElement, html, css } from "lit";
import { customElement, property } from "lit/decorators.js";
import { ControlPanel } from "./control-panel";

@customElement("main-control-view")
export class MainControlView extends LitElement {
	@property({ type: Boolean })
	connected = false;
	@property({ type: Boolean })
	started = false;
	@property({ type: Boolean })
	localServer = true;

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
					<span>${this.connected}</span>
				</p>
				${this.connected
					? html`
						<p>
							<span class="status-label">Status:</span>
							<span class="${this.started ? 'status-enabled' : 'status-disabled'}">${this.started ? 'Started' : 'Stopped'}</span>
						</p>
						<div>
							<button @click=${this.handleStartPressed}>Start</button>
							<button @click=${this.handleShutdownPressed}>Shutdown</button>
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
								<input style="display: block;" type="number">
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
										<span>Event socket users HTTPs:</span>
										<input type="checkbox">
									</label>
								`
							}
							<hr>
							<button type="submit">Connect</button>
						</fieldset>
					</form>
				`
			}
		`;
	}

	handleStartPressed(e:Event) {
		e.preventDefault()
		const controlPanel:ControlPanel = this.parentElement as ControlPanel;
		controlPanel.connect()
	}

	handleShutdownPressed() {

	}
}
