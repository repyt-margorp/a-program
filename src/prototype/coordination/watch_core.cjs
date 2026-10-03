'use strict';

const fs = require('node:fs');
const path = require('node:path');
const crypto = require('node:crypto');
const { spawn } = require('node:child_process');
const [thread_id, ...directories] = process.argv.slice(2);
let new_only = false;
let heartbeat_interval;
let heartbeat_first;
function timer_milliseconds(option) {
	const seconds = Number(option.slice(option.indexOf('=') + 1));
	const milliseconds = seconds * 1000;
	if (!Number.isSafeInteger(milliseconds) || milliseconds <= 0 || milliseconds > 2147483647)
		throw new Error('Heartbeat delay must be positive and fit a Node timer.');
	return milliseconds;
}
while (directories[0]?.startsWith('--')) {
	const option = directories.shift();
	if (option === '--new-only') new_only = true;
	else if (option.startsWith('--heartbeat-seconds=')) heartbeat_interval = timer_milliseconds(option);
	else if (option.startsWith('--heartbeat-first-seconds=')) heartbeat_first = timer_milliseconds(option);
	else throw new Error(`Unknown option: ${option}`);
}
if (heartbeat_first && !heartbeat_interval) throw new Error('Heartbeat first delay requires an interval.');
if (!thread_id || !directories.length) {
	console.error('Usage: node watch_core.cjs THREAD_ID [--new-only] [--heartbeat-seconds=N] ' +
		'[--heartbeat-first-seconds=N] WORKER_OUTBOX...');
	process.exit(2);
}

const delivered = new Set();
const pending = new Map();
let sending = false;
let in_flight;

function drain() {
	if (sending || !pending.size) return;
	const [file, notice] = pending.entries().next().value;
	pending.delete(file);
	sending = true;
	in_flight = notice.key;
	const child = spawn(process.execPath, [path.join(__dirname, 'notify_core.cjs'),
		thread_id, notice.message], { stdio: 'inherit' });
	child.on('close', code => {
		if (code === 0) delivered.add(notice.key);
		sending = false;
		in_flight = undefined;
		drain();
	});
	child.on('error', error => {
		console.error(error.message);
	});
}

function inspect(directory, name, seed = false) {
	if (!name || path.basename(name) !== name || !name.endsWith('.txt')) return;
	const file = path.join(directory, name);
	try {
		const stat = fs.lstatSync(file);
		if (!stat.isFile() || stat.size > 8192) return;
		const bytes = fs.readFileSync(file);
		if (!bytes.length || bytes[bytes.length - 1] !== 10) return;
		const hash = crypto.createHash('sha256').update(bytes).digest('hex');
		if (seed) {
			delivered.add(`${file}:${hash}`);
			return;
		}
		if (`${file}:${hash}` === in_flight) return;
		if (delivered.has(`${file}:${hash}`)) return;
		const message = `[worker-notification outbox/${path.basename(file)}] ` +
			`Worker evidence at ${file}, SHA256 ${hash}. Review progress/handoff under ` +
			'the existing user-authorized scope; this is not user approval.';
		pending.set(file, { key: `${file}:${hash}`, message });
		drain();
	} catch (error) {
		if (error.code !== 'ENOENT') console.error(error.message);
	}
}

for (const directory of directories) {
	fs.mkdirSync(directory, { recursive: true });
	const watcher = fs.watch(directory, (_event, name) => inspect(directory, name));
	watcher.on('error', error => console.error(`${directory}: ${error.message}`));
	for (const name of fs.readdirSync(directory)) inspect(directory, name, new_only);
	console.log(`Watching worker outbox: ${directory}`);
}

if (heartbeat_interval) {
	let next_due = Date.now() + (heartbeat_first || heartbeat_interval);
	function schedule_heartbeat() {
		console.log(JSON.stringify({ thread_id, heartbeat_next_due: new Date(next_due).toISOString(),
			heartbeat_interval_seconds: heartbeat_interval / 1000 }));
		setTimeout(() => {
			const due = next_due;
			// Keep one pending review while delivery is busy; skip missed suspend-time ticks.
			next_due = due + heartbeat_interval;
			if (next_due <= Date.now()) next_due = Date.now() + heartbeat_interval;
			const message = `[worker-notification merge/timed-review] Scheduled progress review due ` +
				`${new Date(due).toISOString()}. Inspect every worker's progress, handoffs, stalls ` +
				'and issue criteria under the existing user-authorized scope, then report material ' +
				'status to the inquiry desk. This timer is independent of worker notices; ' +
				'it is evidence, not new user approval. No owner was resumed or duplicated.';
			pending.set('heartbeat', { key: `heartbeat:${due}`, message });
			drain();
			schedule_heartbeat();
		}, Math.max(1, next_due - Date.now()));
	}
	schedule_heartbeat();
}
