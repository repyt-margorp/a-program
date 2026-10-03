'use strict';

const assert = require('node:assert/strict');
const fs = require('node:fs');
const http = require('node:http');
const os = require('node:os');
const path = require('node:path');
const { spawn } = require('node:child_process');
const WebSocket = require('ws');

async function control(mode) {
	const home = fs.mkdtempSync(path.join(os.tmpdir(), 'a-program-notify-test-'));
	const directory = path.join(home, '.codex/app-server-control');
	fs.mkdirSync(directory, { recursive: true });
	const server = http.createServer();
	const sockets = new WebSocket.WebSocketServer({ server });
	const calls = [];
	let notices = 0;
	sockets.on('connection', socket => socket.on('message', bytes => {
		const request = JSON.parse(bytes);
		calls.push(request.method);
		if (!request.id) return;
		let result = {};
		if (request.id === 2) result = { data: mode === 'absent' ? [] : ['test-core'] };
		if (request.id === 3) result = {
			data: [{ id: 'test-turn', status: mode === 'idle' ? 'completed' : 'inProgress' }]
		};
		if (request.id === 4) {
			++notices;
			assert.equal(request.method, mode === 'idle' ? 'turn/start' : 'turn/steer');
			if (mode !== 'idle') assert.equal(request.params.expectedTurnId, 'test-turn');
			if (mode === 'stale') {
				socket.send(JSON.stringify({ id: request.id, error: { message: 'active turn changed' } }));
				return;
			}
		}
		socket.send(JSON.stringify({ id: request.id, result }));
	}));
	await new Promise(resolve => server.listen(path.join(directory, 'app-server-control.sock'), resolve));
	let child;
	try {
		const relay = mode === 'relay';
		const outbox = path.join(home, 'outbox');
		const args = relay ? ['test-core', outbox] : ['test-core', '[worker-notification control/epoch] mock-only evidence'];
		child = spawn(process.execPath, [path.join(__dirname, relay ? 'watch_core.cjs' : 'notify_core.cjs'),
			...args], { env: { ...process.env, HOME: home } });
		let output = '';
		child.stdout.on('data', bytes => output += bytes);
		child.stderr.on('data', () => {});
		const exit = new Promise(resolve => child.once('close', resolve));
		if (relay) {
			while (!output.includes('Watching worker outbox:')) await new Promise(resolve => setTimeout(resolve, 10));
			const file = path.join(outbox, 'epoch-ready.txt');
			fs.writeFileSync(file, 'partial');
			fs.writeFileSync(path.join(outbox, 'oversized.txt'), 'x'.repeat(8193) + '\n');
			fs.symlinkSync(file, path.join(outbox, 'symlink.txt'));
			await new Promise(resolve => setTimeout(resolve, 100));
			assert.equal(notices, 0);
			fs.writeFileSync(file, 'ready\n');
			while (!output.includes('"acknowledged":true')) await new Promise(resolve => setTimeout(resolve, 10));
			fs.writeFileSync(file, 'ready\n');
			await new Promise(resolve => setTimeout(resolve, 100));
			assert.equal(notices, 1);
			child.kill();
			await exit;
		} else {
			assert.equal(await exit, mode === 'absent' || mode === 'stale' ? 1 : 0);
		}
		for (const forbidden of ['thread/start', 'thread/resume', 'thread/fork'])
			assert(!calls.includes(forbidden));
		if (mode === 'absent') assert(!calls.includes('thread/turns/list'));
		console.log(`${mode}: passed, ${notices} notification(s), no new/resumed Core`);
	} finally {
		if (child && child.exitCode === null) child.kill();
		for (const socket of sockets.clients) socket.terminate();
		await new Promise(resolve => sockets.close(resolve));
		await new Promise(resolve => server.close(resolve));
		fs.rmSync(home, { recursive: true });
	}
}

const deadline = setTimeout(() => {
	console.error('Notification controls timed out.');
	process.exit(1);
}, 10000);
(async () => {
	for (const mode of ['absent', 'active', 'idle', 'stale', 'relay']) await control(mode);
	clearTimeout(deadline);
})().catch(error => {
	console.error(error);
	process.exitCode = 1;
});
