#!/usr/bin/env bun
import { createCanvas } from "canvas"
import { parseArgs } from "util"

interface Metadata {
	palette: string[]
	width: number
	height: number
}

const { values, positionals } = parseArgs({
	args: Bun.argv.slice(2),
	options: {
		output: {
			type: "string",
			short: "o",
			default: "./place.png"
		},
		"base-url": {
			type: "string",
			short: "b",
			default: "https://raw.githubusercontent.com/rplacetk/canvas1/"
		},
		palette: {
			type: "boolean",
			short: "p",
			default: false
		},
		help: {
			type: "boolean",
			short: "h"
		}
	},
	allowPositionals: true
})

if (values.help) {
	console.log(`Usage: bun run index.ts <commitHash> [options]

Arguments:
  commitHash              Commit hash to fetch data from

Options:
  -o, --output <path>     Output file path (default: ./place.png)
  -b, --base-url <url>    Base URL for fetching data
  -p, --palette           Generate palette visualisation instead of canvas
  -h, --help              Show this help message`)
	process.exit(0)
}

if (positionals.length === 0) {
	console.error("Error: commitHash is required")
	process.exit(1)
}

const commitHash = positionals[0]
const outputFilePath = values.output!
const baseUrl = values["base-url"]!
const paletteMode = values.palette!

console.log(`Using commit hash: ${commitHash}`)
console.log(`Output file path: ${outputFilePath}`)
console.log(`Base URL: ${baseUrl}`)
console.log(`Palette mode: ${paletteMode}`)

async function generatePalette(palette: string[]): Promise<void> {
	const blockSize = 128
	const cols = Math.ceil(Math.sqrt(palette.length))
	const rows = Math.ceil(palette.length / cols)
	const canvasWidth = cols * blockSize
	const canvasHeight = rows * blockSize

	const canvas = createCanvas(canvasWidth, canvasHeight)
	const ctx = canvas.getContext("2d")

	ctx.fillStyle = "#000000"
	ctx.fillRect(0, 0, canvasWidth, canvasHeight)

	for (let i = 0; i < palette.length; i++) {
		const row = Math.floor(i / cols)
		const col = i % cols
		const x = col * blockSize
		const y = row * blockSize

		const colourInt = +palette[i]
		const r = (colourInt >> 24) & 0xFF
		const g = (colourInt >> 16) & 0xFF
		const b = (colourInt >> 8) & 0xFF
		const a = (colourInt & 0xFF) / 255

		ctx.fillStyle = `rgba(${r}, ${g}, ${b}, ${a})`
		ctx.fillRect(x, y, blockSize, blockSize)

		const brightness = (r * 0.299 + g * 0.587 + b * 0.114) / 255
		const textColor = brightness > 0.5 ? "#000000" : "#FFFFFF"

		ctx.fillStyle = textColor
		ctx.font = "bold 16px Arial"
		ctx.textAlign = "center"
		ctx.textBaseline = "middle"

		const centerX = x + blockSize / 2
		const centerY = y + blockSize / 2

		ctx.fillText(`Index: ${i}`, centerX, centerY - 10)
		ctx.fillText(`Value: ${colourInt}`, centerX, centerY + 10)
	}

	const buffer = canvas.toBuffer("image/png")
	await Bun.write(outputFilePath, buffer)
}

async function generateCanvas(palette: string[], width: number, height: number, fullBaseUrl: string): Promise<void> {
	const boardRes = await fetch(fullBaseUrl + "/place")
	if (!boardRes.ok) throw new Error("Failed to fetch place data")
	const board = new Uint8Array(await boardRes.arrayBuffer())

	const canvas = createCanvas(width, height)
	const ctx = canvas.getContext("2d")
	const imageData = ctx.createImageData(width, height)

	for (let i = 0; i < board.byteLength; i++) {
		const colourInt = +palette[board[i]]
		const pixelIndex = i * 4

		imageData.data[pixelIndex] = (colourInt >> 24) & 0xFF     // R
		imageData.data[pixelIndex + 1] = (colourInt >> 16) & 0xFF // G
		imageData.data[pixelIndex + 2] = (colourInt >> 8) & 0xFF  // B
		imageData.data[pixelIndex + 3] = colourInt & 0xFF         // A
	}

	ctx.putImageData(imageData, 0, 0)
	const buffer = canvas.toBuffer("image/png")
	await Bun.write(outputFilePath, buffer)
}

try {
	const startDate = Date.now()
	const fullBaseUrl = `${baseUrl}${commitHash}`

	const metadataRes = await fetch(fullBaseUrl + "/metadata.json")
	if (!metadataRes.ok) throw new Error("Failed to fetch metadata")
	const { palette, width, height }: Metadata = await metadataRes.json()

	if (paletteMode) {
		await generatePalette(palette)
		console.log(`Palette saved to ${outputFilePath}, time taken: ${Date.now() - startDate}ms`)
	} else {
		await generateCanvas(palette, width, height, fullBaseUrl)
		console.log(`Image saved to ${outputFilePath}, time taken: ${Date.now() - startDate}ms`)
	}
} catch (error) {
	console.error("Error:", error instanceof Error ? error.message : String(error))
	process.exit(1)
}