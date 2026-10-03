'use strict';

const path = require('node:path');
const os = require('node:os');
const WebSocket = require('ws');
const [thread_id, message] = process.argv.slice(2);

if (!thread_id || !message || process.argv.length !== 4) {
	console.error('Usage: node notify_core.cjs THREAD_ID "[worker-notification lane/epoch] report"');
	process.exit(2);
}
if (!message.startsWith('[worker-notification ')) {
	console.error('Identify the notification as worker evidence, not a user instruction.');
	process.exit(2);
}

const socket_path = path.join(os.homedir(), '.codex/app-server-control/app-server-control.sock');
const socket = new WebSocket(`ws+unix://${socket_path}:/`);
let delivered = false;
let method;
const timeout = setTimeout(() => finish('Core notification timed out.'), 60000);

function finish(error) {
	clearTimeout(timeout);
	if (error) {
		console.error(error);
		process.exitCode = 1;
		socket.terminate();
	} else {
		delivered = true;
		console.log(JSON.stringify({ thread_id, method, acknowledged: true }));
		socket.close();
	}
}

function send(id, request_method, params) {
	socket.send(JSON.stringify({ id, method: request_method, params }));
}

socket.on('open', () => send(1, 'initialize', {
	clientInfo: { name: 'a_program_worker_notification', version: '0.1' },
	capabilities: { experimentalApi: true }
}));

socket.on('message', data => {
	const response = JSON.parse(data);
	if (response.error) {
		finish(response.error.message);
		return;
	}
	switch (response.id) {
	case 1:
		socket.send(JSON.stringify({ method: 'initialized', params: {} }));
		send(2, 'thread/loaded/list', {});
		break;
	case 2:
		// Never resume a persisted thread that may have another live owner.
		if (!response.result.data.includes(thread_id)) {
			finish('Core is not loaded on this server; no thread was resumed.');
			return;
		}
		send(3, 'thread/turns/list', {
			threadId: thread_id, limit: 1, sortDirection: 'desc', itemsView: 'notLoaded'
		});
		break;
	case 3: {
		const turn = response.result.data[0];
		const params = { threadId: thread_id, input: [{ type: 'text', text: message }] };
		method = 'turn/start';
		if (turn?.status === 'inProgress') {
			method = 'turn/steer';
			params.expectedTurnId = turn.id;
		}
		send(4, method, params);
		break;
	}
	case 4:
		finish();
		break;
	}
});

socket.on('error', error => finish(error.message));
socket.on('close', () => {
	clearTimeout(timeout);
	if (!delivered) process.exitCode = 1;
});
