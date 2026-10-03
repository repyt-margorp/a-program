'use strict';

const assert = require('node:assert/strict');
const fs = require('node:fs');
const http = require('node:http');
const os = require('node:os');
const path = require('node:path');
const { spawn } = require('node:child_process');
const WebSocket = require('ws');

async function wait_for(predicate) {
	while (!predicate()) await new Promise(resolve => setTimeout(resolve, 10));
}

async function control(mode) {
	const root = fs.mkdtempSync(path.join(os.tmpdir(), 'a-program-timer-test-'));
	const directory = path.join(root, '.codex/app-server-control');
	const outbox = path.join(root, 'outbox');
	fs.mkdirSync(directory, { recursive: true });
	fs.mkdirSync(outbox);
	fs.writeFileSync(path.join(outbox, 'old.txt'), 'previously reviewed\n');
	// Copy the exact watcher and redirect only the helper's socket to the mock.
	fs.copyFileSync(path.join(__dirname, 'watch_core.cjs'), path.join(root, 'watch_core.cjs'));
	const helper = fs.readFileSync(path.join(__dirname, 'notify_core.cjs'), 'utf8');
	const socket_line = "const socket_path = path.join(os.homedir(), '.codex/app-server-control/app-server-control.sock');";
	assert(helper.includes(socket_line));
	fs.writeFileSync(path.join(root, 'notify_core.cjs'), helper.replace(socket_line,
		`const socket_path = ${JSON.stringify(path.join(directory, 'app-server-control.sock'))};`));
	const server = http.createServer();
	const sockets = new WebSocket.WebSocketServer({ server });
	const calls = [];
	const messages = [];
	let connections = 0;
	let maximum_connections = 0;
	sockets.on('connection', socket => {
		maximum_connections = Math.max(maximum_connections, ++connections);
		socket.once('close', () => --connections);
		socket.on('message', bytes => {
			const request = JSON.parse(bytes);
			calls.push(request.method);
			if (!request.id) return;
			let result = {};
			if (request.id === 2) result = { data: mode === 'absent' ? [] : ['test-merge'] };
			if (request.id === 3) result = { data: [{ id: 'current-turn',
				status: mode === 'idle' ? 'completed' : 'inProgress' }] };
			if (request.id === 4) {
				assert.equal(request.method, mode === 'idle' ? 'turn/start' : 'turn/steer');
				if (mode !== 'idle') assert.equal(request.params.expectedTurnId, 'current-turn');
				messages.push(request.params.input[0].text);
				if (mode === 'busy') {
					setTimeout(() => {
						if (socket.readyState === WebSocket.OPEN)
							socket.send(JSON.stringify({ id: request.id, result }));
					}, 160);
					return;
				}
			}
			socket.send(JSON.stringify({ id: request.id, result }));
		});
	});
	await new Promise(resolve => server.listen(path.join(directory, 'app-server-control.sock'), resolve));
	let child;
	try {
		child = spawn(process.execPath, [path.join(root, 'watch_core.cjs'), 'test-merge',
			'--new-only', `--heartbeat-seconds=${mode === 'busy' ? 0.04 : 0.3}`,
			'--heartbeat-first-seconds=0.15', outbox]);
		let output = '';
		child.stdout.on('data', bytes => output += bytes);
		child.stderr.on('data', bytes => output += bytes);
		const exit = new Promise(resolve => child.once('close', resolve));
		await wait_for(() => output.includes('heartbeat_next_due'));
		if (mode === 'active') {
			const file = path.join(outbox, 'ready.txt');
			fs.writeFileSync(file, 'ready\n');
			await wait_for(() => messages.some(message => message.includes('outbox/ready.txt')));
			fs.writeFileSync(file, 'ready\n');
		}
		if (mode === 'absent') {
			await wait_for(() => (output.match(/Core is not loaded/g) || []).length >= 2);
			assert.equal(messages.length, 0);
			assert(!calls.includes('thread/turns/list'));
		} else {
			const expected = mode === 'active' ? 3 : 2;
			await wait_for(() => (output.match(/"acknowledged":true/g) || []).length >= expected);
			const timed = messages.filter(message => message.includes('merge/timed-review'));
			assert(timed.length >= 2, 'Timer must repeat independently of file notices.');
			assert.equal(new Set(timed).size, timed.length, 'No duplicate deadline delivery.');
			if (mode === 'active') assert.equal(messages.filter(message => message.includes('outbox/')).length, 1);
			assert(!messages.some(message => message.includes('outbox/old.txt')));
		}
		assert.equal(maximum_connections, 1, 'One notification helper may deliver at a time.');
		for (const forbidden of ['thread/start', 'thread/resume', 'thread/fork'])
			assert(!calls.includes(forbidden));
		child.kill();
		await exit;
		console.log(`timer-${mode}: passed; guarded periodic delivery, no duplicate owner`);
	} finally {
		if (child && child.exitCode === null) child.kill();
		for (const socket of sockets.clients) socket.terminate();
		await new Promise(resolve => sockets.close(resolve));
		await new Promise(resolve => server.close(resolve));
		fs.rmSync(root, { recursive: true });
	}
}

const deadline = setTimeout(() => {
	console.error('Timer controls timed out.');
	process.exit(1);
}, 10000);
(async () => {
	for (const mode of ['active', 'idle', 'absent', 'busy']) await control(mode);
	clearTimeout(deadline);
})().catch(error => {
	console.error(error);
	process.exitCode = 1;
});
