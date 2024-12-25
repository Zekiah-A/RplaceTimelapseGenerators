# NodeBackupGenerator

This tool generates a timelapse image from a specific commit hash of the r/place canvas.

## Prerequisites

- Node.js (v14 or higher) or Bun
- npm or bun

## Usage

Run the tool with the desired commit hash:

```sh
node index.js <commit-hash> [options]
# or
bun run index.js <commit-hash> [options]
```

### Options

- `--output, -o`: Output file path (default: `./place.png`)
- `--base-url, -b`: Base URL for fetching data (default: `https://raw.githubusercontent.com/rplacetk/canvas1/`)

### Example

```sh
node index.js abcdef1234567890 --output ./output.png --base-url https://example.com/data/
# or
bun run index.js abcdef1234567890 --output ./output.png --base-url https://example.com/data/
```

This will generate an image from the specified commit hash and save it to the specified output file path.
