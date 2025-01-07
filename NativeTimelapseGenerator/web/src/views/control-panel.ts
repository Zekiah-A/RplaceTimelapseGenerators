import { LitElement, html } from "lit";
import { customElement } from "lit/decorators.js";

import "./main-control-view.ts";
import "./worker-manager-view.ts";
import "./backups-view.ts";
import "./logs-list-view.ts";


@customElement("control-panel")
export class ControlPanel extends LitElement {
	connected = false;
	websocket: WebSocket | null = null;

	setConnected(state: boolean) {
		this.connected = state;
		console.log(`WebSocket connected: ${state}`);
	}

	connect(https: boolean, domain: string, port: string) {
		const protocol = https ? "wss" : "ws";
		const url = `${protocol}://${domain}:${port}/events`;
	
		if (this.websocket && (this.websocket.readyState === WebSocket.OPEN || this.websocket.readyState === WebSocket.CONNECTING)) {
			console.warn("WebSocket is already connected or connecting.");
			return;
		}
	
		this.websocket = new WebSocket(url);
	
		this.websocket.onopen = () => {
			this.setConnected(true);
			console.log("WebSocket connection established.");
		};
	
		this.websocket.onclose = () => {
			this.setConnected(false);
			console.log("WebSocket connection closed.");
		};
	
		this.websocket.onerror = (error) => {
			console.error("WebSocket error occurred:", error);
			this.setConnected(false);
		};

		this.websocket.onmessage = async (event:MessageEvent) => {
		};
	}

	disconnect() {
		if (!this.websocket || this.websocket.readyState !== WebSocket.OPEN) {
			console.warn("WebSocket is not connected.");
			return;
		}
	
		this.websocket.close();
		this.websocket = null;
		console.log("WebSocket disconnect requested.");
	}

	createRenderRoot() {
		return this
	}

	render() {
		return html`
			<main-control-view .connected=${this.connected}></main-control-view>
			<worker-manager-view></worker-manager-view>
			<backups-view></backups-view>
			<logs-list-view></logs-list-view>
		`
	}
}