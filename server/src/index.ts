import { fileURLToPath } from 'node:url';
import { dirname, join } from 'node:path';
import express, { type Request, type Response, type NextFunction } from 'express';
import GtfsRealtimeBindings from 'gtfs-realtime-bindings';
import {
  FEED_BASE, MAX_MINS, CACHE_MS, FETCH_TIMEOUT_MS, TIMEZONE,
  STOPS, load, save, validate, type Row,
} from './config.js';

// Deliberately not PORT: shell.nix exports that for the ESP32 serial device.
const PORT = Number(process.env.ARRIVALS_PORT ?? 8080);

/** What the ESP32 parses. Changing this means reflashing the device. */
interface Payload {
  rows: { line: string; label: string; mins: number[] }[];
  updated: number;
  updated_text: string;
}

type FeedMessage = ReturnType<
  typeof GtfsRealtimeBindings.transit_realtime.FeedMessage.decode
>;

// LAN-only service: no auth. If it ever faces the internet, that changes.
let rows: Row[] = await load();

// ---------------------------------------------------------------- feed access

async function fetchFeed(feed: string): Promise<FeedMessage> {
  const res = await fetch(FEED_BASE + encodeURIComponent(`nyct/${feed}`), {
    signal: AbortSignal.timeout(FETCH_TIMEOUT_MS),
  });
  if (!res.ok) throw new Error(`${feed}: HTTP ${res.status}`);
  const buf = new Uint8Array(await res.arrayBuffer());
  return GtfsRealtimeBindings.transit_realtime.FeedMessage.decode(buf);
}

/** Minutes from now until the next trains on `line` calling at `stopId`. */
function arrivalsFrom(
  message: FeedMessage, line: string, stopId: string, nowSec: number,
): number[] {
  const times: number[] = [];
  for (const entity of message.entity) {
    const trip = entity.tripUpdate;
    // A feed carries its whole line group, so the route filter is required:
    // the ace feed has A trains calling at A27 too.
    if (!trip || trip.trip?.routeId !== line) continue;
    for (const stop of trip.stopTimeUpdate ?? []) {
      if (stop.stopId !== stopId) continue;
      const when = Number(stop.arrival?.time ?? stop.departure?.time ?? 0);
      if (when > 0) times.push(when);
    }
  }
  return [...new Set(times)]
    .filter((t) => t > nowSec - 30)              // drop trains already gone
    .sort((a, b) => a - b)
    .slice(0, MAX_MINS)
    .map((t) => Math.max(0, Math.round((t - nowSec) / 60)));
}

function stamp(date: Date): string {
  return new Intl.DateTimeFormat('en-US', {
    timeZone: TIMEZONE, month: 'short', day: 'numeric',
    hour: 'numeric', minute: '2-digit',
  }).format(date);
}

async function buildPayload(): Promise<Payload> {
  const nowSec = Math.floor(Date.now() / 1000);

  // One fetch per distinct feed, even when several rows share it.
  const feeds = [...new Set(rows.map((r) => r.feed))];
  const decoded = new Map<string, FeedMessage>(
    await Promise.all(
      feeds.map(async (f) => [f, await fetchFeed(f)] as [string, FeedMessage]),
    ),
  );

  return {
    rows: rows.map((row) => ({
      line: row.line,
      label: row.label,
      mins: arrivalsFrom(decoded.get(row.feed)!, row.line, row.stopId, nowSec),
    })),
    updated: nowSec,
    // Preformatted here: the ESP32 has no clock and no timezone database.
    updated_text: stamp(new Date()),
  };
}

// ------------------------------------------------------------------- caching

let cache: { payload: Payload; at: number } | null = null;
let lastGood: Payload | null = null;   // survives upstream outages

async function arrivals(): Promise<Payload> {
  if (cache && Date.now() - cache.at < CACHE_MS) return cache.payload;
  try {
    const payload = await buildPayload();
    cache = { payload, at: Date.now() };
    lastGood = payload;
    return payload;
  } catch (err) {
    // Serve the last good payload rather than an error. Its updated_text stays
    // old, so the display shows stale numbers with an honestly stale timestamp.
    if (lastGood) {
      console.warn(`upstream failed (${(err as Error).message}); serving stale payload`);
      return lastGood;
    }
    throw err;
  }
}

// ---------------------------------------------------------------------- http

const publicRows = () =>
  rows.map(({ line, stopId, label, station }) => ({ line, stopId, label, station }));

const app = express();
app.disable('x-powered-by');
app.use(express.json({ limit: '8kb' }));

// Minimal access log -- the only way to see whether the display is reaching us.
app.use((req: Request, res: Response, next: NextFunction) => {
  const ip = (req.ip ?? '').replace('::ffff:', '');
  res.on('finish', () => {
    console.log(`${new Date().toISOString()} ${ip} ${req.method} ${req.path} ${res.statusCode}`);
  });
  next();
});

// The config UI. Same origin as the API, so no CORS to think about.
app.use(express.static(join(dirname(fileURLToPath(import.meta.url)), '..', 'public')));

app.get('/healthz', (_req: Request, res: Response) => { res.json({ ok: true }); });

app.get('/api/arrivals', async (_req: Request, res: Response) => {
  try {
    res.set('Cache-Control', 'no-store').json(await arrivals());
  } catch (err) {
    console.error(err);
    res.status(502).json({ error: 'upstream unavailable' });
  }
});

app.get('/api/config', (_req: Request, res: Response) => {
  res.json({ rows: publicRows() });
});

/** Replace the whole set of rows. Body: { "rows": [ { line, stopId, label } ] } */
app.put('/api/config', async (req: Request, res: Response) => {
  let next: Row[];
  try {
    next = validate(req.body?.rows);
  } catch (err) {
    res.status(400).json({ error: (err as Error).message });
    return;
  }
  rows = next;
  cache = null;          // next poll reflects the new stations immediately
  await save(rows);
  console.log('config updated:', rows.map((r) => `${r.line}@${r.stopId}`).join(' '));
  res.json({ rows: publicRows() });
});

/** Find a stopId without digging through static GTFS. */
app.get('/api/stops', (req: Request, res: Response) => {
  const q = String(req.query.q ?? '').trim().toLowerCase();
  const line = String(req.query.line ?? '').toUpperCase();
  const stops = Object.entries(STOPS)
    .filter(([id, s]) =>
      (!q || s.name.toLowerCase().includes(q) || id.toLowerCase().startsWith(q))
      && (!line || s.routes.includes(line)))
    .slice(0, 50)
    .map(([id, s]) => ({
      id, name: s.name, routes: s.routes, north: `${id}N`, south: `${id}S`,
    }));
  res.json({ stops });
});

app.listen(PORT, () => {
  console.log(`arrivals server on :${PORT}`);
  for (const r of rows) console.log(`  ${r.line} @ ${r.stopId}  ${r.station}`);
});
