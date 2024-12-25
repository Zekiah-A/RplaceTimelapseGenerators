import jimp from "jimp"
import fetch from "node-fetch"
import yargs from "yargs"
import { hideBin } from "yargs/helpers"

const argv = yargs(hideBin(process.argv))
	.usage("Usage: $0 <commit-hash> [options]")
	.demandCommand(1, "You need to provide the commit hash")
	.option("output", {
		alias: "o",
		type: "string",
		description: "Output file path",
		default: "./place.png"
	})
	.option("base-url", {
		alias: "b",
		type: "string",
		description: "Base URL for fetching data",
		default: "https://raw.githubusercontent.com/rplacetk/canvas1/"
	})
	.help()
	.argv

const commitHash = argv._[0]
const outputFilePath = argv.output
const baseUrl = argv.baseUrl

console.log(`Using commit hash: ${commitHash}`)
console.log(`Output file path: ${outputFilePath}`)
console.log(`Base URL: ${baseUrl}`)

;(async function() {
	try {
		const startDate = Date.now()
		const fullBaseUrl = `${baseUrl}${commitHash}`
		
		const metadataRes = await fetch(fullBaseUrl + "/metadata.json")
		if (!metadataRes.ok) throw new Error("Failed to fetch metadata")
		const { palette, width, height } = await metadataRes.json()

		new jimp(width, height, 0x0, async (err, img) => {
			if (err) throw err

			const boardRes = await fetch(fullBaseUrl + "/place")
			if (!boardRes.ok) throw new Error("Failed to fetch place data")
			const board = new Uint8Array(await boardRes.arrayBuffer())

			for (let i = 0; i < board.byteLength; i++) {
				const colourInt = +palette[board[i]]
				const colour = jimp.rgbaToInt(
					(colourInt >> 24) & 0xFF, 
					(colourInt >> 16) & 0xFF,
					(colourInt >> 8) & 0xFF, 
					colourInt & 0xFF
				)
				img.setPixelColor(colour, i % width, Math.floor(i / width))
			}

			img.write(outputFilePath, () => {
				console.log(`Image saved to ${outputFilePath}, time taken: ${Date.now() - startDate}ms`)
			})
		})
	} catch (error) {
		console.error("Error:", error.message)
	}
})()
