import jimp from 'jimp';
import fetch from 'node-fetch';

let palette = [
    '109, 0, 26',
    '190, 0, 57',
    '255, 69, 0',
    '255, 168, 0',
    '255, 214, 53',
    '255, 248, 184',
    '0, 163, 104',
    '0, 204, 120',
    '126, 237, 86',
    '0, 117, 111',
    '0, 158, 170',
    '0, 204, 192',
    '36, 80, 164',
    '54, 144, 234',
    '81, 233, 244',
    '73, 58, 193',
    '106, 92, 255',
    '148, 179, 255',
    '129, 30, 159',
    '180, 74, 192',
    '228, 171, 255',
    '222, 16, 127',
    '255, 56, 129',
    '255, 153, 170',
    '109, 72, 47',
    '156, 105, 38',
    '255, 180, 112',
    '0, 0, 0',
    '81, 82, 82',
    '137, 141, 144',
    '212, 215, 217',
    '255, 255, 255'
]

if (!process.argv[2]) {
    console.log("You need to run this program with the commit hash of the backup with the desired place file, i.e 'node downloader.js 69190a70f0d116029f5cf882d9ab4e336ac42664'")
    process.exit(0)
}

console.log(`Using ${process.argv[2]}`)

new jimp(2000, 2000, 0x0, (err, img) => {
    let before = Date.now()
    fetch(`https://raw.githubusercontent.com/rslashplace2/rslashplace2.github.io/${process.argv[2]}/place`).then(async response => {
        let board = new Uint8Array(await response.arrayBuffer())
        for (let i = 0; i < board.byteLength; i++) {
                let c = (palette[board[i]] || palette[0]).split(", ")
                let rc = jimp.rgbaToInt(Number(c[0]), Number(c[1]), Number(c[2]), 255)
                img.setPixelColor(rc, i % 2000, i / 2000)
        }
        img.write('./place.png')
        console.log("saved, time taken: " + (Date.now() - before) + "ms")
    })
})
