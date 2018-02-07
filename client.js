#!/usr/bin/env node

let destHost;
let destPort;
let cmdLineErrors = [];

for( let argi=2; argi<process.argv.length; ++argi ) {
	let arg = process.argv[argi];
	if( destHost == undefined ) destHost = arg;
	else if( destPort == undefined ) destPort = arg;
	else {
		cmdLineErrors.push("Too many arguments ha ha what do you mean, '"+arg+"'");
	}
}

if( destHost == undefined ) {
   cmdLineErrors.push("No host specified");
}
if( destPort == undefined ) {
   cmdLineErrors.push("No port specified");
}

if( cmdLineErrors.length > 0 ) {
   for( let e in cmdLineErrors ) {
      console.error(cmdLineErrors[e]);
   }
   process.exit(1);
}

const dgram = require('dgram');
const readline = require('readline');
const server = dgram.createSocket('udp4');

server.on('listening', () => {
	let addr = server.address();
	console.log("### Listening on "+addr.address+":"+addr.port);
});

server.on('message', (msg, rinfo) => {
	console.log("### From "+rinfo.address+":"+rinfo.port+":");
	console.log(msg.toString());
});

server.bind();
const rl = readline.createInterface({
	input: process.stdin,
	output: process.stdout
});
rl.on('line', (line) => {
	server.send(line, destPort, destHost);
});
