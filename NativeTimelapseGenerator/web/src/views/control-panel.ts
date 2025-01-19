import { LitElement, html } from "lit";
import { customElement, property } from "lit/decorators.js";
import { BufReader } from "nanobuf";

import "./main-control-view.ts";
import "./worker-manager-view.ts";
import "./backups-view.ts";
import "./logs-list-view.ts";

export enum EventPacket {
	LogMessage = 0,
	WorkerStatus = 1
};

export enum ControlPacket {
	Start = 16,
	Stop = 17,
	AddWorker = 18,
	RemoveWorker = 19
}

@customElement("control-panel")
export class ControlPanel extends LitElement {
	@property({ type: Boolean })
	connected = false;

	websocket: WebSocket | null = null;
	packetHandlers:Map<EventPacket, Array<(packet:BufReader) => void>> = new Map<EventPacket, Array<(packet:BufReader) => void>>([]);

	addPacketHandler(packet:EventPacket, handler:(packet:BufReader) => void) {
		let handlers = this.packetHandlers.get(packet);
		if (!handlers) {
			handlers = [];
			this.packetHandlers.set(packet, handlers);
		}
		handlers.push(handler);
	}

	setConnected(state: boolean) {
		this.connected = state;
		console.log(`WebSocket connected: ${state}`);
	}

	connect(https: boolean, domain: string, port: string) {
		const protocol = https ? "wss" : "ws";
		const url = `${protocol}://${domain}:${port}`;
	
		if (this.websocket && (this.websocket.readyState === WebSocket.OPEN || this.websocket.readyState === WebSocket.CONNECTING)) {
			console.warn("WebSocket is already connected or connecting.");
			return;
		}
	
		this.websocket = new WebSocket(url);
		this.websocket.binaryType = "arraybuffer";
	
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

		this.websocket.onmessage = (event:MessageEvent) => {			
			const reader = new BufReader(event.data)
			const packet = reader.u8() as EventPacket;

			const handlers = this.packetHandlers.get(packet);
			if (!handlers) {
				this.packetHandlers.set(packet, []);
				return;
			}
			const rest = reader.copyRemainingToArrayBuffer();
			for (const handler of handlers) {
				const packet = new BufReader(rest);
				handler(packet);
			}
		};
	}

	disconnect() {
		if (!this.websocket || this.websocket.readyState !== WebSocket.OPEN) {
			console.warn("WebSocket is not connected.");
			return;
		}
	
		this.websocket.close();
		this.websocket = null;
		this.connected = false;
	}

	createRenderRoot() {
		return this
	}

	connectedCallback(): void {
		super.connectedCallback()
		
		// Attempt auto connect
		this.connect(false, "localhost", "5555")
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