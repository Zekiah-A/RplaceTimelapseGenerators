import { ComponentContainer, GoldenLayout, LayoutConfig } from "golden-layout";

import "./views/control-panel.ts";
import { ControlPanel } from "./views/control-panel.ts";
import "./views/main-control-view.ts";
import { MainControlView } from "./views/main-control-view.ts";
import "./views/worker-manager-view.ts";
import { WorkerManagerView } from "./views/worker-manager-view.ts";
import "./views/backups-view.ts";
import { BackupsView } from "./views/backups-view.ts";
import "./views/logs-list-view.ts";
import { LogsListView } from "./views/logs-list-view.ts";

document.addEventListener("DOMContentLoaded", async () => {
	const controlPanel: ControlPanel = document.querySelector("control-panel") as ControlPanel;
	await controlPanel.updateComplete;

	class MainControls {
		private _view: MainControlView;

		constructor(public container: ComponentContainer) {
			this._view = document.querySelector("main-control-view") as MainControlView;
			this._view.classList.add("layout-view");
			this._view.style.position = 'absolute';
			this._view.style.overflow = 'hidden';
		}

		// Implement the required getter for VirtuableComponent
		get rootHtmlElement(): HTMLElement {
			return this._view;
		}
	}

	class WorkerManager {
		private _view: WorkerManagerView;

		constructor(public container: ComponentContainer) {
			this._view = document.querySelector("worker-manager-view") as WorkerManagerView;
			this._view.classList.add("layout-view");
			this._view.style.position = 'absolute';
			this._view.style.overflow = 'hidden';
		}

		get rootHtmlElement(): HTMLElement {
			return this._view;
		}
	}

	class BackupsPanel {
		private _view: BackupsView;

		constructor(public container: ComponentContainer) {
			this._view = document.querySelector("backups-view") as BackupsView;
			this._view.classList.add("layout-view");
			this._view.style.position = 'absolute';
			this._view.style.overflow = 'hidden';
		}

		get rootHtmlElement(): HTMLElement {
			return this._view;
		}
	}

	class LogsPanel {
		private _view: LogsListView;

		constructor(public container: ComponentContainer) {
			this._view = document.querySelector("logs-list-view") as LogsListView;
			this._view.classList.add("layout-view");
			this._view.style.position = 'absolute';
			this._view.style.overflow = 'hidden';
		}

		get rootHtmlElement(): HTMLElement {
			return this._view;
		}
	}

	const layout: LayoutConfig = {
		root: {
			type: "row",
			content: [
				{
					type: "column",
					content: [
						{
							type: "row",
							content: [
								{
									title: "Main Controls",
									type: "component",
									componentType: "main-controls",
									reorderEnabled: true
								},
								{
									title: "Worker Management",
									type: "component",
									componentType: "worker-manager",
									reorderEnabled: true
								},
							]
						},
						{
							title: "Logs",
							type: "component",
							componentType: "logs-panel",
							reorderEnabled: true,
						}
					]			
				},
				{
					title: "Backups",
					type: "component",
					componentType: "backups-panel",
					reorderEnabled: true,
					size: "20%"
				}
			]
		},
		
	};

	const layoutElement: HTMLElement = document.querySelector("#layoutContainer")!;
	// Ensure the layout container has position set
	layoutElement.style.position = 'relative';

	const goldenLayout = new GoldenLayout(layoutElement);
	
	// Register components as virtual
	goldenLayout.registerComponentConstructor("main-controls", MainControls, true);
	goldenLayout.registerComponentConstructor("worker-manager", WorkerManager, true);
	goldenLayout.registerComponentConstructor("backups-panel", BackupsPanel, true);
	goldenLayout.registerComponentConstructor("logs-panel", LogsPanel, true);
	goldenLayout.loadLayout(layout);
	goldenLayout.resizeWithContainerAutomatically = true;
	goldenLayout.resizeDebounceExtendedWhenPossible = false;
});