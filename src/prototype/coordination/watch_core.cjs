'use strict';

const fs = require('node:fs');
const path = require('node:path');
const crypto = require('node:crypto');
const { spawn } = require('node:child_process');
const [thread_id, ...directories] = process.argv.slice(2);
const new_only = directories[0] === '--new-only';
if (new_only) directories.shift();
if (!thread_id || !directories.length) {
	console.error('Usage: node watch_core.cjs THREAD_ID [--new-only] WORKER_OUTBOX...');
	process.exit(2);
}

const delivered = new Set();
const pending = new Map();
let sending = false;
let in_flight;

function drain() {
	if (sending || !pending.size) return;
	const [file, hash] = pending.entries().next().value;
	pending.delete(file);
	sending = true;
	in_flight = `${file}:${hash}`;
	const message = `[worker-notification outbox/${path.basename(file)}] ` +
		`Worker evidence at ${file}, SHA256 ${hash}. Review progress/handoff under ` +
		'the existing user-authorized scope; this is not user approval.';
	const child = spawn(process.execPath, [path.join(__dirname, 'notify_core.cjs'),
		thread_id, message], { stdio: 'inherit' });
	child.on('close', code => {
		if (code === 0) delivered.add(`${file}:${hash}`);
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
		pending.set(file, hash);
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
