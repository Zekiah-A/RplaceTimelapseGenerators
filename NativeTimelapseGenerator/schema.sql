-- TODO: Implement this

-- Stores Git repositories being tracked.
CREATE TABLE IF NOT EXISTS Repos (
	id INTEGER PRIMARY KEY, -- Unique identifier for the repository.
	url TEXT NOT NULL       -- URL of the Git repository.
);

-- Represents palettes of colours used in canvases.
CREATE TABLE IF NOT EXISTS Palettes (
	id INTEGER PRIMARY KEY, -- Unique identifier for the palette.
	size INTEGER NOT NULL   -- Number of colours in the palette.
);

-- Links colours to a palette and specifies their order within the palette.
CREATE TABLE IF NOT EXISTS Colours (
	palette_id INTEGER NOT NULL,                    -- ID of the palette.
	red INTEGER CHECK (red BETWEEN 0 AND 255),      -- Red channel (0-255).
	green INTEGER CHECK (green BETWEEN 0 AND 255),  -- Green channel (0-255).
	blue INTEGER CHECK (blue BETWEEN 0 AND 255),    -- Blue channel (0-255).
	alpha INTEGER CHECK (alpha BETWEEN 0 AND 255),  -- Alpha channel (0-255).
	position INTEGER NOT NULL,                      -- Index of the colour in the palette.
	PRIMARY KEY (palette_id, position),
	FOREIGN KEY (palette_id) REFERENCES Palettes(id)
);

-- Tracks Git commits for a repository.
CREATE TABLE IF NOT EXISTS Commits (
	id INTEGER PRIMARY KEY,         -- Unique identifier for the commit.
	repo_id INTEGER NOT NULL,       -- ID of the associated repository.
	hash TEXT NOT NULL UNIQUE,      -- Commit hash.
	date INTEGER NOT NULL,          -- Commit timestamp (UNIX epoch time).
	FOREIGN KEY (repo_id) REFERENCES Repos(id)
);

-- Stores metadata for a canvas, such as palette and dimensions.
CREATE TABLE IF NOT EXISTS CanvasMetadata (
	id INTEGER PRIMARY KEY,                     -- ID of metadata.
	palette_id INTEGER NOT NULL,                -- ID of the associated palette.
	first_seen_commit_id INTEGER NOT NULL,      -- First commit where this metadata was observed.
	width INTEGER NOT NULL CHECK (width > 0),   -- Canvas width (positive integer).
	height INTEGER NOT NULL CHECK (height > 0), -- Canvas height (positive integer).
	FOREIGN KEY (palette_id) REFERENCES Palettes(id),
	FOREIGN KEY (first_seen_commit_id) REFERENCES Commits(id)
);

-- Links commits to the metadata of the canvases they contain.
CREATE TABLE IF NOT EXISTS CommitCanvasMetadatas (
	commit_id INTEGER NOT NULL,                 -- ID of the commit.
	canvas_metadata_id INTEGER NOT NULL,        -- ID of the associated canvas metadata.
	PRIMARY KEY (commit_id, canvas_metadata_id),
	FOREIGN KEY (commit_id) REFERENCES Commits(id),
	FOREIGN KEY (canvas_metadata_id) REFERENCES CanvasMetadata(id)
);

-- Stores details of rendered canvases.
CREATE TABLE IF NOT EXISTS CanvasRenders (
	id INTEGER PRIMARY KEY,            -- Unique identifier for the rendered canvas.
	commit_id INTEGER NOT NULL,        -- ID of the associated commit.
	start_date INTEGER NOT NULL,       -- Timestamp of when the canvas render was started (UNIX epoch time).
	finish_date INTEGER NOT NULL,      -- Timestamp of when the canvas render was completed (UNIX epoch time).
	save_path TEXT NOT NULL,           -- File path where the render is stored.
	FOREIGN KEY (commit_id) REFERENCES Commits(id)
);
