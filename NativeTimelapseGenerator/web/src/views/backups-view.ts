import { LitElement, html } from "lit";
import { customElement } from "lit/decorators.js";

@customElement("backups-view")
export class BackupsView extends LitElement {
	createRenderRoot() {
		return this
	}

	render() {
		return html`
			<h2>Backups:</h2>
			<p>Total Frames: 0</p>
			<p>Backups per Second: 0</p>
			<p>Current Canvas Date: null</p>
		`;
	}
}
