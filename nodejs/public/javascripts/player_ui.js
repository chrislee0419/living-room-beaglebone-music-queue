"use strict";
/*
	player_ui.js

	Manages front-end behaviour
*/

const POLL_INTERVAL_MS = 1000;
const UPDATE_TIMEOUT = 5000;
const ERROR_DISPLAY_TIME = 2000;
const COMMAND_DELIM = ";"

var socket = io.connect();

var lastUpdateTimeNodejs = Date.now();

// Run when webpage fully loaded
$(document).ready(function() {

	// Register callback functions for each button
	$('#btn-addsong').click(function() { submitSongLink(); });

	$('#btn-playpause').click(	function() { sendPlayPause(); });
	$('#btn-skipsong').click(	function() { skipSong(); });

	$('#btn-vol-up').click(	 function() { setServerVolume(getDisplayVolume() + 5); });
	$('#btn-vol-down').click(function() { setServerVolume(getDisplayVolume() - 5); });

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
// Song Queue
//

// This 
var songQueue = [];

// Submits input link to server
function submitSongLink() {
	// Get form input

	// Clear form input

	// Send to server
}

// Adds a song to end of the list
function addSongLink(songUrl) {
	// Check Youtube link

	// Get youtube video title

	// Add to song list table HTML
}

// Finds the input URL in current list, then removes it
function removeSongLink(songUrl) {
	// Find the song url

	// If it exists, remove it and update the table HTML
}


//
// Play/Pause
//
var isPlaying = false;
function sendPlayPause() {
	if (isPlaying) {
		sendServerCommand("pause")
	}
	else {
		sendServerCommand("play")
	}
	setPlayPauseDisplay(!isPlaying);
}

function setPlayPauseDisplay(isPlayingInput) {
	isPlaying = isPlayingInput;
	
	// Change play/puase button display
	if (isPlaying) {
		$("#btn-playpause").attr("value", "Pause");
	}
	else {
		$("#btn-playpause").attr("value", "Play");
	}
}


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

	switch (primaryCommand) {
		case "play":
			setPlayPauseDisplay(true);
			break;

		case "pause":
			setPlayPauseDisplay(false);

		case "volume":
			setDisplayVolume(subCommand);
			break;

		case "nodejsping":
			lastUpdateTimeNodejs = Date.now();
			break;

		default:
			console.log("Error: Unrecognized command %s", primaryCommand);
	}
}

var errorTimeout;
function setError(errorMsg) {
	$("#error-text").html(errorMsg);
	$('#error-box').show();

	clearTimeout(errorTimeout);
	errorTimeout = window.setTimeout(function() { $('#error-box').hide(); }, ERROR_DISPLAY_TIME);
}
