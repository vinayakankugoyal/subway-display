import { readFile, writeFile, rename } from 'node:fs/promises';
import { fileURLToPath } from 'node:url';
import { dirname, join } from 'node:path';

// Resolve data files against the package root, so this works the same whether
// it is running from src/ during development or from dist/ after a build.
const packageRoot = join(dirname(fileURLToPath(import.meta.url)), '..');
const STATE_FILE = process.env.STATE_FILE ?? join(packageRoot, 'stations.json');
const STOPS_FILE = join(packageRoot, 'stops.json');

export const FEED_BASE = 'https://api-endpoint.mta.info/Dataservice/mtagtfsfeeds/';
export const MAX_MINS = 3;        // arrivals per row; the device shows at most 4
export const MAX_ROWS = 4;        // device MAX_ROWS
export const MAX_LABEL = 23;      // device label buffer is char[24]
export const CACHE_MS = 30_000;
export const FETCH_TIMEOUT_MS = 10_000;
export const TIMEZONE = 'America/New_York';

/** A station in the catalog generated from static GTFS. */
export interface Station {
  name: string;
  routes: string[];
}

/** What the user supplies when changing stations. */
export interface RowInput {
  line: string;
  stopId: string;
  label: string;
}

/** A validated row, with the feed and station name resolved. */
export interface Row extends RowInput {
  feed: string;
  station: string;
}

// A realtime feed carries a whole group of lines, so the line picks the feed.
const FEED_FOR_LINE: Record<string, string> = {
  1: 'gtfs', 2: 'gtfs', 3: 'gtfs', 4: 'gtfs', 5: 'gtfs', 6: 'gtfs', 7: 'gtfs', GS: 'gtfs',
  A: 'gtfs-ace', C: 'gtfs-ace', E: 'gtfs-ace', H: 'gtfs-ace', FS: 'gtfs-ace',
  B: 'gtfs-bdfm', D: 'gtfs-bdfm', F: 'gtfs-bdfm', M: 'gtfs-bdfm',
  G: 'gtfs-g',
  J: 'gtfs-jz', Z: 'gtfs-jz',
  L: 'gtfs-l',
  N: 'gtfs-nqrw', Q: 'gtfs-nqrw', R: 'gtfs-nqrw', W: 'gtfs-nqrw',
  SI: 'gtfs-si',
};

export const feedForLine = (line: string): string | undefined => FEED_FOR_LINE[line];

export const STOPS: Record<string, Station> =
  JSON.parse(await readFile(STOPS_FILE, 'utf8'));

const DEFAULT_ROWS: RowInput[] = [
  { line: 'Q', stopId: 'R16N', label: 'Uptown' },  // Times Sq-42 St
  { line: 'C', stopId: 'A27N', label: 'Uptown' },  // 42 St-Port Authority
];

/** Throws with a message meant to be shown to whoever sent the request. */
export function validate(rows: unknown): Row[] {
  if (!Array.isArray(rows) || rows.length === 0) {
    throw new Error('rows must be a non-empty array');
  }
  if (rows.length > MAX_ROWS) {
    throw new Error(`at most ${MAX_ROWS} rows (the display fits ${MAX_ROWS})`);
  }
  return rows.map((row: Partial<RowInput>, i: number): Row => {
    const where = `rows[${i}]`;
    const line = String(row?.line ?? '').toUpperCase();
    const stopId = String(row?.stopId ?? '').toUpperCase();
    const label = String(row?.label ?? '');

    const feed = feedForLine(line);
    if (!feed) throw new Error(`${where}: unknown line "${line}"`);
    if (!/^[A-Z0-9]{3}[NS]$/.test(stopId)) {
      throw new Error(`${where}: stopId must be 3 characters plus N or S, e.g. R16N`);
    }
    const station = STOPS[stopId.slice(0, 3)];
    if (!station) throw new Error(`${where}: no station "${stopId.slice(0, 3)}"`);
    if (!station.routes.includes(line)) {
      throw new Error(
        `${where}: the ${line} does not stop at ${stopId.slice(0, 3)} `
        + `(${station.name}); it is served by ${station.routes.join(', ')}`,
      );
    }
    if (label.length > MAX_LABEL) {
      throw new Error(`${where}: label longer than ${MAX_LABEL} characters`);
    }
    return { line, stopId, label, feed, station: station.name };
  });
}

export async function load(): Promise<Row[]> {
  try {
    return validate(JSON.parse(await readFile(STATE_FILE, 'utf8')));
  } catch (err) {
    if ((err as NodeJS.ErrnoException).code !== 'ENOENT') {
      console.warn(`stations file unusable (${(err as Error).message}); using defaults`);
    }
    return validate(DEFAULT_ROWS);
  }
}

export async function save(rows: Row[]): Promise<void> {
  // Write then rename, so a crash mid-write cannot leave a truncated file.
  const tmp = `${STATE_FILE}.tmp`;
  const bare: RowInput[] = rows.map(({ line, stopId, label }) => ({ line, stopId, label }));
  await writeFile(tmp, JSON.stringify(bare, null, 2));
  await rename(tmp, STATE_FILE);
}
