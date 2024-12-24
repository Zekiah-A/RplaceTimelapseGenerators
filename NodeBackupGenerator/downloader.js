import jimp from "jimp"
import fetch from "node-fetch"

if (!process.argv[2]) {
    console.log("You need to run this program with the commit hash of the backup with the desired place file, i.e 'bun downloader.js 69190a70f0d116029f5cf882d9ab4e336ac42664'")
    process.exit(0)
}

console.log(`Using ${process.argv[2]}`)

;(async function() {
    const startDate = Date.now()
    const baseUrl = `https://raw.githubusercontent.com/rplacetk/canvas1/${process.argv[2]}`
    const metadataRes = await fetch(baseUrl + "/metadata.json")
    const { palette, width, height } = await metadataRes.json()

    new jimp(width, height, 0x0, async (err, img) => {
        const boardRes = await fetch(baseUrl + "/place")
        const board = new Uint8Array(await boardRes.arrayBuffer())
        for (let i = 0; i < board.byteLength; i++) {
            const colourInt = +palette[board[i]]
            const colour = jimp.rgbaToInt((colourInt >> 24) & 0xFF, (colourInt >> 16) & 0xFF,
                (colourInt >> 8) & 0xFF, colourInt && 0xFF)  // Directly use the palette value
            img.setPixelColor(colour, i % width, Math.floor(i / width))
        }
        img.write("./place.png")
        console.log("Image saved, time taken: " + (Date.now() - startDate) + "ms")
    })
})()
