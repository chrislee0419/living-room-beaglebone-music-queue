"use strict";
/*
	player_ui.js

	Manages front-end behaviour
*/

const POLL_INTERVAL_MS = 1000;
const UPDATE_TIMEOUT = 1000;
const ERROR_DISPLAY_TIME = 2000;
const COMMAND_DELIM = ";"

var socket = io.connect();

var lastUpdateTimeNodejs = Date.now();

var errorTimeout;

// Run when webpage fully loaded
$(document).ready(function() {

	// Register callback functions for each button

	$('#volumeUp').click(	function() { setServerVolume(getDisplayVolume() + 5); });
	$('#volumeDown').click(	function() { setServerVolume(getDisplayVolume() - 5); });


	// Incoming control messages
	socket.on('serverReply', function(data) {
		handleServerCommands(data);
	});
	
	// Poll server for new data
	pollServer();
});


//
// Sending commands to the server
//
function sendServerCommand(data) {
	socket.emit('clientCommand', data);
};

//
// Volume functions
//
function setServerVolume(newVolume) {
	sendServerCommand("volume " + newVolume); 
}

function getDisplayVolume() {
	return parseInt($('#volumeId').attr("value"));
}

function setDisplayVolume(newVolume) {
	$('#volumeId').attr("value", parseInt(newVolume));
}


function pollServer() {
	sendServerCommand("update");
	window.setTimeout(pollServer, POLL_INTERVAL_MS);

	if ((Date.now() - lastUpdateTimeNodejs) > UPDATE_TIMEOUT) {
		setError("No response from Node.js server. Is it running?")
	}
}


//
// Handling server commands
//

// Handles multiple commands seperated by COMMAND_DELIM
function handleServerCommands(data) {
	var commands = data.split(COMMAND_DELIM);
	for (var i in commands) {
		handleServerCommand(commands[i]);
	}
}

// Handles single command
function handleServerCommand(command) {
	var parsedWords = command.split(" ");
	if (parsedWords.length == 0) {
		return;
	}

	var primaryCommand = parsedWords[0];
	var subCommand = parsedWords[1];

	if (primaryCommand == "volume") {
		setDisplayVolume(subCommand);
	}
	else if (primaryCommand == "nodejsping") {
		lastUpdateTimeNodejs = Date.now();
	} 
	else {
		console.log("Error: Unrecognized command %s", primaryCommand);
	}
}

function setError(errorMsg) {
	$("#error-text").html(errorMsg);
	$('#error-box').show();

	clearTimeout(errorTimeout);
	errorTimeout = window.setTimeout(function() { $('#error-box').hide(); }, ERROR_DISPLAY_TIME);
}
